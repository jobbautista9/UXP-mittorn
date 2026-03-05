/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prenv.h"
#include "nsWindow.h"
#include "nsAppShell.h"

#include "nsWidgetsCID.h"

#include "gfxUtils.h"
#include "gfxContext.h"
#include "nsRegion.h"
#include "Layers.h"
#include "nsTArray.h"

#include "mozilla/BasicEvents.h"
#include "nsSDLKeymap.h"
#include "nsSDLCursors.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/TextEventDispatcher.h"
#include "mozilla/Preferences.h"

#include "GeckoProfiler.h"
#include "ClientLayerManager.h"
#include "nsDragService.h"
#include "BasicLayers.h"
#include "mozilla/layers/APZEventState.h"
#include "mozilla/layers/APZThreadUtils.h"
#include "mozilla/layers/IAPZCTreeManager.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
//static nsTArray<nsWindow*> gTopLevelWindows;
#define ALOG(args...) fprintf(stderr, args); fprintf(stderr, "\n")


NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, Inherited)

nsWindow::nsWindow()
  : mNativeWindow(nullptr)
  , mVisible(false)
  , mScheduleResize(false)
  , mRotation(ROTATION_0)
  , mParent(nullptr)
  , mDrawableSize(0,0)
  , mHandleTouchEvent(false)
{
}

nsWindow::~nsWindow()
{
  if (mNativeWindow) {
    if (nsAppShell::gAppShell->mSeparateEventThread) {
      SDL_Window *win = mNativeWindow;
      nsAppShell::gAppShell->mSDLEventsThread->Dispatch(
        NS_NewRunnableFunction([win]{
          SDL_DestroyWindow(win);
        }).take(), NS_DISPATCH_NORMAL);
    } else {
      MutexAutoLock lock(nsAppShell::gAppShell->mEventsMutex);
      SDL_DestroyWindow(mNativeWindow);
    }
  }
  mNativeWindow = nullptr;
}

// SDL does not support child widgets, so make everything toplevel except child widgets
bool
nsWindow::IsTopLevel()
{
  return mWindowType == eWindowType_toplevel ||
         mWindowType == eWindowType_dialog ||
         mWindowType == eWindowType_invisible ||
         mWindowType == eWindowType_popup;
}
#include <SDL_syswm.h>

//
// nsIWidget
//
static void *sX11Display;
nsresult
nsWindow::Create(nsIWidget* aParent,
                 nsNativeWidget aNativeParent,
                 const LayoutDeviceIntRect& aRect,
                 nsWidgetInitData* aInitData)
{
  ALOG("nsWindow[%p]::Create %p/%p [%d %d %d %d]",
       (void *)this, (void *)aParent, (void *)aNativeParent, aRect.x, aRect.y, aRect.width, aRect.height);
  nsWindow *parent = (nsWindow *)aParent;
  mBounds = aRect;
  // Set defaults which can be overriden from aInitData in BaseCreate
  mWindowType = eWindowType_toplevel;
  mBorderStyle = eBorderStyle_default;
  if (parent) {
    mWindowType = eWindowType_child;
  }
  Inherited::BaseCreate(aParent, aInitData);

  // only create window for toplevel widget
  if (IsTopLevel()) {
    nsAppShell::gAppShell->EnsureInit();
    nsAppShell::gAppShell->mTopLevelWindows.AppendElement(this);

    // spawn window as invisible
    Uint32 flags = SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL;

    // calc window flags based on class, styles
    nsBorderStyle style = aInitData->mBorderStyle;
    if (style != eBorderStyle_default) {
      flags &= ~SDL_WINDOW_RESIZABLE;
      if (style & (eBorderStyle_all | eBorderStyle_resizeh))
        flags |= SDL_WINDOW_RESIZABLE;
      else if (!(style & eBorderStyle_border) || !(style & eBorderStyle_title))
        flags |= SDL_WINDOW_BORDERLESS;
    }
    if (mWindowType == eWindowType_popup) {
      flags |= SDL_WINDOW_POPUP_MENU;
    }

    // motor
    nsAppShell::DispatchToSDLThread([this, aRect, &flags, &parent, aNativeParent] {

      mNativeWindow = SDL_CreateWindow("UXP", aRect.x, aRect.y, aRect.width, aRect.height, flags);
      if (!mNativeWindow) { // if it's failed, reset opengl flag and retry
        flags &= ~SDL_WINDOW_OPENGL;
        mNativeWindow = SDL_CreateWindow("UXP", aRect.x, aRect.y, aRect.width, aRect.height, flags);
      }
      SDL_SetWindowData(mNativeWindow, "nsWindow", this);
      // set parent from native parent
      if (!parent && aNativeParent) {
        parent = (nsWindow *)SDL_GetWindowData((SDL_Window *)aNativeParent, "nsWindow");
      }
      if (parent) {
        SDL_Window *nativeParent = (SDL_Window *)GetNativeData(NS_NATIVE_WIDGET);
        if (nativeParent) {
          SDL_SetWindowModalFor(mNativeWindow, nativeParent);
        }
      }

      // initialize platform window for known platforms (mostly for EGL)
      SDL_SysWMinfo wmInfo;
      SDL_VERSION(&wmInfo.version);
      if (SDL_GetWindowWMInfo(mNativeWindow, &wmInfo)) {
        switch(wmInfo.subsystem) {
          case SDL_SYSWM_X11:
#ifdef SDL_VIDEO_DRIVER_X11
            if (wmInfo.info.x11.display) {
              nsAppShell::gAppShell->mNativePlatform = 0x31D5; //  EGL_PLATFORM_X11_KHR
              nsAppShell::gAppShell->mPlatformDisplay = wmInfo.info.x11.display;
            }
            mPlatformWindow = (void*)wmInfo.info.x11.window;
#endif
            break;
          case SDL_SYSWM_WAYLAND:
#ifdef SDL_VIDEO_DRIVER_WAYLAND
            if (wmInfo.info.wl.display) {
              nsAppShell::gAppShell->mNativePlatform = 0x31D8; //  EGL_PLATFORM_WAYLAND_EXT
              nsAppShell::gAppShell->mPlatformDisplay = wmInfo.info.wl.display;
            }
            mPlatformWindow = (void*)wmInfo.info.wl.egl_window;
#endif
          case SDL_SYSWM_KMSDRM:
#ifdef SDL_VIDEO_DRIVER_KMSDRM
            if (wmInfo.info.kmsdrm.gbm_dev) {
              nsAppShell::gAppShell->mNativePlatform = 0x31D7; //  EGL_PLATFORM_GBM_KHR
              nsAppShell::gAppShell->mPlatformDisplay = wmInfo.info.kmsdrm.gbm_dev;
            }
            // it's impossible to use EGL on SDL-KMSDRM directly now
            // SDL does not export gbm surface, so we need create and update it manually
            // fallback to pbuffer for now if --with-gl-provider=SDL not enabled
            mPlatformWindow = nullptr;
#endif
            break;
          default:
            break;
        }
      }

    });
    MOZ_ASSERT(mNativeWindow);
  }



  if (parent) {
    parent->mChildren.AppendElement(this);
    mParent = parent;
  }

  NS_ASSERTION(IsTopLevel() || parent, "non top level window doesn't have a parent!");

  return NS_OK;
}

void
nsWindow::Destroy()
{
  for (uint32_t i = 0; i < mChildren.Length(); ++i) {
    // why do we still have children?
    mChildren[i]->SetParent(nullptr);
  }

  if (mParent) {
    mParent->mChildren.RemoveElement(this);
  }

  if (IsTopLevel())
    nsAppShell::gAppShell->mTopLevelWindows.RemoveElement(this);

  SDL_Window *win = mNativeWindow;
  mNativeWindow = nullptr;
  if (win) {
    // prevent drawing while destroying
    MutexAutoLock lock(nsAppShell::gAppShell->mEventsMutex);
    /** Need to clean our LayerManager up while still alive */
    if (mLayerManager) {
        mLayerManager->Destroy();
    }
    mLayerManager = nullptr;

    DestroyCompositor();
    NotifyWindowDestroyed();
    if (nsAppShell::gAppShell->mSeparateEventThread) {
      nsAppShell::gAppShell->mSDLEventsThread->Dispatch(
        NS_NewRunnableFunction([win]{
          SDL_DestroyWindow(win);
        }).take(), NS_DISPATCH_NORMAL);
    } else {
      SDL_DestroyWindow(win);
    }
  }

  nsBaseWidget::Destroy();
  nsBaseWidget::OnDestroy();
}

NS_IMETHODIMP
nsWindow::ConfigureChildren(const nsTArray<nsIWidget::Configuration>& config)
{
  for (uint32_t i = 0; i < config.Length(); ++i) {
    nsWindow *childWin = (nsWindow *)config[i].mChild.get();
    childWin->Resize(config[i].mBounds.x, config[i].mBounds.y, config[i].mBounds.width, config[i].mBounds.height, false);
  }

  return NS_OK;
}


NS_IMETHODIMP
nsWindow::Show(bool aState)
{
  if (mVisible == aState) {
    return NS_OK;
  }
  mVisible = aState;
  if (!mNativeWindow) {
    return NS_OK;
  }
  if (aState) {
    SDL_ShowWindow(mNativeWindow);
    ScheduleResize();
  } else {
    SDL_HideWindow(mNativeWindow);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindow::Move(double aX, double aY)
{
  mBounds.x = aX;
  mBounds.y = aY;
  if (mNativeWindow) {
    SDL_SetWindowPosition(mNativeWindow, aX, aY);
  }

  ReportMoveEvent();
  return NS_OK;
}

NS_IMETHODIMP
nsWindow::Resize(double aX, double aY,
                 double aWidth, double aHeight,
                 bool aRepaint)
{
  bool isMoving = (mBounds.x != aX || mBounds.y != aY);
  bool isResizing = (mBounds.width != aWidth || mBounds.height != aHeight);
  if ((!isMoving && !isResizing)) {
    return NS_OK;
  }

  if (isMoving) {
    mBounds.x = aX;
    mBounds.y = aY;

    if (mNativeWindow) {
      SDL_SetWindowPosition(mNativeWindow, aX, aY);
    }
  }
  if (isResizing) {
    mBounds.width = aWidth;
    mBounds.height = aHeight;

    if (mNativeWindow) {
      ScheduleResize();
    }
  }

  if (isMoving) {
    ReportMoveEvent();
  }

  if (isResizing) {
    ReportSizeEvent();
  }

  return NS_OK;
}

NS_IMETHODIMP nsWindow::Resize(double aWidth, double aHeight, bool aRepaint)
{
  if ((mBounds.width == aWidth && mBounds.height == aHeight)) {
    return NS_OK;
  }

  mBounds.width = aWidth;
  mBounds.height = aHeight;
  if (mNativeWindow) {
    ScheduleResize();
  }

  ReportSizeEvent();

  return NS_OK;
}

void
nsWindow::SetSizeMode(nsSizeMode aMode)
{
  if (aMode == static_cast<int32_t>(mSizeMode)) {
    return;
  }

  mSizeMode = static_cast<nsSizeMode>(aMode);
  if (aMode == nsSizeMode_Maximized || aMode == nsSizeMode_Fullscreen) {
    // Resize to fill screen
    // nsBaseWidget::InfallibleMakeFullScreen(true);
  }
  ReportSizeModeEvent(aMode);
}

/*
 * resize is very slow on SDL/X11, so accumulate resize request
 * to call actual resize once */
void nsWindow::ScheduleResize()
{
  if (nsAppShell::GetForcedFullscreen() > 0) {
    if (!mDrawableSize.width && mVisible) {
      SDL_SetWindowFullscreen(mNativeWindow, SDL_WINDOW_FULLSCREEN_DESKTOP);
      SDL_PumpEvents();
      SDL_GetWindowSizeInPixels(mNativeWindow, &mDrawableSize.width, &mDrawableSize.height);

      // fake fullscreen requested, revert back to windowed
      if (nsAppShell::GetForcedFullscreen() == 2) {
        SDL_SetWindowFullscreen(mNativeWindow, 0);
        SDL_SetWindowSize(mNativeWindow, mDrawableSize.width, mDrawableSize.height);
      }
      RefPtr<nsIScreen> scr = GetWidgetScreen();
      uint32_t id, rot;
      scr->GetId(&id);
      scr->GetRotation(&rot);
      ScreenRotationChanged(id, rot);
    }
    return;
  }
  printf("ScheduleResize %d %d\n",mBounds.width, mBounds.height);
  mScheduleResize = true;
  for (uint32_t i = 0; i < nsAppShell::gAppShell->mInvalidWindows.Length(); ++i) {
    if(nsAppShell::gAppShell->mInvalidWindows[i].window == this) {
      return;
    }
  }
  nsAppShell::gAppShell->mInvalidWindows.AppendElement(InvalidWindow{LayoutDeviceIntRegion(), this});
}

NS_IMETHODIMP
nsWindow::Invalidate(const LayoutDeviceIntRect& aRect)
{
  if (!mVisible)
    return NS_OK;

  if (mNativeWindow) {
    for (uint32_t i = 0; i < nsAppShell::gAppShell->mInvalidWindows.Length();  ++i) {
      if (nsAppShell::gAppShell->mInvalidWindows[i].window == this) {
        nsAppShell::gAppShell->mInvalidWindows[i].region.OrWith(aRect);
        return NS_OK;
      }
    }
    nsAppShell::gAppShell->mInvalidWindows.AppendElement(InvalidWindow{LayoutDeviceIntRegion(aRect), this});

  }

  else if (mParent)
    return mParent->Invalidate(aRect);
  return NS_OK;
}

NS_IMETHODIMP nsWindow::MakeFullScreen(bool aFullScreen, nsIScreen* aScreen)
{
  if (mNativeWindow) {
    SDL_SetWindowFullscreen(mNativeWindow, aFullScreen ? SDL_WINDOW_FULLSCREEN_DESKTOP : 0);
  }
  SetSizeMode(aFullScreen ? nsSizeMode_Fullscreen : nsSizeMode_Normal);
  // without this browser stops updating window. Why?
  return NS_ERROR_NOT_AVAILABLE;
}

NS_IMETHODIMP
nsWindow::SetFocus(bool aRaise)
{
  if (mNativeWindow && aRaise) {
    SDL_RaiseWindow(mNativeWindow);
  }
  return NS_OK;
}


void nsWindow::ReportMoveEvent()
{
    NotifyWindowMoved(mBounds.x, mBounds.y);
}

void nsWindow::ReportSizeModeEvent(nsSizeMode aMode)
{
  if (mWidgetListener) {
    // This is terrible.
    nsSizeMode theMode;
    switch (aMode) {
    case nsSizeMode_Maximized:
      theMode = nsSizeMode_Maximized;
      break;
    case nsSizeMode_Fullscreen:
      theMode = nsSizeMode_Fullscreen;
      break;
    default:
      return;
    }
    mWidgetListener->SizeModeChanged(theMode);
  }
}


LayoutDeviceIntRect nsWindow::GetNaturalBounds() {
/* TODO: basic compositor's BeginFrame is still relies on nsIWidget::GetClientSize (probably a bug?),
  so this only works in OpenGL for now */
   if (mDrawableSize.width) {
     return {0, 0, mDrawableSize.width, mDrawableSize.height};
   }
   return mBounds;
}

void nsWindow::ReportSizeEvent()
{
  LayoutDeviceIntRect innerBounds = GetClientBounds();
  if (mWidgetListener) {
    mWidgetListener->WindowResized(this, innerBounds.width, innerBounds.height);
  }
  if (mAttachedWidgetListener) {
    mAttachedWidgetListener->WindowResized(this, innerBounds.width, innerBounds.height);
  }
}

LayoutDeviceIntRect
nsWindow::GetScreenBounds()
{
  return LayoutDeviceIntRect(WidgetToScreenOffset(), mBounds.Size());
}

LayoutDeviceIntPoint nsWindow::WidgetToScreenOffset()
{
  LayoutDeviceIntPoint offset = mBounds.TopLeft();
  if (mNativeWindow) {
    return offset;
  }

  if (mParent) {
     return mParent->WidgetToScreenOffset();
  }

  // toplevel without native window?
  return offset;
}

NS_IMETHODIMP
nsWindow::DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                        nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;
  nsCOMPtr<nsIWidget> kungFuDeathGrip(aEvent->mWidget);

  nsIWidgetListener* listener = mAttachedWidgetListener ? mAttachedWidgetListener : mWidgetListener;
  if (listener) {
    aStatus = listener->HandleEvent(aEvent, mUseAttachedEvents);
  }

  return NS_OK;
}

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
  mInputContext = aContext;
  if (aAction.mFocusChange == InputContextAction::LOST_FOCUS) {
    SDL_StopTextInput();
  } else {
    SDL_StartTextInput();
  }
}

NS_IMETHODIMP_(mozilla::widget::InputContext)
nsWindow::GetInputContext()
{
    return mInputContext;
}

#define NS_WINDOW_TITLE_MAX_LENGTH 4095
NS_IMETHODIMP nsWindow::SetTitle(const nsAString& aTitle)
{
  if (!mNativeWindow) {
    return NS_OK;
  }

  // just in case, same as GTK
#define UTF8_FOLLOWBYTE(ch) (((ch) & 0xC0) == 0x80)
  NS_ConvertUTF16toUTF8 titleUTF8(aTitle);
  if (titleUTF8.Length() > NS_WINDOW_TITLE_MAX_LENGTH) {
    // Truncate overlong titles (bug 167315). Make sure we chop after a
    // complete sequence by making sure the next char isn't a follow-byte.
    uint32_t len = NS_WINDOW_TITLE_MAX_LENGTH;
    while (UTF8_FOLLOWBYTE(titleUTF8[len])) {
      --len;
    }
    titleUTF8.Truncate(len);
  }
  SDL_SetWindowTitle(mNativeWindow, (const char *)titleUTF8.get());

  return NS_OK;
}


/**************************************************************
 *
 * SECTION: nsIWidget::SetCursor
 *
 * SetCursor and related utilities for manging cursor state.
 *
 **************************************************************/
// Set this component cursor
NS_IMETHODIMP nsWindow::SetCursor(nsCursor aCursor)
{
  if (!mNativeWindow && mParent) {
    return mParent->SetCursor(aCursor);
  }

  SDL_Cursor *newCursor = GetSDLCursor(aCursor);
  if (newCursor) {
    SDL_SetCursor(newCursor);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetCursor(imgIContainer* aCursor,
                    uint32_t aHotspotX, uint32_t aHotspotY)
{
  static SDL_Surface *sCursorSurface;
  static SDL_Cursor *sCursor;
  SDL_Cursor *oldCursor = sCursor;
  SDL_Surface *oldSurface = sCursorSurface;
  SDL_Surface *newSurface = ImageToSurface(aCursor);
  if (!newSurface) {
    return NS_ERROR_FAILURE;
  }

  SDL_Cursor *newCursor = SDL_CreateColorCursor(newSurface, aHotspotX, aHotspotY);
  if (!newCursor) {
    SDL_FreeSurface(newSurface);
    return NS_ERROR_FAILURE;
  }
  SDL_SetCursor(newCursor);
  sCursorSurface = newSurface;
  sCursor = newCursor;
  if (oldCursor) {
    SDL_FreeCursor(oldCursor);
  }
  if (oldSurface) {
    SDL_FreeSurface(oldSurface);
  }

  return NS_OK;
}


void* nsWindow::GetNativeData(uint32_t aDataType)
{
  switch (aDataType) {
  case NS_NATIVE_DISPLAY:
    return nsAppShell::gAppShell->mPlatformDisplay;
  case NS_NATIVE_WIDGET:
    // Use SDL_Window as widget to handle parent widgets
    return mNativeWindow;
  case NS_NATIVE_WINDOW:
    return mPlatformWindow;
  case NS_RAW_NATIVE_IME_CONTEXT:
    return NS_ONLY_ONE_NATIVE_IME_CONTEXT;
  case NS_NATIVE_EGL_PLATFORM:
    return (void*)(uintptr_t)nsAppShell::gAppShell->mNativePlatform;
  }
  return nullptr;
}

LayoutDeviceIntPoint nsWindow::TransformCoord(int x, int y) {
  const nsWindow *fallback = mParent? mParent: this;
  const LayoutDeviceIntSize &bounds = mNativeWindow? mDrawableSize : fallback->mDrawableSize;
  ScreenRotation rot = (ScreenRotation)(mNativeWindow? mRotation: fallback->mRotation);
  switch(rot) {
     case mozilla::ROTATION_0:
        return {nscoord(x), nscoord(y)};
     case mozilla::ROTATION_90:
        return {nscoord(y), nscoord(bounds.width - x)};
     case mozilla::ROTATION_180:
        return {nscoord(bounds.width - x), nscoord(bounds.height - y)};
     case mozilla::ROTATION_270:
        return {nscoord(bounds.height - y), nscoord(x)};
  }
  return {};
}

void nsWindow::HandleResize(int aW, int aH) {
  int w, h;

  if (mRotation == mozilla::ROTATION_90 || mRotation == mozilla::ROTATION_270) {
    w = aH, h = aW;
  } else {
    w = aW, h = aH;
  }
  mBounds.width = w, mBounds.height = h;

  for (uint32_t i = 0; i < mChildren.Length(); ++i) {
      mChildren[i]->mBounds.width = w;
      mChildren[i]->mBounds.height = h;
      mChildren[i]->ReportSizeEvent();
  }
  ReportSizeEvent();

  if (!mLayerManager) {
     return;
  }
  MutexAutoLock lock(nsAppShell::gAppShell->mEventsMutex);
  if (ClientLayerManager *mgr = mLayerManager->AsClientLayerManager()) {
    mgr->SetDefaultTargetConfiguration(BufferMode::BUFFER_NONE, (ScreenRotation)mRotation);
  }
}

static int GetClientRotation(int aDisplayIndex) {
  const char *env = PR_GetEnv("MOZ_SDL_FORCE_ROTATION");
  if (env) {
    return atoi(env);
  } else {
    char str[256] = "";
    snprintf(str, 255, "widget.sdl.displayOverrides.%d.rotation", aDisplayIndex);
    return mozilla::Preferences::GetInt(str, 0);
  }
}

void nsWindow::ScreenRotationChanged(int aScreenIndex, uint32_t aRotation) {
  printf("screen rotation %d %d\n", aScreenIndex, aRotation);
  uint32_t rotation = GetClientRotation(aScreenIndex);
  if (rotation < 4) {
    if (mRotation == rotation) {
      return;
    }
    mRotation = rotation;
  } else {
    // guess
    bool swapAxis = mDrawableSize.height > mDrawableSize.width;
    mRotation = swapAxis? (aRotation + 3) % 4: aRotation;
  }

  HandleResize(mDrawableSize.width, mDrawableSize.height);
  Invalidate(mBounds);
}

/* track mouse state while dragging outside window
 * this needed to simulate dragging tabs out of browser window */
static nsWindow *sDragWindow;

void nsWindow::HandleInvalidWindows()
{
  nsTArray<InvalidWindow> &windows = nsAppShell::gAppShell->mInvalidWindows;
  for (uint32_t i = 0; i < windows.Length(); ++i) {
    nsWindow *w = windows[i].window;

    // first, apply actual size
    if (w->mScheduleResize) {
      MutexAutoLock lock(nsAppShell::gAppShell->mEventsMutex);
      SDL_SetWindowSize(w->mNativeWindow, w->mBounds.width, w->mBounds.height);
      // ensure bounds before drawing
      SDL_GetWindowSize(w->mNativeWindow, &w->mBounds.width, &w->mBounds.height);
    }
    w->mScheduleResize = false;

    w->ExposeRegion(windows[i].region);
  }

  windows.Clear();

  if (!sDragWindow) {
    return;
  }

  WidgetMouseEvent event(true, eMouseUp, sDragWindow, WidgetMouseEvent::eReal);
  int x, y;
  Uint32 buttons = SDL_GetGlobalMouseState(&x, &y);

  if (!buttons) {
    x -= sDragWindow->mBounds.x;
    y -= sDragWindow->mBounds.y;

    event.mRefPoint.x = nscoord(x);
    event.mRefPoint.y = nscoord(y);

    nsDragService::HandleMouseEvent(sDragWindow, event, x, y);
    sDragWindow = NULL;
  }
}

/* this implements surface locking/unlocking for software drawing
 * Might be called from compositor thread while OMTC not accelerated
 * or from ExposeRegion */
already_AddRefed<mozilla::gfx::DrawTarget> nsWindow::StartRemoteDrawing()
{
  // prevent resizing in other thead while surface locked
  nsAppShell::gAppShell->mEventsMutex.Lock();
  if (!mNativeWindow) {
    nsAppShell::gAppShell->mEventsMutex.Unlock();
    return nullptr;
  }
  mSurface = SDL_GetWindowSurface(mNativeWindow);

  NS_ASSERTION(mSurface, SDL_GetError());
  if (!mSurface) {
    nsAppShell::gAppShell->mEventsMutex.Unlock();
    return nullptr;
  }
  SDL_LockSurface(mSurface);
  return gfxPlatform::CreateDrawTargetForData(
    (unsigned char *)mSurface->pixels,
    gfx::IntSize(mSurface->w, mSurface->h),
    mSurface->pitch,
    gfx::SurfaceFormat::A8R8G8B8_UINT32);
}

void nsWindow::EndRemoteDrawing()
{
  SDL_UnlockSurface(mSurface);
  SDL_UpdateWindowSurface(mNativeWindow);
  nsAppShell::gAppShell->mEventsMutex.Unlock();
}

void nsWindow::EndRemoteDrawingInRegion(mozilla::gfx::DrawTarget* aDrawTarget,
                                        LayoutDeviceIntRegion& aInvalidRegion)
{
  SDL_UnlockSurface(mSurface);
  uint32_t numRects = aInvalidRegion.GetNumRects();
  AutoTArray<SDL_Rect, 32> rects;
  rects.SetCapacity(aInvalidRegion.GetNumRects());

  for (auto iter = aInvalidRegion.RectIter(); !iter.Done(); iter.Next()) {
    const LayoutDeviceIntRect &r = iter.Get();
    SDL_Rect rect = {(short)r.x, (short)r.y, (unsigned short)r.width, (unsigned short)r.height};
    rects.AppendElement(rect);
  }

  SDL_UpdateWindowSurfaceRects(mNativeWindow, rects.Elements(), rects.Length());
  nsAppShell::gAppShell->mEventsMutex.Unlock();
}

/* handle window update */
void nsWindow::ExposeRegion(LayoutDeviceIntRegion region)
{
  if (!mLayerManager || !mWidgetListener || !mNativeWindow || !mVisible) {
    return;
  }
  ClientLayerManager *clientLayerManager = GetLayerManager()->AsClientLayerManager();
  nsIWidgetListener* listener = mAttachedWidgetListener ? mAttachedWidgetListener : mWidgetListener;

  if (clientLayerManager) {
    // we are in compositor client
    // just commit region to OMTC
    clientLayerManager->SetNeedsComposite(true);
    clientLayerManager->SendInvalidRegion(region.ToUnknownRegion());
    listener->WillPaintWindow(this);
    listener->PaintWindow(this, region);
    listener->DidPaintWindow();

    clientLayerManager->Composite();
    clientLayerManager->SetNeedsComposite(false);
  } else if (GetLayerManager()->GetBackendType() == LayersBackend::LAYERS_BASIC) {
    // basic unaccelerated layer
    // Lock surface and trigger update
    BufferMode bm = BufferMode::BUFFERED;
    RefPtr<DrawTarget> dt = StartRemoteDrawingInRegion(region, &bm);
    if (!dt) {
      return;
    }

    // while surface locked, bounds should not change!
    LayoutDeviceIntRect bounds = {0, 0, mBounds.width, mBounds.height};
    region.AndWith(bounds);
    gfxUtils::ClipToRegion(dt, region.ToUnknownRegion());
    RefPtr<gfxContext> ctx = gfxContext::CreatePreservingTransformOrNull(dt);
    AutoLayerManagerSetup setupLayerManager(this, ctx, bm);
    listener->WillPaintWindow(this);
    listener->PaintWindow(this, region);
    listener->DidPaintWindow();

    // all uncomposited child widgets expose events are routed to toplevel,
    // handle it here
    for (uint32_t i = 0; i < mChildren.Length(); ++i) {
      if (!mChildren[i]->mWidgetListener || !mChildren[i]->mVisible || mChildren[i]->mNativeWindow) {
        continue;
      }
      mChildren[i]->mWidgetListener->WillPaintWindow(mChildren[i]);
      mChildren[i]->mWidgetListener->PaintWindow(mChildren[i], region);
      mChildren[i]->mWidgetListener->DidPaintWindow();
    }

    EndRemoteDrawingInRegion(dt, region);
  } else {
    // unreachable
    MOZ_CRASH("Unknown layer manager");
  }
}

void
nsWindow::DispatchEventToAPZOnly(mozilla::WidgetInputEvent* aEvent)
{
  if (!nsAppShell::gAppShell->mSeparateEventThread) {
    nsBaseWidget::DispatchEventToAPZOnly(aEvent);
  }
}


void
nsWindow::ConfigureAPZControllerThread()
{
  if (nsAppShell::gAppShell->mSeparateEventThread) {
    APZThreadUtils::SetControllerThread(nsAppShell::gAppShell->mSDLEventsLoop);
  } else {
    nsBaseWidget::ConfigureAPZControllerThread();
  }
}

nsEventStatus nsWindow::DispatchInputEvent(WidgetInputEvent* aEvent) {
  if (nsAppShell::gAppShell->mSeparateEventThread) {
    nsEventStatus ret;
    DispatchEvent(aEvent, ret);
    return ret;
  }
  return nsBaseWidget::DispatchInputEvent(aEvent);
}

bool nsWindow::OnEvent(const SDL_Event *ev)
{
  static struct {
    bool mouseDown, keyPressRepeat;
    uint8_t keyPressLocation, keyPressIndex;
    mozilla::Modifiers modifiers;
  } sInputState;
  bool dropFree = false;
  bool textEditFree = false;

  switch (ev->type) {
  case SDL_KEYDOWN:
  case SDL_KEYUP: {
    WidgetKeyboardEvent event(true, ev->type == SDL_KEYDOWN ? eKeyDown : eKeyUp, this);
    event.mTime = ev->key.timestamp;
    uint sym = ev->key.keysym.sym;
    int keyIndex = MapKey(sym);
    // some keys not detcted
    if (keyIndex == ArrayLength(nsKeycodes) - 1) {
      keyIndex = MapKey(SDL_SCANCODE_TO_KEYCODE(ev->key.keysym.scancode));
    }
    const nsKeyConverter &key = nsKeycodes[keyIndex];
    uint keyCode = key.vkCode;
    event.mKeyCode = keyCode;
    event.mKeyNameIndex = key.keyNameIndex;
    event.mCodeNameIndex = key.codeNameIndex;
    sInputState.modifiers = ConvertModifiers(ev->key.keysym.mod);

    // handle printable syms
    if (sym <= 0x7f) {
      HandleCharSyms(event, key, sym);
    } else if(event.mCodeNameIndex == mozilla::CODE_NAME_INDEX_NumpadEnter) {
      event.SetCharCode('\r');
    } else if (!(sInputState.modifiers & MODIFIER_NUMLOCK)) {
      HandleNumpad(event, sym);
    }
    SetLatinKeys(event, key, !!(sInputState.modifiers & MODIFIER_SHIFT),
                 !!(sInputState.modifiers & MODIFIER_CAPSLOCK));
    SetKeyLocation(event, sym, ev->key.keysym.scancode);
    event.mIsRepeat = ev->key.repeat;
    event.mModifiers = sInputState.modifiers;

    if (ev->type == SDL_KEYDOWN) {
        // commit any pending composition
        // without this libeditor blows up
        nsEventStatus status = nsEventStatus_eIgnore;
        RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher();
        nsresult rv = dispatcher->BeginNativeInputTransaction();
        if (!dispatcher->DispatchKeyboardEvent(eKeyDown, event, status) || status != nsEventStatus_eConsumeNoDefault) {
          DispatchInputEvent(&event);
        }
        if (dispatcher->IsComposing()) {
          dispatcher->CommitComposition(status, nullptr, &event);
        }
    } else {
      DispatchInputEvent(&event);
    }

    // prepare keyPress event
    if (ev->type == SDL_KEYDOWN) {
      if (IsContextMenuKey(event)) {
        WidgetMouseEvent contextMenuEvent(true, eContextMenu, this,
                                          WidgetMouseEvent::eReal,
                                          WidgetMouseEvent::eContextMenuKey);
        contextMenuEvent.mTime = event.mTime;
        contextMenuEvent.mClickCount = 1;
        contextMenuEvent.mModifiers = sInputState.modifiers;
        DispatchInputEvent(&contextMenuEvent);
      }
      event.mMessage = eKeyPress;

      // only set char for hotkeys, otherwise handled by IME
      if (sInputState.modifiers & (MODIFIER_ALT | MODIFIER_CONTROL | MODIFIER_META | MODIFIER_OS) && event.mIsChar) {
        event.SetCharCode(ev->key.keysym.sym);
      }
      // forward to textinput handler
      if (event.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING && !event.mCharCode) {
        sInputState.keyPressIndex = keyIndex;
        sInputState.keyPressLocation = event.mLocation;
        sInputState.keyPressRepeat = event.mIsRepeat;
      } else {
        sInputState.keyPressIndex = nsInvalidKeycodeIndex;
        DispatchInputEvent(&event);
      }
    }

  } break;
  case SDL_TEXTINPUT:
    if (mWidgetListener) {
      NS_ConvertUTF8toUTF16 text(ev->text.text);

      // if key is detected, emit keyPress
      if (text.Length() == 1 && sInputState.keyPressIndex != nsInvalidKeycodeIndex) {
        WidgetKeyboardEvent event(true, eKeyPress, this);
        const nsKeyConverter &key = nsKeycodes[sInputState.keyPressIndex];
        uint keyCode = key.vkCode;
        event.mKeyCode = keyCode;
        event.mKeyNameIndex = key.keyNameIndex;
        event.mCodeNameIndex = key.codeNameIndex;
        event.mTime = ev->text.timestamp;
        event.SetCharCode(text[0]);

        event.mIsChar = true;
        if (!key.keyNameIndex || key.keyNameIndex == KEY_NAME_INDEX_USE_STRING) {
          event.mKeyNameIndex = KEY_NAME_INDEX_USE_STRING;
          event.mKeyValue.Append(text[0]);
        }
        if (!key.codeNameIndex || key.codeNameIndex == CODE_NAME_INDEX_USE_STRING) {
          event.mCodeNameIndex = CODE_NAME_INDEX_USE_STRING;
          event.mCodeValue.Append(text[0]);
        }
        event.mLocation = sInputState.keyPressLocation;
        event.mModifiers = sInputState.modifiers;
        event.mIsRepeat = sInputState.keyPressRepeat;
        DispatchInputEvent(&event);
      } else { // compose text without keypresses
#if 0
        WidgetCompositionEvent event(true, eCompositionStart, this);
        event.mTime = ev->text.timestamp;
        nsEventStatus status = nsEventStatus_eIgnore;
        DispatchEvent(&event, status);
        event.mData = text;
        event.mMessage = eCompositionCommit;
        DispatchEvent(&event, status);
#else
        RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher();
        nsresult rv = dispatcher->BeginNativeInputTransaction();
        if (!text.Length()) {
            break;
        }
        WidgetEventTime eventTime;
        eventTime.mTime = ev->text.timestamp;
        nsEventStatus status = nsEventStatus_eIgnore;
        dispatcher->CommitComposition(status, &text, &eventTime);
#endif
      }
    }
    break;
  case SDL_TEXTEDITING:
  {
    NS_ConvertUTF8toUTF16 text(ev->edit.text);
    RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher();
    nsresult rv = dispatcher->BeginNativeInputTransaction();
    WidgetEventTime eventTime;
    eventTime.mTime = ev->text.timestamp;
    nsEventStatus status = nsEventStatus_eIgnore;
    int len = text.Length();
    if (!len) {
        break;
    }
    RefPtr<TextRangeArray> textRangeArray = new TextRangeArray();
    TextRange range;
    range.mStartOffset = ev->edit.start;
    range.mEndOffset = ev->edit.start + ev->edit.length;
    if (range.mEndOffset > len) {
      range.mEndOffset = len;
    }
    if (range.mStartOffset < 0) {
      range.mStartOffset = 0;
    }
    range.mRangeType = TextRangeType::eRawClause;
    range.mRangeStyle.mLineStyle = TextRangeStyle::LINESTYLE_SOLID;
    range.mRangeStyle.mDefinedStyles = TextRangeStyle::DEFINED_LINESTYLE;
    textRangeArray->AppendElement(range);
    dispatcher->SetPendingComposition(text, textRangeArray);
    dispatcher->FlushPendingComposition(status, &eventTime);
  }
  break;

  case SDL_TEXTEDITING_EXT:
    textEditFree = true;
    // todo: prcoess this too
    break;
  case SDL_WINDOWEVENT:
    switch (ev->window.event) {
    case SDL_WINDOWEVENT_EXPOSED:
        if (mNativeWindow) {
          Invalidate({0, 0, mBounds.width, mBounds.height});
        }
      break;
    case SDL_WINDOWEVENT_CLOSE:
      if (mWidgetListener) {
        mWidgetListener->RequestWindowClose(this);
      }
      break;
    case SDL_WINDOWEVENT_MOVED:
      mBounds.x = ev->window.data1;
      mBounds.y = ev->window.data2;
      if (mNativeWindow) {
        ReportMoveEvent();
      }
      break;
    case SDL_WINDOWEVENT_RESIZED:
      if (mNativeWindow && nsAppShell::GetForcedFullscreen() < 1) {
        SDL_GetWindowSizeInPixels(mNativeWindow, &mDrawableSize.width, &mDrawableSize.height);
        HandleResize(ev->window.data1, ev->window.data2);
      }
      break;
    case SDL_WINDOWEVENT_LEAVE:
    case SDL_WINDOWEVENT_ENTER:
    case SDL_WINDOWEVENT_FOCUS_LOST:
      break;
    case SDL_WINDOWEVENT_FOCUS_GAINED:
    case SDL_WINDOWEVENT_TAKE_FOCUS:
      if (mWidgetListener) {
        mWidgetListener->WindowActivated();
      }
      break;
    }

    break;
    // SDL only receives text and file drags
  case SDL_DROPFILE:
  case SDL_DROPTEXT:
  case SDL_DROPBEGIN:
  case SDL_DROPCOMPLETE: {
    int x, y;
    SDL_GetMouseState(&x, &y);
    nsDragService::HandleIncoming(
        this, ev->drop.file, x, y, ev->type == SDL_DROPCOMPLETE,
        ev->type == SDL_DROPFILE, ev->drop.timestamp);
    if (ev->drop.file)
      dropFree = true;
  } break;
  }

  // pass child widgets, but not toplevel windows, if any
  for (uint32_t i = 0; i < mChildren.Length(); ++i) {
    // pass events to non-toplevel children
    if (!mChildren[i]->mNativeWindow) {
      mChildren[i]->OnEvent(ev);
    }
  }

  // free SDL  "leaked" data after processing events in child windows
  if (mNativeWindow) {

    if (dropFree) {
      SDL_free(ev->drop.file);
    }
    if (textEditFree) {
      SDL_free(ev->editExt.text);
    }
  }
  return true;
}

nsIWidget::LayerManager*
nsWindow::GetLayerManager(PLayerTransactionChild* aShadowManager,
                          LayersBackend aBackendHint,
                          LayerManagerPersistence aPersistence)
{
  if (mNativeWindow) {
    if (mLayerManager) {
      return mLayerManager;
    }

    LayerManager *ret = nsBaseWidget::GetLayerManager(aShadowManager, aBackendHint, aPersistence);
    if (!ret ||  mRotation == ROTATION_0) {
      return ret;
    }
    if (ClientLayerManager *mgr = ret->AsClientLayerManager()) {
      mgr->SetDefaultTargetConfiguration(BufferMode::BUFFER_NONE, (ScreenRotation)mRotation);
    } else if (BasicLayerManager *mgr = ret->AsBasicLayerManager()) {
      mgr->SetDefaultTargetConfiguration(BufferMode::BUFFER_NONE, (ScreenRotation)mRotation);
    }
    return ret;
  }
  // never create LayerManagers for child widgets
  if (mParent) {
    return mParent->GetLayerManager(aShadowManager, aBackendHint, aPersistence);
  }
  return mLayerManager;
}

static ScreenIntPoint ToScreen(const LayoutDeviceIntPoint aPoint) {
  return ViewAs<ScreenPixel>(aPoint, PixelCastJustification::LayoutDeviceIsScreenForUntransformedEvent);
}

nsEventStatus nsWindow::ProcessUntransformedAPZEvent(
                             WidgetInputEvent* aEvent,
                             const ScrollableLayerGuid& aGuid,
                             uint64_t aInputBlockId,
                             nsEventStatus aApzResponse) {
  // ProcessUntransformedAPZEvent will fail without compositor session
  if (!mAPZC || !mCompositorSession || !aEvent->mFlags.mHandledByAPZ) {
    nsEventStatus status;
    DispatchEvent(aEvent, status);
    return status;
  }
  return nsBaseWidget::ProcessUntransformedAPZEvent(aEvent, aGuid, aInputBlockId, aApzResponse);
}

//static
bool nsWindow::PreprocessEvent(SDL_Event *ev, nsWindow **aWindow)
{
  static uint32_t lastMoveTime;
  static uint32_t lastTouchID;
  static uint16_t buttons;
  static mozilla::Modifiers modifiers;
  static bool touchDrag;
  ScrollableLayerGuid guid;
  uint64_t blockId;
  nsEventStatus status = nsEventStatus_eIgnore;

  Uint32 winId = 0;
  bool dropFree = false;
  bool textEditFree = false;
  *aWindow = nullptr;
  switch (ev->type) {
  case SDL_KEYDOWN:
  case SDL_KEYUP:
    winId = ev->key.windowID;
    modifiers = ConvertModifiers(ev->key.keysym.mod);
    break;
  case SDL_WINDOWEVENT:
    winId = ev->window.windowID;
    break;
  case SDL_TEXTINPUT:
    winId = ev->text.windowID;
    break;
  case SDL_MOUSEMOTION: {
    winId = ev->motion.windowID;
    SDL_Window *win = SDL_GetWindowFromID(winId);
    nsWindow *w = (nsWindow *)SDL_GetWindowData(win, "nsWindow");
    if (!w) {
      return false;
    }
    bool touch = ev->motion.which == SDL_TOUCH_MOUSEID || nsAppShell::gAppShell->mEmulateTouchscreen;
    MouseInput input(MouseInput::MOUSE_MOVE, MouseInput::NONE,
                     touch?nsIDOMMouseEvent::MOZ_SOURCE_TOUCH:nsIDOMMouseEvent::MOZ_SOURCE_MOUSE,
                     buttons, ToScreen(w->TransformCoord(ev->motion.x, ev->motion.y)),
                     ev->motion.timestamp, TimeStamp::Now(), modifiers);
    // skip APZ for touch-generated events, it conflicts with touch APZ
    if(!touch&& w->mAPZC) {
      status = w->mAPZC->ReceiveInputEvent(input, &guid, &blockId);

    }
    if (status == nsEventStatus_eConsumeNoDefault) {
      return false;
    }
    if (touch && w->mHandleTouchEvent && w->mAPZC) {
      // pass some motion events to make ui responsible
      // do not send events to often
      input.mInputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
      if ((ev->motion.timestamp - lastMoveTime > 150 || sDragWindow) && w->mTouches.Count() == 1) {
        lastMoveTime = ev->motion.timestamp;
      } else {
        return false;
      }
    }
    // capture mouse when moving with clicked button and no drag session
    // if drag session active, mouse should not be captured to allow receive events from other windows (cross window drag)
    if (buttons & WidgetMouseEventBase::eLeftButtonFlag) {
      SDL_CaptureMouse(sDragWindow? SDL_FALSE: SDL_TRUE);
    }
    return nsAppShell::PostToMainThread(ev,[input, w, guid, blockId, status] {
      WidgetMouseEvent event = input.ToWidgetMouseEvent(w);
      bool consume = nsDragService::HandleMouseEvent(w, event, input.mOrigin.x, input.mOrigin.y);
      if (!consume) {
        w->ProcessUntransformedAPZEvent(&event, guid, blockId, status);
      }
      else {
        sDragWindow = w;
      }
    });;;
  }
    break;
  case SDL_MOUSEBUTTONUP:
  case SDL_MOUSEBUTTONDOWN: {
    winId = ev->button.windowID;
    SDL_Window *win = SDL_GetWindowFromID(winId);
    nsWindow *w = (nsWindow *)SDL_GetWindowData(win, "nsWindow");
    if (!w) {
      return false;
    }
    MouseInput::ButtonType btype = MouseInput::NONE;
    switch (ev->button.button) {
      case SDL_BUTTON_RIGHT:
        btype = MouseInput::RIGHT_BUTTON;
        break;
      case SDL_BUTTON_LEFT:
        btype = MouseInput::LEFT_BUTTON;
        break;
      case SDL_BUTTON_MIDDLE:
        btype = MouseInput::MIDDLE_BUTTON;
        break;
    }
    if (btype !=  MouseInput::NONE) {
      if (ev->type == SDL_MOUSEBUTTONUP) {
        buttons &= ~(1 << btype);
      } else {
        buttons |= 1 << btype;
      }
    }

    bool touch = ev->button.which == SDL_TOUCH_MOUSEID || nsAppShell::gAppShell->mEmulateTouchscreen;
    bool apz = w->mHandleTouchEvent && w->mAPZC;

    MouseInput input(
      ev->type == SDL_MOUSEBUTTONDOWN?MouseInput::MOUSE_DOWN: MouseInput::MOUSE_UP,
      btype, touch?nsIDOMMouseEvent::MOZ_SOURCE_TOUCH:nsIDOMMouseEvent::MOZ_SOURCE_MOUSE, buttons,
      ToScreen(w->TransformCoord(ev->button.x, ev->button.y)),
      ev->button.timestamp, TimeStamp::Now(), modifiers);
    int clicks = ev->button.clicks;
    if (ev->type == SDL_MOUSEBUTTONDOWN && apz && clicks == 2) {
      // eMouseTouchDrag works only if sent after touch input
      touchDrag = true;
    }

    // skip APZ for touch-generated events, it conflicts with touch APZ
    if (!touch && w->mAPZC) {
      status = w->mAPZC->ReceiveInputEvent(input, &guid, &blockId);
    }

    if (status == nsEventStatus_eConsumeNoDefault) {
      return false;
    }

    // reset mouse capture on any button event
    SDL_CaptureMouse(SDL_FALSE);
    sDragWindow = nullptr;
    return nsAppShell::PostToMainThread(ev,[input, w, guid, blockId, status, apz, touch, clicks] {
      WidgetMouseEvent event = input.ToWidgetMouseEvent(w);
      event.mClickCount = clicks;

      // right menu click on linux should also pop up a context menu
      if (event.button == WidgetMouseEvent::eRightButton) {
        WidgetMouseEvent contextMenuEvent(true, eContextMenu, w, WidgetMouseEvent::eReal);
        contextMenuEvent.mRefPoint = event.mRefPoint;
        contextMenuEvent.mClickCount = event.mClickCount;
        contextMenuEvent.mTime = event.mTime;
        w->DispatchInputEvent(&contextMenuEvent);
      }
      nsDragService::HandleMouseEvent(w, event, input.mOrigin.x, input.mOrigin.y);
      if (touch && apz && event.mMessage == eMouseDown && event.mClickCount == 1) {
        event.mMessage = eMouseMove;
        event.mClickCount = 0;
        event.button = 0;
        nsEventStatus status = nsEventStatus_eIgnore;
        // bypass APZ
        w->DispatchEvent(&event, status);
      }
      if (!touch) {
        w->ProcessUntransformedAPZEvent(&event, guid, blockId, status);
      }

    });
    break;
  }
  case SDL_MOUSEWHEEL: {
    winId = ev->wheel.windowID;
    SDL_Window *win = SDL_GetWindowFromID(winId);
    nsWindow *w = (nsWindow *)SDL_GetWindowData(win, "nsWindow");
    if (!w) {
      return false;
    }
    int posX, posY;
    SDL_GetMouseState(&posX, &posY);

    ScrollWheelInput input(ev->wheel.timestamp, TimeStamp::Now(), modifiers,
                           ScrollWheelInput::SCROLLMODE_SMOOTH,
                           ScrollWheelInput::SCROLLDELTA_LINE,
                           ToScreen(w->TransformCoord(posX, posY)),
                           ev->wheel.x, ev->wheel.y * -3,
                           false);
    input.mLineOrPageDeltaX = ev->wheel.x;
    input.mLineOrPageDeltaY = ev->wheel.y * -3;
    // WheelPrefs action modifiers ignored when event is handled, so only process APZ with zero modifiers
    if (w->mAPZC && !modifiers) {
      status = w->mAPZC->ReceiveInputEvent(input, &guid, &blockId);
    }
    if (status == nsEventStatus_eConsumeNoDefault) {
      return false;
    }
    return nsAppShell::PostToMainThread(ev,[input, w, guid, blockId, status]() {
      WidgetWheelEvent event = input.ToWidgetWheelEvent(w);
      w->ProcessUntransformedAPZEvent(&event, guid, blockId, status);
    });
    break;
  }
  case SDL_DROPFILE:
  case SDL_DROPTEXT:
  case SDL_DROPBEGIN:
  case SDL_DROPCOMPLETE:
    winId = ev->drop.windowID;
    if (ev->drop.file) {
      dropFree = true;
    }
    break;
  case SDL_TEXTEDITING:
    winId = ev->edit.windowID;
    break;
  case SDL_TEXTEDITING_EXT:
    winId = ev->editExt.windowID;
    textEditFree = true;
    break;
  case SDL_FINGERMOTION:
  case SDL_FINGERDOWN:
  case SDL_FINGERUP: {
    winId = ev->tfinger.windowID;
    SDL_Window *win = SDL_GetWindowFromID(winId);
    nsWindow *w = (nsWindow *)SDL_GetWindowData(win, "nsWindow");
    if (!w) {
      return false;
    }
    MultiTouchInput touchInput;

    // start and move use same code path, so check for it first
    touchInput.mType = ev->type == SDL_FINGERMOTION ? MultiTouchInput::MULTITOUCH_MOVE: MultiTouchInput::MULTITOUCH_START;
    touchInput.mTime = ev->tfinger.timestamp;
    touchInput.modifiers = modifiers;
    int32_t id;
    nsAutoPtr<SingleTouchData> touch;

    // generate IDs with dictionary (same as gtk touch)
    w->mTouches.RemoveAndForget((uint64_t)ev->tfinger.fingerId, touch);
    if (touch) {
      id = touch->mIdentifier;
    } else {
      id = ++lastTouchID & 0x7FFFFFFF;
    }
    LayoutDeviceIntPoint point = w->TransformCoord(w->mDrawableSize.width * ev->tfinger.x, w->mDrawableSize.height * ev->tfinger.y);
    touch = new SingleTouchData(id, ToScreen(point), ScreenSize(1,1), 0.0f, 1.0f);

    if (ev->type == SDL_FINGERUP) {
      // on end only send touch, which generated this event
      touchInput.mType = MultiTouchInput::MULTITOUCH_END;
      touchInput.mTouches.AppendElement(*touch);
    } else {
      // start/move report all touches
      w->mTouches.Put((uint64_t)ev->tfinger.fingerId, touch.forget());

      // count detected touches
      int count = 0;
      for (auto iter = w->mTouches.Iter(); !iter.Done(); iter.Next()) {
        touchInput.mTouches.AppendElement(*iter.UserData());
        count++;
      }
      if (count > w->mMaxTouchFingerCount) {
        w->mMaxTouchFingerCount = count;
      }
    }
    if (w->mAPZC) {
      status = w->mAPZC->ReceiveInputEvent(touchInput, &guid, &blockId);
    }
    if (status == nsEventStatus_eConsumeNoDefault) {
      return false;
    }
    uint32_t time = ev->tfinger.timestamp;
    return nsAppShell::PostToMainThread(ev,[touchInput, w, guid, blockId, status]() {
      WidgetTouchEvent touchEvent = touchInput.ToWidgetTouchEvent(w);
      w->ProcessUntransformedAPZEvent(&touchEvent, guid, blockId, status);
      if (touchEvent.mMessage == eTouchStart && touchEvent.mTouches.Length() == 1) {
        WidgetMouseEvent event(true, mozilla::eMouseHitTest, w, WidgetMouseEvent::eReal);
        event.mRefPoint = touchEvent.mTouches[0]->mRefPoint;
        event.mIgnoreRootScrollFrame = true;
        event.inputSource = nsIDOMMouseEvent::MOZ_SOURCE_TOUCH;
        event.mTime = touchInput.mTime;
        nsEventStatus status;
        w->DispatchEvent(&event, status);

        if (w->mAPZEventState && event.hitCluster) {
          w->mAPZEventState->ProcessClusterHit();
        }
        if (touchDrag) {
          event.hitCluster = false;
          event.mIgnoreRootScrollFrame = false;
          event.mMessage = mozilla::eMouseTouchDrag;
          event.mClickCount = 2;
          event.button= WidgetMouseEvent::eLeftButton;
          w->DispatchEvent(&event, status);
          touchDrag = false;
        }
      }

    });

  }
  break;
  }

  SDL_Window *win = SDL_GetWindowFromID(winId);
  if (!win) {
    return false;
  }

  *aWindow = (nsWindow *)SDL_GetWindowData(win, "nsWindow");
  if (!*aWindow) {
    if (dropFree) {
      SDL_free(ev->drop.file);
    }
    if (textEditFree) {
      SDL_free(ev->editExt.text);
    }
    return false;
  }

  return !!*aWindow;
}


// stub TextEventDispatcherListener to make TextEventDispatcher happy
class IMContextWrapper final : public nsWindow::TextEventDispatcherListener
{
public:
  // TextEventDispatcherListener implementation
  NS_DECL_ISUPPORTS

      NS_IMETHOD NotifyIME(nsWindow::TextEventDispatcher* aTextEventDispatcher,
                       const nsWindow::IMENotification& aNotification) override {
        return NS_OK;
      }
  NS_IMETHOD_(void) OnRemovedFrom(
        nsWindow::TextEventDispatcher* aTextEventDispatcher) override {
  }
  NS_IMETHOD_(void) WillDispatchKeyboardEvent(
    nsWindow::TextEventDispatcher* aTextEventDispatcher,
    WidgetKeyboardEvent& aKeyboardEvent,
    uint32_t aIndexOfKeypress,
    void* aData) override {
  }
private:
  ~IMContextWrapper() {}
};

NS_IMPL_ISUPPORTS(IMContextWrapper, TextEventDispatcherListener, nsISupportsWeakReference);

NS_IMETHODIMP_(nsWindow::TextEventDispatcherListener*)
nsWindow::GetNativeTextEventDispatcherListener()
{
  if(!mTextEventDispatcherListener) {
    mTextEventDispatcherListener = new IMContextWrapper();
  }
  return mTextEventDispatcherListener;
}

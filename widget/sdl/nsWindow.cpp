/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


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
#include "mozilla/TextEventDispatcher.h"

#include "GeckoProfiler.h"
#include "ClientLayerManager.h"
#include "nsDragService.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
//static nsTArray<nsWindow*> gTopLevelWindows;
#define ALOG(args...) fprintf(stderr, args); fprintf(stderr, "\n")


NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, Inherited)

nsWindow::nsWindow()
  : mNativeWindow(nullptr)
  , mVisible(false)
  , mParent(nullptr)
{
}

nsWindow::~nsWindow()
{
  if (mNativeWindow) {
    MutexAutoLock lock(nsAppShell::gAppShell->mEventsMutex);
    SDL_DestroyWindow(mNativeWindow);
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
    mNativeWindow = SDL_CreateWindow("UXP", aRect.x, aRect.y, aRect.width, aRect.height, flags);
    if (!mNativeWindow) { // if it's failed, reset opengl flag and retry
      flags &= ~SDL_WINDOW_OPENGL;
      mNativeWindow = SDL_CreateWindow("UXP", aRect.x, aRect.y, aRect.width, aRect.height, flags);
    }
    SDL_SetWindowData(mNativeWindow, "nsWindow", this);
  }

  // set parent from native parent
  if (!parent && aNativeParent) {
    parent = (nsWindow *)SDL_GetWindowData((SDL_Window *)aNativeParent, "nsWindow");
  }

  if (parent) {
    parent->mChildren.AppendElement(this);
    mParent = parent;
    if (mNativeWindow) {
      SDL_Window *nativeParent = (SDL_Window *)GetNativeData(NS_NATIVE_WIDGET);
      // make popups and modals draw over parent window
      if (nativeParent) {
        SDL_SetWindowModalFor(mNativeWindow, nativeParent);
      }
    }
  }

  // save x11 display for EGL
#ifdef SDL_VIDEO_DRIVER_X11
  {
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);

    if (mNativeWindow && !sX11Display &&
        SDL_GetWindowWMInfo(mNativeWindow, &wminfo)) {
      sX11Display = wminfo.info.x11.display;
    }
  }
#endif
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
    SDL_DestroyWindow(win);
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
  if (mNativeWindow && mVisible != aState) {
    if (aState) {
      SDL_ShowWindow(mNativeWindow);
    } else{
      SDL_HideWindow(mNativeWindow);
    }
  }
  mVisible = aState;
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

  ReportSizeEvent();

  if (mNativeWindow) {
    ScheduleResize();
  }

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

void nsWindow::ReportSizeEvent()
{
  if (mWidgetListener) {
    LayoutDeviceIntRect innerBounds = GetClientBounds();
    mWidgetListener->WindowResized(this, innerBounds.width, innerBounds.height);
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

  if (mWidgetListener) {
    aStatus = mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);
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
  void *retVal = nullptr;

  switch (aDataType) {
  case NS_NATIVE_DISPLAY: {
    // for now, only support X11 platform for EGL
    retVal = sX11Display;
    break;
  }

  case NS_NATIVE_WIDGET:
    // Use SDL_Window as widget to handle parent widgets
    return mNativeWindow;
  case NS_NATIVE_WINDOW: {
    // EGL code using this
#if SDL_VIDEO_DRIVER_X11
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);

    retVal = nullptr;
    if (mNativeWindow) {
      SDL_GetWindowWMInfo(mNativeWindow, &wminfo);
      if(wminfo.subsystem == SDL_SYSWM_X11) {
        retVal = (void *)wminfo.info.x11.window;
      }
    // hack: we do not support child widgets, so pass call
    // to toplevel widget, so compositor will always render to toplevel
    } else if (mParent) {
      return mParent->GetNativeData(aDataType);
    }
#endif
    break;
  }

  case NS_RAW_NATIVE_IME_CONTEXT:
    retVal = NS_ONLY_ONE_NATIVE_IME_CONTEXT;
    break;
  case NS_NATIVE_EGL_PLATFORM: {
    SDL_SysWMinfo wminfo;
    SDL_VERSION(&wminfo.version);
    SDL_GetWindowWMInfo(mNativeWindow, &wminfo);
    if(wminfo.subsystem == SDL_SYSWM_X11) {
      retVal = (void *)0x31D5; //  EGL_PLATFORM_X11_KHR
    }
    break;
  }
  }

  return retVal;
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

  if (clientLayerManager) {
    // we are in compositor client
    // just commit region to OMTC
    clientLayerManager->SetNeedsComposite(true);
    clientLayerManager->SendInvalidRegion(region.ToUnknownRegion());
    mWidgetListener->WillPaintWindow(this);
    mWidgetListener->PaintWindow(this, region);
    mWidgetListener->DidPaintWindow();

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
    mWidgetListener->WillPaintWindow(this);
    mWidgetListener->PaintWindow(this, region);
    mWidgetListener->DidPaintWindow();

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

bool nsWindow::OnEvent(const SDL_Event *ev)
{
  static struct {
    bool mouseDown, keyPressRepeat;
    uint8_t keyPressLocation, keyPressIndex;
    mozilla::Modifiers modifiers;
  } sInputState;

  switch (ev->type) {
  case SDL_KEYDOWN:
  case SDL_KEYUP: {
    WidgetKeyboardEvent event(true, ev->type == SDL_KEYDOWN ? eKeyDown : eKeyUp, this);
    event.mTime = ev->key.timestamp;
    uint sym = ev->key.keysym.sym;
    int keyIndex = MapKey(sym);
    const nsKeyConverter &key = nsKeycodes[keyIndex];
    uint keyCode = key.vkCode;
    event.mKeyCode = keyCode;
    event.mKeyNameIndex = key.keyNameIndex;
    event.mCodeNameIndex = key.codeNameIndex;
    sInputState.modifiers = ConvertModifiers(ev->key.keysym.mod);

    // handle printable syms
    if (sym <= 0x7f) {
      HandleCharSyms(event, key, sym);
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
        nsEventStatus status;
        RefPtr<TextEventDispatcher> dispatcher = GetTextEventDispatcher();
        nsresult rv = dispatcher->BeginNativeInputTransaction();
        dispatcher->DispatchKeyboardEvent(eKeyDown, event, status);
        if (dispatcher->IsComposing()) {
          dispatcher->CommitComposition(status, nullptr, &event);
        }
    }

    DispatchInputEvent(&event);

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
      if (event.mKeyNameIndex == KEY_NAME_INDEX_USE_STRING &&  !event.mCharCode) {
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
      mBounds.width = ev->window.data1;
      mBounds.height = ev->window.data2;
      if (mNativeWindow) {
        ReportSizeEvent();
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
  case SDL_MOUSEMOTION: {
    WidgetMouseEvent event(true, eMouseMove, this, WidgetMouseEvent::eReal,
                           WidgetMouseEvent::eNormal);

    event.mRefPoint.x = nscoord(ev->motion.x);
    event.mRefPoint.y = nscoord(ev->motion.y);
    event.mModifiers = sInputState.modifiers;
    event.mTime = ev->motion.timestamp;

    // if fake drag enabled, consume motion
    bool consume = false;
    // pass child widget events to DragService
    if (mWidgetListener && !mNativeWindow) {
      consume = nsDragService::HandleMouseEvent(this, event, ev->motion.x, ev->motion.y);
    }
    if (!consume) {
      DispatchInputEvent(&event);
      // capture mouse if button down when not dragging
      if (sInputState.mouseDown && mWidgetListener && !mNativeWindow) {
        SDL_CaptureMouse(SDL_TRUE);
      }
    } else
      sDragWindow = this;
  } break;
  case SDL_MOUSEBUTTONUP:
  case SDL_MOUSEBUTTONDOWN: {
    if (ev->button.button == SDL_BUTTON_LEFT) {
      sInputState.mouseDown = ev->type == SDL_MOUSEBUTTONDOWN;
    }

    // reset capture, set in SDL_MOUSEMOTION
    SDL_CaptureMouse(SDL_FALSE);
    sDragWindow = nullptr;

    WidgetMouseEvent event(
        true, ev->type == SDL_MOUSEBUTTONUP ? eMouseUp : eMouseDown, this,
        WidgetMouseEvent::eReal);

    event.mRefPoint.x = nscoord(ev->button.x);
    event.mRefPoint.y = nscoord(ev->button.y);
    event.mClickCount = ev->button.clicks;
    event.mModifiers = sInputState.modifiers;
    event.mTime = ev->button.timestamp;

    switch (ev->button.button) {
    case SDL_BUTTON_RIGHT:
      event.button = WidgetMouseEvent::eRightButton;
      break;
    case SDL_BUTTON_LEFT:
      event.button = WidgetMouseEvent::eLeftButton;
      break;
    case SDL_BUTTON_MIDDLE:
      event.button = WidgetMouseEvent::eMiddleButton;
      break;
    }

    // right menu click on linux should also pop up a context menu
    if (event.button == WidgetMouseEvent::eRightButton) {
      WidgetMouseEvent contextMenuEvent(true, eContextMenu, this, WidgetMouseEvent::eReal);
      contextMenuEvent.mRefPoint.x = nscoord(ev->button.x);
      contextMenuEvent.mRefPoint.y = nscoord(ev->button.y);
      contextMenuEvent.mClickCount = ev->button.clicks;
      contextMenuEvent.mTime = ev->button.timestamp;
      DispatchInputEvent(&contextMenuEvent);
    }
    event.mTime = ev->button.timestamp;

    // pass child widget events to DragService
    if (mWidgetListener && !mNativeWindow) {
      nsDragService::HandleMouseEvent(this, event, ev->button.x, ev->button.y);
    }
    DispatchInputEvent(&event);

  } break;
  case SDL_MOUSEWHEEL: {
    WidgetWheelEvent wheelEvent(true, eWheel, this);

    wheelEvent.mDeltaMode = nsIDOMWheelEvent::DOM_DELTA_LINE;
    wheelEvent.mDeltaX = wheelEvent.mLineOrPageDeltaX = ev->wheel.x;
    // scroll by 3 lines
    wheelEvent.mDeltaY = wheelEvent.mLineOrPageDeltaY = ev->wheel.y * -3;

    int posX, posY;
    SDL_GetMouseState(&posX, &posY);

    wheelEvent.mRefPoint.x = nscoord(posX);
    wheelEvent.mRefPoint.y = nscoord(posY);
    wheelEvent.mModifiers = sInputState.modifiers;
    wheelEvent.mTime = ev->wheel.timestamp;

    DispatchInputEvent(&wheelEvent);
  } break;
    // SDL only receives text and file drags
  case SDL_DROPFILE:
  case SDL_DROPTEXT:
  case SDL_DROPBEGIN:
  case SDL_DROPCOMPLETE:
    // only send drops to child widget
    if (mWidgetListener && !mNativeWindow) {
      int x, y;
      SDL_GetMouseState(&x, &y);
      // printf("drop %x %s %d %d\n", ev->type, ev->drop.file, x, y);
      nsDragService::HandleIncoming(
          this, ev->drop.file, x, y, ev->type == SDL_DROPCOMPLETE,
          ev->type == SDL_DROPFILE, ev->drop.timestamp);
    }
    break;
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

    return nsBaseWidget::GetLayerManager(aShadowManager, aBackendHint, aPersistence);
  }
  // never create LayerManagers for child widgets
  if (mParent) {
    return mParent->GetLayerManager(aShadowManager, aBackendHint, aPersistence);
  }
  return mLayerManager;
}

//static
bool nsWindow::OnGlobalEvent(const SDL_Event *ev)
{
  Uint32 winId = 0;
  bool dropFree = false;
  bool textEditFree = false;
  switch (ev->type) {
  case SDL_KEYDOWN:
  case SDL_KEYUP:
    winId = ev->key.windowID;
    break;
  case SDL_WINDOWEVENT:
    winId = ev->window.windowID;
    break;
  case SDL_TEXTINPUT:
    winId = ev->text.windowID;
    break;
  case SDL_MOUSEMOTION:
    winId = ev->motion.windowID;
    break;
  case SDL_MOUSEBUTTONUP:
  case SDL_MOUSEBUTTONDOWN:
    winId = ev->button.windowID;
    break;
  case SDL_MOUSEWHEEL:
    winId = ev->wheel.windowID;
    break;
  case SDL_DROPFILE:
  case SDL_DROPTEXT:
  case SDL_DROPBEGIN:
  case SDL_DROPCOMPLETE:
    winId = ev->drop.windowID;
    if (ev->drop.file)
      dropFree = true;
    break;
  case SDL_TEXTEDITING:
    winId = ev->edit.windowID;
    break;
  case SDL_TEXTEDITING_EXT:
    winId = ev->editExt.windowID;
    textEditFree = true;
    break;
  }

  SDL_Window *win = SDL_GetWindowFromID(winId);
  if (!win) {
    return false;
  }

  nsWindow *aWindow = (nsWindow *)SDL_GetWindowData(win, "nsWindow");
  if (!aWindow) {
    return false;
  }

  aWindow->OnEvent(ev);
  for (uint32_t i = 0; i < aWindow->mChildren.Length(); ++i) {
    // pass events to non-toplevel children
    if (!aWindow->mChildren[i]->mNativeWindow) {
      aWindow->mChildren[i]->OnEvent(ev);
    }
  }

  // SDL2 doucmented memory "leaks"
  if (dropFree) {
    SDL_free(ev->drop.file);
  }
  if (textEditFree) {
    SDL_free(ev->editExt.text);
  }

  return true;
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

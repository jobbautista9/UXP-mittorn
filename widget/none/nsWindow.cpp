/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */


#include "nsWindow.h"
#include "nsAppShell.h"

#include "nsWidgetsCID.h"
#include "nsGfxCIID.h"

#include "gfxUtils.h"
#include "gfxImageSurface.h"
#include "gfxContext.h"
#include "nsRegion.h"
#include "Layers.h"
#include "nsTArray.h"

#include "mozilla/BasicEvents.h"
#include "mozilla/TouchEvents.h"
#include "mozilla/Unused.h"

#include "GeckoProfiler.h"

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;
static nsTArray<nsWindow*> gTopLevelWindows;
#define ALOG(args...) fprintf(stderr, args); fprintf(stderr, "\n")


NS_IMPL_ISUPPORTS_INHERITED0(nsWindow, Inherited)

nsWindow::nsWindow() :
  mVisible(false),
  mParent(nullptr)
{
}

nsWindow::~nsWindow()
{
}
bool
nsWindow::IsTopLevel()
{
    return mWindowType == eWindowType_toplevel ||
        mWindowType == eWindowType_dialog ||
        mWindowType == eWindowType_invisible;
}

//
// nsIWidget
//

nsresult
nsWindow::Create(nsIWidget* aParent,
                 nsNativeWidget aNativeParent,
                 const LayoutDeviceIntRect& aRect,
                 nsWidgetInitData* aInitData)
{
    ALOG("nsWindow[%p]::Create %p/%p [%d %d %d %d]", (void*)this, (void*)aParent, (void*)aNativeParent, aRect.x, aRect.y, aRect.width, aRect.height);
    nsWindow* parent = (nsWindow*) aParent;
    mBounds = aRect;
    // Set defaults which can be overriden from aInitData in BaseCreate
    mWindowType = eWindowType_toplevel;
    mBorderStyle = eBorderStyle_default;
    Inherited::BaseCreate(aParent, aInitData);
    if (IsTopLevel()) {
        gTopLevelWindows.AppendElement(this);
        mVisible = true;
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

    if (mParent)
        mParent->mChildren.RemoveElement(this);

    if (IsTopLevel())
        gTopLevelWindows.RemoveElement(this);

    nsBaseWidget::Destroy();
    nsBaseWidget::OnDestroy();
}

NS_IMETHODIMP
nsWindow::ConfigureChildren(const nsTArray<nsIWidget::Configuration>& config)
{
    for (uint32_t i = 0; i < config.Length(); ++i) {
        nsWindow *childWin = (nsWindow*) config[i].mChild.get();
        childWin->Resize(config[i].mBounds.x,
                         config[i].mBounds.y,
                         config[i].mBounds.width,
                         config[i].mBounds.height,
                         false);
    }

    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Show(bool aState)
{
    mVisible = aState;
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::Move(double aX, double aY)
{
    mBounds.x = aX;
    mBounds.y = aY;

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
    if ((!isMoving && !isResizing))
        return NS_OK;

    if (isMoving) {
        mBounds.x = aX;
        mBounds.y = aY;
    }
    if (isResizing) {
        mBounds.width  = aWidth;
        mBounds.height  = aHeight;
    }

    if (isMoving)
        ReportMoveEvent();

    if (isResizing)
        ReportSizeEvent();

    return NS_OK;
}

NS_IMETHODIMP nsWindow::Resize(double aWidth, double aHeight, bool aRepaint)
{
    if ((mBounds.width == aWidth && mBounds.height == aHeight))
        return NS_OK;

    mBounds.width  = aWidth;
    mBounds.height = aHeight;

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
        nsBaseWidget::InfallibleMakeFullScreen(true);
    }
    ReportSizeModeEvent(aMode);
}

NS_IMETHODIMP
nsWindow::Invalidate(const LayoutDeviceIntRect& aRect)
{
    if (!mVisible)
      return NS_OK;

    MOZ_RELEASE_ASSERT(GetLayerManager()->GetBackendType() != LayersBackend::LAYERS_CLIENT,
                     "Shouldn't need to invalidate with accelerated OMTC layers!");
    return NS_OK;
}

NS_IMETHODIMP
nsWindow::SetFocus(bool aRaise)
{
    return NS_OK;
}

void nsWindow::WillPaintWindow()
{
    if (mWidgetListener) {
      mWidgetListener->WillPaintWindow(this);
    }
}

bool nsWindow::PaintWindow(LayoutDeviceIntRegion aRegion)
{
    if (!mWidgetListener)
        return false;

    bool returnValue = false;
    returnValue = mWidgetListener->PaintWindow(this, aRegion);

    if (mWidgetListener) {
        mWidgetListener->DidPaintWindow();
    }

    return returnValue;
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
    LayoutDeviceIntPoint offset(0, 0);
    if (mParent) {
        offset = mParent->WidgetToScreenOffset();
    }

    return offset;
}

NS_IMETHODIMP
nsWindow::DispatchEvent(mozilla::WidgetGUIEvent* aEvent,
                        nsEventStatus& aStatus)
{
  aStatus = nsEventStatus_eIgnore;
  nsCOMPtr<nsIWidget> kungFuDeathGrip(aEvent->mWidget);

  if (mWidgetListener)
    aStatus = mWidgetListener->HandleEvent(aEvent, mUseAttachedEvents);

  return NS_OK;
}

NS_IMETHODIMP_(void)
nsWindow::SetInputContext(const InputContext& aContext,
                          const InputContextAction& aAction)
{
    //TODO: actually show VKB
    mInputContext = aContext;
}

NS_IMETHODIMP_(mozilla::widget::InputContext)
nsWindow::GetInputContext()
{
    return mInputContext;
}

void
nsWindow::SetBackgroundColor(const nscolor &aColor)
{
}

void* nsWindow::GetNativeData(uint32_t aDataType)
{
  void* retVal = nullptr;

  switch (aDataType)
  {
    case NS_NATIVE_WIDGET: // NULL NativeWindow will create Pbuffer
    case NS_NATIVE_DISPLAY:
      retVal = nullptr; // EGL_DEFAULT_DISPLAY
      break;

    case NS_NATIVE_WINDOW:
      retVal = nullptr;
      break;

    case NS_NATIVE_GRAPHIC:
      NS_ERROR("Requesting NS_NATIVE_GRAPHIC on a None child view!");
      break;

    case NS_NATIVE_OFFSETX:
      retVal = 0;
      break;

    case NS_NATIVE_OFFSETY:
      retVal = 0;
      break;

    case NS_NATIVE_PLUGIN_PORT:
        // not implemented
        break;

    case NS_RAW_NATIVE_IME_CONTEXT:
      retVal = NS_ONLY_ONE_NATIVE_IME_CONTEXT;
      break;
  }

  return retVal;
}


int32_t
nsWindow::RoundsWidgetCoordinatesTo()
{
  return 1;
}

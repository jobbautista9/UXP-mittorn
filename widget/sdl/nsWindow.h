/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSWINDOW_H_
#define NSWINDOW_H_

#include "nsBaseWidget.h"
#include "nsTArray.h"
#include "mozilla/TouchEvents.h"

#include <SDL.h>

class nsWindow final:
    public nsBaseWidget
{
  typedef nsBaseWidget Inherited;

public:
  nsWindow();

  NS_DECL_ISUPPORTS_INHERITED

  //
  // nsIWidget
  //

  [[nodiscard]] virtual nsresult
  Create(nsIWidget *aParent, nsNativeWidget aNativeParent,
         const LayoutDeviceIntRect &aRect,
         nsWidgetInitData *aInitData = nullptr) override;
  virtual void Destroy() override;
  NS_IMETHOD Show(bool aState) override;
  NS_IMETHOD Enable(bool aState) override { return NS_OK; }
  virtual bool IsEnabled() const override { return true; }
  virtual bool IsVisible() const override { return mVisible; }
  NS_IMETHOD SetFocus(bool aState = false) override;
  virtual LayoutDeviceIntPoint WidgetToScreenOffset() override;

  virtual void *GetNativeData(uint32_t aDataType) override;

  NS_IMETHOD Move(double aX, double aY) override;
  virtual void SetSizeMode(nsSizeMode aMode) override;
  NS_IMETHOD MakeFullScreen(bool aFullScreen, nsIScreen *aScreen) override;

  NS_IMETHOD Resize(double aWidth, double aHeight, bool aRepaint) override;
  NS_IMETHOD Resize(double aX, double aY, double aWidth, double aHeight,  bool aRepaint) override;
  virtual LayoutDeviceIntRect GetScreenBounds() override;
  void ReportMoveEvent();
  void ReportSizeEvent();
  void ReportSizeModeEvent(nsSizeMode aMode);

  NS_IMETHOD SetTitle(const nsAString &aTitle) override;
  NS_IMETHOD SetCursor(nsCursor aCursor) override;
  NS_IMETHOD SetCursor(imgIContainer *aCursor, uint32_t aHotspotX, uint32_t aHotspotY) override;

  // nsBaseWidget
  virtual LayerManager *GetLayerManager(
      PLayerTransactionChild *aShadowManager = nullptr,
      LayersBackend aBackendHint = mozilla::layers::LayersBackend::LAYERS_NONE,
      LayerManagerPersistence aPersistence = LAYER_MANAGER_CURRENT) override;

  NS_IMETHOD Invalidate(const LayoutDeviceIntRect &aRect) override;
  virtual nsresult ConfigureChildren(const nsTArray<Configuration> &aConfigurations) override;
  NS_IMETHOD DispatchEvent(mozilla::WidgetGUIEvent *aEvent, nsEventStatus &aStatus) override;

  virtual already_AddRefed<mozilla::gfx::DrawTarget> StartRemoteDrawing() override;
  virtual void EndRemoteDrawing() override;
  virtual void EndRemoteDrawingInRegion(
    mozilla::gfx::DrawTarget *aDrawTarget,
    LayoutDeviceIntRegion &aInvalidRegion) override;

  NS_IMETHOD_(void)
  SetInputContext(const InputContext &aContext, const InputContextAction &aAction) override;
  NS_IMETHOD_(InputContext) GetInputContext() override;
  void ConfigureAPZControllerThread() override;
  nsEventStatus DispatchInputEvent(mozilla::WidgetInputEvent* aEvent) override;

  static bool PreprocessEvent(SDL_Event *aEv, nsWindow** aWindow);
  static void HandleInvalidWindows();

  NS_IMETHODIMP_(TextEventDispatcherListener*)
  GetNativeTextEventDispatcherListener() override;
  void ScreenRotationChanged(int aScreenIndex, uint32_t aRotation);
  LayoutDeviceIntRect GetNaturalBounds() override;
  uint32_t        GetMaxTouchPoints() const override { return mMaxTouchFingerCount; }
  virtual void RegisterTouchWindow() override { mHandleTouchEvent = true; }
  void DispatchEventToAPZOnly(mozilla::WidgetInputEvent* aEvent) override;


  bool OnEvent(const SDL_Event *ev);

protected:
  virtual ~nsWindow();
  nsWindow *FindTopLevel();
  bool IsTopLevel();

  SDL_Window *mNativeWindow;
  SDL_Surface *mSurface;
  bool mVisible;
  bool mScheduleResize;
  uint32_t mRotation;
  nsTArray<nsWindow *> mChildren;
  nsWindow *mParent;
  InputContext mInputContext;
  RefPtr<TextEventDispatcherListener>mTextEventDispatcherListener;

  void OnSizeChanged(const mozilla::gfx::IntSize &aSize);
  void ExposeRegion(LayoutDeviceIntRegion);
  void ScheduleResize();
  void HandleResize(int aWidth, int aHeight);
  LayoutDeviceIntPoint TransformCoord(int aX, int aY);
  LayoutDeviceIntSize mDrawableSize;
  nsClassHashtable<nsUint64HashKey, mozilla::SingleTouchData> mTouches;
  int mMaxTouchFingerCount;
  bool mHandleTouchEvent;
};

#endif /* NSWINDOW_H_ */

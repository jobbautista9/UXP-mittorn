/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef NSWINDOW_H_
#define NSWINDOW_H_

#include "nsBaseWidget.h"

#include "nsTArray.h"

class nsWindow : public nsBaseWidget {
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
  void EnteredFullScreen(bool aFullScreen);
  NS_IMETHOD Resize(double aWidth, double aHeight, bool aRepaint) override;
  NS_IMETHOD Resize(double aX, double aY, double aWidth, double aHeight,
                    bool aRepaint) override;
  virtual LayoutDeviceIntRect GetScreenBounds() override;
  void ReportMoveEvent();
  void ReportSizeEvent();
  void ReportSizeModeEvent(nsSizeMode aMode);

  NS_IMETHOD SetTitle(const nsAString &aTitle) override { return NS_OK; }

  NS_IMETHOD Invalidate(const LayoutDeviceIntRect &aRect) override;
  NS_IMETHOD
  ConfigureChildren(const nsTArray<Configuration> &aConfigurations) override;
  NS_IMETHOD DispatchEvent(mozilla::WidgetGUIEvent *aEvent,
                           nsEventStatus &aStatus) override;

  NS_IMETHOD_(void)
  SetInputContext(const InputContext &aContext,
                  const InputContextAction &aAction) override;
  NS_IMETHOD_(InputContext) GetInputContext() override;

protected:
  virtual ~nsWindow();
  nsWindow *FindTopLevel();
  bool IsTopLevel();

  bool mVisible;
  nsTArray<nsWindow *> mChildren;
  nsWindow *mParent;
  InputContext mInputContext;

  void OnSizeChanged(const mozilla::gfx::IntSize &aSize);
};

#endif /* NSWINDOW_H_ */

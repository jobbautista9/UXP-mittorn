/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "none/nsScreenManager.h"
#include "nsIWidget.h"

NS_IMETHODIMP
nsNoneScreen::GetRect(int32_t *outX, int32_t *outY, int32_t *outWidth, int32_t *outHeight) {
  return GetRectDisplayPix(outX, outY, outWidth, outHeight);
}

NS_IMETHODIMP
nsNoneScreen::GetAvailRect(int32_t *outX, int32_t *outY, int32_t *outWidth, int32_t *outHeight) {
  return GetAvailRectDisplayPix(outX, outY, outWidth, outHeight);
}

NS_IMETHODIMP
nsNoneScreen::GetRectDisplayPix(int32_t *outX, int32_t *outY, int32_t *outWidth, int32_t *outHeight) {
  nsIntRect rect = {0, 0, 1920, 1080};
  *outX = rect.x;
  *outY = rect.y;
  *outWidth = rect.width;
  *outHeight = rect.height;
  return NS_OK;
}

NS_IMETHODIMP
nsNoneScreen::GetAvailRectDisplayPix(int32_t *outX, int32_t *outY, int32_t *outWidth, int32_t *outHeight) {
  nsIntRect rect = {0, 0, 1920, 1080};
  *outX = rect.x;
  *outY = rect.y;
  *outWidth = rect.width;
  *outHeight = rect.height;

  return NS_OK;
}

NS_IMETHODIMP
nsNoneScreen::GetPixelDepth(int32_t *aPixelDepth) {
  *aPixelDepth = 24;
  return NS_OK;
}

NS_IMETHODIMP
nsNoneScreen::GetColorDepth(int32_t *aColorDepth) {
  return GetPixelDepth(aColorDepth);
}

NS_IMETHODIMP
nsNoneScreen::GetContentsScaleFactor(double *aContentsScaleFactor) {
  *aContentsScaleFactor = 1.0;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsScreenManager, nsIScreenManager)

nsScreenManager::nsScreenManager() : mScreen(new nsNoneScreen()) {}

NS_IMETHODIMP
nsScreenManager::GetPrimaryScreen(nsIScreen **outScreen) {
  NS_IF_ADDREF(*outScreen = mScreen.get());
  return NS_OK;
}

NS_IMETHODIMP
nsScreenManager::ScreenForRect(int32_t inLeft, int32_t inTop, int32_t inWidth, int32_t inHeight, nsIScreen **outScreen) {
  return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
nsScreenManager::ScreenForId(uint32_t id, nsIScreen **outScreen) {
  return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
nsScreenManager::ScreenForNativeWidget(void *aWidget, nsIScreen **outScreen) {
  return GetPrimaryScreen(outScreen);
}

NS_IMETHODIMP
nsScreenManager::GetNumberOfScreens(uint32_t *aNumberOfScreens) {
  *aNumberOfScreens = 1;
  return NS_OK;
}

NS_IMETHODIMP
nsScreenManager::GetSystemDefaultScale(float *aScale) {
  *aScale = 1.0f;
  return NS_OK;
}

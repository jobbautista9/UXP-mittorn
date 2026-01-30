/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsScreenManager.h"
#include "nsAppShell.h"
#include "nsIWidget.h"
#include <SDL.h>

NS_IMETHODIMP
nsSDLScreen::GetRect(int32_t *outX, int32_t *outY, int32_t *outWidth, int32_t *outHeight)
{
  return GetRectDisplayPix(outX, outY, outWidth, outHeight);
}

NS_IMETHODIMP
nsSDLScreen::GetAvailRect(int32_t *outX, int32_t *outY, int32_t *outWidth,  int32_t *outHeight)
{
  return GetAvailRectDisplayPix(outX, outY, outWidth, outHeight);
}

NS_IMETHODIMP
nsSDLScreen::GetRectDisplayPix(int32_t *outX, int32_t *outY, int32_t *outWidth, int32_t *outHeight)
{
  const nsIntRect &rect = mRect;
  *outX = rect.x;
  *outY = rect.y;
  *outWidth = rect.width;
  *outHeight = rect.height;
  return NS_OK;
}

NS_IMETHODIMP
nsSDLScreen::GetAvailRectDisplayPix(int32_t *outX, int32_t *outY, int32_t *outWidth, int32_t *outHeight)
{
  const nsIntRect &rect = mAvailRect;
  *outX = rect.x;
  *outY = rect.y;
  *outWidth = rect.width;
  *outHeight = rect.height;

  return NS_OK;
}

NS_IMETHODIMP
nsSDLScreen::GetPixelDepth(int32_t *aPixelDepth)
{
  // Close enough.
  *aPixelDepth = 24;
  return NS_OK;
}

NS_IMETHODIMP
nsSDLScreen::GetColorDepth(int32_t *aColorDepth)
{
  return GetPixelDepth(aColorDepth);
}

NS_IMETHODIMP
nsSDLScreen::GetContentsScaleFactor(double *aContentsScaleFactor)
{
  *aContentsScaleFactor = 1.0;
  return NS_OK;
}

NS_IMPL_ISUPPORTS(nsScreenManager, nsIScreenManager)

nsScreenManager::nsScreenManager()
{
  SDL_Init(SDL_INIT_VIDEO);
  EnsureInit();
}

NS_IMETHODIMP
nsScreenManager::GetPrimaryScreen(nsIScreen **outScreen)
{
  EnsureInit();

  // detect which screen is pointed by mouse
  if (mPrimary < 0 || mPrimary >= mNumScreens) {
    SDL_Point point = {};
    SDL_GetGlobalMouseState(&point.x, &point.y);
    mPrimary = SDL_GetPointDisplayIndex(&point);
  }

  NS_IF_ADDREF(*outScreen = mCachedScreenArray[mPrimary]);

  return NS_OK;
}

NS_IMETHODIMP
nsScreenManager::ScreenForRect(int32_t inLeft, int32_t inTop, int32_t inWidth, int32_t inHeight, nsIScreen **outScreen)
{
  EnsureInit();

  SDL_Rect rect = {inLeft, inTop, inWidth, inHeight};
  int index = SDL_GetRectDisplayIndex(&rect);
  NS_IF_ADDREF(*outScreen = mCachedScreenArray[index]);
  return NS_OK;
}

NS_IMETHODIMP
nsScreenManager::ScreenForId(uint32_t id, nsIScreen **outScreen)
{
  EnsureInit();

  NS_IF_ADDREF(*outScreen = mCachedScreenArray[id]);

  return NS_OK;
}

nsresult nsScreenManager::EnsureInit()
{
  if (!mNumScreens)
    return UpdateScreens();
  return NS_OK;
}

nsresult nsScreenManager::UpdateScreens()
{
  int numDisplays = SDL_GetNumVideoDisplays();
  int oldCount = mCachedScreenArray.Count();
  for (int i = 0; i < numDisplays; i++) {
    RefPtr<nsSDLScreen> screen;
    if (oldCount <= i) {
      screen = new nsSDLScreen();
      if (!screen || !mCachedScreenArray.AppendObject(screen)) {
        return NS_ERROR_OUT_OF_MEMORY;
      }
    } else {
      screen = (nsSDLScreen *)mCachedScreenArray[i];
    }
    screen->mIndex = i;
    SDL_GetDisplayBounds(i, (SDL_Rect *)&screen->mRect);
    SDL_GetDisplayUsableBounds(i, (SDL_Rect *)&screen->mAvailRect);
  }
  mNumScreens = numDisplays;
  return NS_OK;
}

NS_IMETHODIMP
nsScreenManager::ScreenForNativeWidget(void *aWidget, nsIScreen **outScreen)
{
  SDL_Window *win = nullptr;
  EnsureInit();

  // for some reason, native window, not widget passed, so find the widget
  for (int i = 0; i < nsAppShell::gAppShell->mTopLevelWindows.Length(); i++) {
    nsIWidget *nsWin = nsAppShell::gAppShell->mTopLevelWindows[i];
    if (nsWin->GetNativeData(NS_NATIVE_WINDOW) == aWidget) {
      win = (SDL_Window *)nsWin->GetNativeData(NS_NATIVE_WIDGET);
      break;
    }
  }
  if (!win) {
    return NS_ERROR_FAILURE;
  }
  int index = SDL_GetWindowDisplayIndex(win);
  NS_IF_ADDREF(*outScreen = mCachedScreenArray[index]);
  return NS_OK;
}

NS_IMETHODIMP
nsScreenManager::GetNumberOfScreens(uint32_t *aNumberOfScreens)
{
  EnsureInit();
  *aNumberOfScreens = mNumScreens;
  return NS_OK;
}

NS_IMETHODIMP
nsScreenManager::GetSystemDefaultScale(float *aScale)
{
  *aScale = 1.0f;
  return NS_OK;
}

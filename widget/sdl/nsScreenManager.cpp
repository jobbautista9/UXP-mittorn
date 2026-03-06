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
nsSDLScreen::GetRotation(uint32_t *aRotation)
{
  *aRotation = mRotation;
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
NS_IMPL_ISUPPORTS(nsSDLScreen, nsIScreen)

nsScreenManager::nsScreenManager()
 : mUpdateMutex("nsScreenManager.UpdateMutex")
{
}

NS_IMETHODIMP
nsScreenManager::GetPrimaryScreen(nsIScreen **outScreen)
{
  mozilla::MutexAutoLock lock(mUpdateMutex);
  if (mPrimary < 0 || mPrimary >= mNumScreens) {
    return NS_ERROR_NOT_AVAILABLE;
  }

  NS_IF_ADDREF(*outScreen = mCachedScreenArray[mPrimary]);

  return NS_OK;
}

NS_IMETHODIMP
nsScreenManager::ScreenForRect(int32_t inLeft, int32_t inTop, int32_t inWidth, int32_t inHeight, nsIScreen **outScreen)
{
  mozilla::MutexAutoLock lock(mUpdateMutex);

  SDL_Rect rect = {inLeft, inTop, inWidth, inHeight};
  int index = SDL_GetRectDisplayIndex(&rect);
  NS_IF_ADDREF(*outScreen = mCachedScreenArray[index]);
  return NS_OK;
}

NS_IMETHODIMP
nsScreenManager::ScreenForId(uint32_t id, nsIScreen **outScreen)
{
  mozilla::MutexAutoLock lock(mUpdateMutex);
  NS_IF_ADDREF(*outScreen = mCachedScreenArray[id]);

  return NS_OK;
}

void nsScreenManager::UpdateScreens()
{
  mozilla::MutexAutoLock lock(mUpdateMutex);
  int numDisplays = SDL_GetNumVideoDisplays();
  int oldCount = mCachedScreenArray.Count();

  for (int i = 0; i < numDisplays; i++) {
    RefPtr<nsSDLScreen> screen;
    if (oldCount <= i) {
      screen = new nsSDLScreen();
      if (!screen || !mCachedScreenArray.AppendObject(screen)) {
        mNumScreens = i;
        return;
      }
    } else {
      screen = (nsSDLScreen *)mCachedScreenArray[i];
    }
    screen->mIndex = i;
    SDL_GetDisplayBounds(i, (SDL_Rect *)&screen->mRect);
    SDL_GetDisplayUsableBounds(i, (SDL_Rect *)&screen->mAvailRect);
    SDL_DisplayOrientation orientation = SDL_GetDisplayOrientation(i);

    uint32_t oldRotation = screen->mRotation;

    switch(orientation) {
      case SDL_ORIENTATION_LANDSCAPE:
      case SDL_ORIENTATION_UNKNOWN:
        screen->mRotation = nsIScreen::ROTATION_0_DEG;
        break;
      case SDL_ORIENTATION_LANDSCAPE_FLIPPED:
        screen->mRotation = nsIScreen::ROTATION_180_DEG;
        break;
      case SDL_ORIENTATION_PORTRAIT:
        screen->mRotation = nsIScreen::ROTATION_90_DEG;
        break;
      case SDL_ORIENTATION_PORTRAIT_FLIPPED:
        screen->mRotation = nsIScreen::ROTATION_270_DEG;
        break;
    }

    if (screen->mRotation == oldRotation ) {
      continue;
    }
    for (int j = 0; j < nsAppShell::gAppShell->mTopLevelWindows.Length(); j++) {
      nsWindow *nsWin = (nsWindow *)nsAppShell::gAppShell->mTopLevelWindows[j];
      SDL_Window *win = (SDL_Window *)nsWin->GetNativeData(NS_NATIVE_WIDGET);
      if (win && SDL_GetWindowDisplayIndex(win) == i) {
        int rotation = screen->mRotation;
        nsWin->mDisplayIndex = i;
        NS_DispatchToMainThread(NS_NewRunnableFunction([nsWin, i, rotation]{
          nsWin->ScreenRotationChanged(i, rotation);
        }));
      }
    }
  }
  mNumScreens = numDisplays;
  // detect which screen is pointed by mouse
  if (mPrimary < 0 || mPrimary >= mNumScreens) {
    SDL_Point point = {};
    SDL_GetGlobalMouseState(&point.x, &point.y);
    mPrimary = SDL_GetPointDisplayIndex(&point);
  }
}

NS_IMETHODIMP
nsScreenManager::ScreenForNativeWidget(void *aWidget, nsIScreen **outScreen)
{
  mozilla::MutexAutoLock lock(mUpdateMutex);
  nsWindow *win = nullptr;

  // for some reason, native window, not widget passed, so find the widget
  for (int i = 0; i < nsAppShell::gAppShell->mTopLevelWindows.Length(); i++) {
    nsWindow *nsWin = nsAppShell::gAppShell->mTopLevelWindows[i];
    if (nsWin->GetNativeData(NS_NATIVE_WINDOW) == aWidget) {
      win = nsWin;
      break;
    }
  }
  if (!win) {
    return NS_ERROR_FAILURE;
  }
  int index = win->mDisplayIndex;
  NS_IF_ADDREF(*outScreen = mCachedScreenArray[index]);
  return NS_OK;
}

NS_IMETHODIMP
nsScreenManager::GetNumberOfScreens(uint32_t *aNumberOfScreens)
{
  mozilla::MutexAutoLock lock(mUpdateMutex);
  *aNumberOfScreens = mNumScreens;
  return NS_OK;
}

NS_IMETHODIMP
nsScreenManager::GetSystemDefaultScale(float *aScale)
{
  *aScale = 1.0f;
  return NS_OK;
}

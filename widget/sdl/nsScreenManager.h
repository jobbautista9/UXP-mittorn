/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsScreenManager_h_
#define nsScreenManager_h_

#include "nsBaseScreen.h"
#include "nsIScreenManager.h"
#include "nsCOMArray.h"
#include "mozilla/Mutex.h"
#include "nsRect.h"

class nsSDLScreen : public nsIScreen
{
  int mIndex;
  nsIntRect mRect;
  nsIntRect mAvailRect;
  uint32_t mRotation;
  NS_DECL_THREADSAFE_ISUPPORTS
  private:
  ~nsSDLScreen () {}

public:

  NS_IMETHOD GetId(uint32_t* outId) {
    *outId = mIndex;
    return NS_OK;
  }

  NS_IMETHOD GetRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
  NS_IMETHOD GetAvailRect(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
  NS_IMETHOD GetRectDisplayPix(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
  NS_IMETHOD GetAvailRectDisplayPix(int32_t* aLeft, int32_t* aTop, int32_t* aWidth, int32_t* aHeight);
  NS_IMETHOD GetPixelDepth(int32_t* aPixelDepth);
  NS_IMETHOD GetColorDepth(int32_t* aColorDepth);
  NS_IMETHOD GetContentsScaleFactor(double* aContentsScaleFactor);
  NS_IMETHOD GetDefaultCSSScaleFactor(double* aScaleFactor)
  {
    return GetContentsScaleFactor(aScaleFactor);
  }
  NS_IMETHOD GetRotation(uint32_t* aRotation);
  NS_IMETHOD LockMinimumBrightness(uint32_t brightness) { return NS_OK; }
  NS_IMETHOD UnlockMinimumBrightness(uint32_t brightness) { return NS_OK; }
  NS_IMETHOD SetRotation(uint32_t aRotation) { return NS_ERROR_NOT_AVAILABLE; }

  friend class nsScreenManager;
};

class nsScreenManager : public nsIScreenManager
{
public:
  nsScreenManager ();

  NS_DECL_THREADSAFE_ISUPPORTS

  NS_DECL_NSISCREENMANAGER

  void UpdateScreens();

private:
  virtual ~nsScreenManager () {}

  nsCOMArray<nsIScreen> mCachedScreenArray;
  int mNumScreens = 0;
  int mPrimary = -1;
  mozilla::Mutex mUpdateMutex;
};

#endif // nsScreenManager_h_

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Dummy app shell, does not process any events yet
 */

#ifndef nsAppShell_h_
#define nsAppShell_h_

#include "mozilla/Mutex.h"
#include "nsBaseAppShell.h"
#include "nsWindow.h"
#include <SDL.h>

// global window invalidation list
struct InvalidWindow {
  mozilla::LayoutDeviceIntRegion region;
  nsWindow *window;
};

class nsAppShell : public nsBaseAppShell {
public:
  NS_IMETHOD ResumeNative(void);

  nsAppShell();

  nsresult Init();

  NS_IMETHOD Exit(void);

  static nsAppShell *gAppShell;
  // resize events may destroy SDL surface, so lock this during
  // rendering/resize/PumpEvents
  mozilla::Mutex mEventsMutex;
  nsTArray<InvalidWindow> mInvalidWindows;
  nsTArray<nsWindow *> mTopLevelWindows;

  static inline int GetForcedFullscreen() {
    return sForcedFullscreen;
  }

protected:
  virtual ~nsAppShell();

  virtual void ScheduleNativeEventCallback();

public:
  virtual bool ProcessNextNativeEvent(bool aMayWait);

protected:
  static int sForcedFullscreen;
  bool mTerminated;

private:
  void HandleSDLEvent(const SDL_Event *ev);
  int PollSDLEvent(SDL_Event *ev);
};

#endif // nsAppShell_h_

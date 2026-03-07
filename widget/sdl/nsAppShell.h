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
#include "mozilla/CondVar.h"
#include "mozilla/SyncRunnable.h"
#include "nsBaseAppShell.h"
#include "nsScreenManager.h"
#include "nsWindow.h"
#include <SDL.h>

// user sdl events (wakeup thread, running SDL_WaitEvent
#define APPSHELL_SDL_NOTIFY_NATICE_CB 1
#define APPSHELL_SDL_NOTIFY_UI 2
#define APPSHELL_SDL_RUNNABLE_CB 3


// global window invalidation list
struct InvalidWindow {
  mozilla::LayoutDeviceIntRegion region;
  nsWindow *window;
};

struct EventListElement : mozilla::LinkedListElement<EventListElement>
{
  nsWindow *mWindow = nullptr;
  SDL_Event mEvent = {};

  void PostTo(mozilla::LinkedList<EventListElement>& aQueue)
  {
    aQueue.insertBack(this);
  }
};

class nsAppShell : public nsBaseAppShell {
public:
  NS_IMETHOD ResumeNative(void) override;

  nsAppShell();

  nsresult Init();

  NS_IMETHOD Exit(void) override;
  NS_IMETHOD Run(void) override;

  static nsAppShell *gAppShell;
  // resize events may destroy SDL surface, so lock this during
  // rendering/resize/PumpEvents
  mozilla::Mutex mEventsMutex;
  nsTArray<InvalidWindow> mInvalidWindows;
  nsTArray<nsWindow *> mTopLevelWindows;
  nsTArray<SDL_Window *> mDestroyedWindows;

  nsCOMPtr<nsIThread> mSDLEventsThread;
  MessageLoop* mSDLEventsLoop;
  mozilla::Mutex mNotifyMutex;
  mozilla::CondVar mNotifyCondVar;
  mozilla::LinkedList<EventListElement> mQueue;
  RefPtr<nsScreenManager> mScreenManager;
  void *mPlatformDisplay;
  uint mNativePlatform;
  unsigned short mNativeEventCount;

  bool mSeparateEventThread;
  bool mEmulateTouchscreen;
  bool mNotify;
  bool mTerminated;
  void NotifyEvent();
  void EnsureInit();

  static inline int GetForcedFullscreen() {
    return sForcedFullscreen;
  }

  // post to main thread in events order or just call function
  template <typename func>
  static inline bool PostToMainThread(SDL_Event *aEv, func&& aCb) {
    if (gAppShell->mSeparateEventThread) {
      aEv->type = SDL_USEREVENT;
      aEv->user.code = APPSHELL_SDL_RUNNABLE_CB;
      mozilla::Runnable *r = new mozilla::detail::RunnableFunction
        // Make sure we store a non-reference in nsRunnableFunction.
        <typename mozilla::RemoveReference<func>::Type>
        // But still forward aFunction to move if possible.
        (mozilla::Forward<func>(aCb));
      r->AddRef();
      aEv->user.data1 = r;

     // ev->user.data1 = NS_NewRunnableFunction(cb);
    } else {
      aCb();
    }

    return true;
  }

  // dispatch to sdl thread (lighter SyncRunnable)
  template <typename func>
  static inline void DispatchToSDLThread(func&& aCb, bool aWait = true) {
    if (gAppShell->mSeparateEventThread) {
      if (aWait) {
        mozilla::SyncRunnable::DispatchToThread(gAppShell->mSDLEventsThread, (new mozilla::detail::RunnableFunction
                                                                              // Make sure we store a non-reference in nsRunnableFunction.
                                                                              <typename mozilla::RemoveReference<func>::Type>
                                                                              // But still forward aFunction to move if possible.
                                                                              (mozilla::Forward<func>(aCb))));
      } else {
        nsresult rv = gAppShell->mSDLEventsThread->Dispatch(new mozilla::detail::RunnableFunction
                                            // Make sure we store a non-reference in nsRunnableFunction.
                                            <typename mozilla::RemoveReference<func>::Type>
                                            // But still forward aFunction to move if possible.
                                            (mozilla::Forward<func>(aCb)), NS_DISPATCH_NORMAL);
        MOZ_ASSERT(!NS_FAILED(rv));
      }
    } else {
      aCb();
    }
  }

protected:
  virtual ~nsAppShell();

  virtual void ScheduleNativeEventCallback();

public:
  virtual bool ProcessNextNativeEvent(bool aMayWait);

protected:
  static int sForcedFullscreen;

private:
  friend class SDLEventRunnable;
  int PollSDLEvent(SDL_Event *ev);
};

#endif // nsAppShell_h_

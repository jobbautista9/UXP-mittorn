/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prenv.h"
#include "nsAppShell.h"
#include "nsScreenManager.h"
#include "nsWindow.h"
#include "mozilla/Preferences.h"

nsAppShell *nsAppShell::gAppShell = NULL;
int nsAppShell::sForcedFullscreen = -1;

// SDL helpers

// user sdl events (wakeup thread, running SDL_WaitEvent
#define APPSHELL_SDL_NOTIFY_NATICE_CB 1
#define APPSHELL_SDL_NOTIFY_UI 2

static void NotifySDLThread(int code) {
  SDL_Event user_event;
  SDL_zero(user_event); /* SDL will copy this entire struct! Initialize to keep
                           memory checkers happy. */
  user_event.type = SDL_USEREVENT;
  user_event.user.code = code;
  user_event.user.data1 = NULL;
  user_event.user.data2 = NULL;
  SDL_PushEvent(&user_event);
}

static void InitSDL()
{
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
  // prevent SDL from taking over opengl context,
  // without this WebGL breaks on basic layers
  SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "software");
  // workaround to allow other windows receive events during drag
  SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
  // allow ui interaction when accepting focus
  SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
  // we always need simulated mouse events
  SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "1");
}

class SDLEventRunnable : public mozilla::Runnable{
public:
  SDL_Event mEvent;
  SDLEventRunnable(const SDL_Event &aEv)
    : mEvent(aEv) {}
  NS_DECL_NSIRUNNABLE
};

class SDLThread : public mozilla::Runnable{
  public:
    NS_DECL_NSIRUNNABLE
};

NS_IMETHODIMP SDLThread::Run() {
  // from sdl docs (SDL_PumpEvents):
  // WARNING: This should only be run in the thread that initialized
  // the video subsystem, and for extra safety, you should consider
  // only doing those things on the main thread in any case.

  // while it's unclear, what "main thread" mean,
  // it's should be allowed to init video and events in special thread

  // TODO: Some video calls are still done in main thread,
  // which is likely to be UB and need be changed
  InitSDL();

  mozilla::UniquePtr<EventListElement> ev;
  ev.reset(new EventListElement);
  nsAppShell::gAppShell->NotifyEvent();
  while(true) {
    bool ret;
    nsAppShell::gAppShell->mSDLEventsThread->ProcessNextEvent(false, &ret);
    SDL_WaitEvent(&ev->mEvent);
    {
      mozilla::MutexAutoLock lock(nsAppShell::gAppShell->mNotifyMutex);
      if (nsAppShell::gAppShell->mTerminated) {
        break;
      }
      ev->PostTo(nsAppShell::gAppShell->mQueue);
      (void) ev.release();
      ev.reset(new EventListElement);
      while (SDL_PollEvent(&ev->mEvent)) {
        ev->PostTo(nsAppShell::gAppShell->mQueue);
        (void) ev.release();
        ev.reset(new EventListElement);
      }
      nsAppShell::gAppShell->mNotify = true;
      nsAppShell::gAppShell->mNotifyCondVar.Notify();
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAppShell::ResumeNative(void) {
  return nsBaseAppShell::ResumeNative();
}

NS_IMETHODIMP SDLEventRunnable::Run() {
  nsAppShell::gAppShell->HandleSDLEvent(&mEvent);
  return NS_OK;
}

nsAppShell::nsAppShell()
  : mEventsMutex("SDL_Event mutex")
  , mNotifyMutex("AppShell.NotifyMutex")
  , mNotifyCondVar(mNotifyMutex, "AppShell.NotifyCondVar")
  , mNativeEventCount(0)
  , mNotify(false)
  , mTerminated(false) {
  gAppShell = this;
}

nsAppShell::~nsAppShell() { gAppShell = NULL; }

// Init
//
// public
nsresult nsAppShell::Init() {

  const char *env = PR_GetEnv("MOZ_SDL_FORCE_FULLSCREEN");
  if (env) {
    sForcedFullscreen = atoi(env);
  } else {
    mozilla::Preferences::AddIntVarCache(&sForcedFullscreen, "widget.sdl.forcedFullscreen");
  }
  env = PR_GetEnv("MOZ_SDL_EVENT_THREAD");
  if (env) {
    mSeparateEventThread = atoi(env);
  }
  if (mSeparateEventThread) {
    mozilla::MutexAutoLock lock(mNotifyMutex);
    nsCOMPtr<nsIRunnable> sdlThread = new SDLThread();
    NS_NewThread(getter_AddRefs(mSDLEventsThread), sdlThread);

    while (!mNotify) {
      mNotifyCondVar.Wait();
    }

    mNotify = false;
  } else {
    InitSDL();
  }

  return nsBaseAppShell::Init();
}

void nsAppShell::HandleSDLEvent(const SDL_Event *ev) {
  if (ev->type == SDL_USEREVENT && ev->user.code == APPSHELL_SDL_NOTIFY_NATICE_CB) {
    NativeEventCallback();
  }
  if (MOZ_UNLIKELY(ev->type == SDL_DISPLAYEVENT)) {
    nsCOMPtr<nsIScreenManager> manager =
        do_GetService("@mozilla.org/gfx/screenmanager;1");
    nsScreenManager *screenManager =
        (nsScreenManager *)(nsIScreenManager *)manager;
    screenManager->UpdateScreens();
  }
  nsWindow::OnGlobalEvent(ev);
}

int nsAppShell::PollSDLEvent(SDL_Event *ev) {
  mEventsMutex.Lock();
  int ret = SDL_PollEvent(ev);
  mEventsMutex.Unlock();
  return ret;
}

// ProcessNextNativeEvent
//
// protected virtual
bool nsAppShell::ProcessNextNativeEvent(bool aMayWait) {
  if (mTerminated) {
    return false;
  }

  // fast path for LAYERS_BASIC (before waiting event)
  nsWindow::HandleInvalidWindows();
  if (mSeparateEventThread) {
    bool ret = false;
    // we do not have any platform events in this thread. so only wait on CondVar
    mNotifyMutex.Lock();

    while (aMayWait && !mNotify) {
      mNotifyCondVar.Wait();
    }
    mNotify = false;

    unsigned short cnt = mNativeEventCount;
    mNativeEventCount = 0;
    mozilla::LinkedList<EventListElement> queue = Move(mQueue);
    mNotifyMutex.Unlock();

    while (cnt--) {
      NativeEventCallback();
      ret = true;
    }
    while (!queue.isEmpty()) {
      mozilla::UniquePtr<EventListElement> ev(queue.popFirst());
      HandleSDLEvent(&ev->mEvent);
      ret = true;
    }
    return ret;
  }

  int (*evFunc)(SDL_Event *) = aMayWait ? SDL_WaitEvent : SDL_PollEvent;

  SDL_Event ev;
  mEventsMutex.Lock();
  int ret = aMayWait ? SDL_WaitEvent(&ev) : SDL_PollEvent(&ev);
  mEventsMutex.Unlock();

  if (!ret) {
    return false;
  }

  do {
    HandleSDLEvent(&ev);
  } while (PollSDLEvent(&ev));
  // handle invalidated during SDL_WINDOWEVENT_EXPOSE
  nsWindow::HandleInvalidWindows();
  return true;
}

NS_IMETHODIMP
nsAppShell::Exit(void) {
  if (mTerminated) {
    return NS_OK;
  }

  mTerminated = true;
  if (mSeparateEventThread) {
    NotifySDLThread(APPSHELL_SDL_NOTIFY_UI);
  }
  return nsBaseAppShell::Exit();
}

// public
void nsAppShell::NotifyEvent()
{
  MOZ_ASSERT(mSeparateEventThread);
  mozilla::MutexAutoLock lock(mNotifyMutex);

  mNotify = true;
  mNotifyCondVar.Notify();
}

// ScheduleNativeEventCallback
//
// protected virtual
void nsAppShell::ScheduleNativeEventCallback() {
  if (mSeparateEventThread) {
    mozilla::MutexAutoLock lock(mNotifyMutex);

    mNativeEventCount++;
    mNotify = true;
    mNotifyCondVar.Notify();
    return;
  }
  // generate SDL_Event to wake up thread
  NotifySDLThread(APPSHELL_SDL_NOTIFY_NATICE_CB);
}

// Used by IPC code
namespace mozilla {
// ui pump does not seems to be used now, but implement it just in case
bool ProcessNextEvent() {
  return nsAppShell::gAppShell->ProcessNextNativeEvent(true) ? true : false;
}

void NotifyEvent() {
  if (nsAppShell::gAppShell->mSeparateEventThread) {
    nsAppShell::gAppShell->NotifyEvent();
    return;
  }
  // notify nsAppShell thread from UI pump
  NotifySDLThread(APPSHELL_SDL_NOTIFY_UI);
}

} // namespace mozilla

/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "prenv.h"
#include "nsAppShell.h"
#include "nsScreenManager.h"
#include "nsWindow.h"
#include "mozilla/Preferences.h"
#include "base/message_loop.h"
#include "nsThread.h"
#include "nsThreadSyncDispatch.h"

nsAppShell *nsAppShell::gAppShell = NULL;
int nsAppShell::sForcedFullscreen = -1;

// SDL helpers



static void NotifySDLThread(int code) {
  mozilla::MutexAutoLock lock(nsAppShell::gAppShell->mWakeupMutex);
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
  if (nsAppShell::gAppShell->mEmulateTouchscreen) {
    SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "1");
  }

  nsAppShell::gAppShell->mScreenManager->UpdateScreens();
}


static void HandleScreenEvents(const SDL_Event *aEv) {
  if (MOZ_UNLIKELY(aEv->type == SDL_DISPLAYEVENT)) {
    nsAppShell::gAppShell->mScreenManager->UpdateScreens();
  }
}
static volatile bool sThreadShutdown;
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
  nsAppShell::gAppShell->mSDLEventsLoop = MessageLoop::current();
  nsAppShell::gAppShell->NotifyEvent();
  while(true) {
    bool ret = true, hasEvents = true;
    while (ret || hasEvents) {
      nsAppShell::gAppShell->mSDLEventsThread->ProcessNextEvent(false, &ret);
      nsAppShell::gAppShell->mSDLEventsThread->HasPendingEvents(&hasEvents);
    }
    if(sThreadShutdown)
      break;
    SDL_WaitEvent(&ev->mEvent);
    ret = hasEvents = true;
    while (ret || hasEvents) {
      nsAppShell::gAppShell->mSDLEventsThread->ProcessNextEvent(false, &ret);
      nsAppShell::gAppShell->mSDLEventsThread->HasPendingEvents(&hasEvents);
    }

    HandleScreenEvents(&ev->mEvent);
    if (nsWindow::PreprocessEvent(&ev->mEvent, &ev->mWindow)) {
      mozilla::MutexAutoLock lock(nsAppShell::gAppShell->mNotifyMutex);
      if (nsAppShell::gAppShell->mTerminated) {
        break;
      }
      ev->PostTo(nsAppShell::gAppShell->mQueue);
      (void) ev.release();
      ev.reset(new EventListElement);
    }
    while (SDL_PollEvent(&ev->mEvent)) {
      HandleScreenEvents(&ev->mEvent);
      if (nsWindow::PreprocessEvent(&ev->mEvent, &ev->mWindow)) {
        mozilla::MutexAutoLock lock(nsAppShell::gAppShell->mNotifyMutex);
        if (nsAppShell::gAppShell->mTerminated) {
          break;
        }
        ev->PostTo(nsAppShell::gAppShell->mQueue);
        (void) ev.release();
        ev.reset(new EventListElement);
      }
    }
    nsAppShell::gAppShell->NotifyEvent();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsAppShell::ResumeNative(void) {
  return nsBaseAppShell::ResumeNative();
}

nsAppShell::nsAppShell()
  : mEventsMutex("AppShell.EventsMutex")
  , mWakeupMutex("AppShell.WakeupMutex")
  , mNotifyMutex("AppShell.NotifyMutex")
  , mNotifyCondVar(mNotifyMutex, "AppShell.NotifyCondVar")
  , mNativeEventCount(0)
  , mSeparateEventThread(false)
  , mEmulateTouchscreen(false)
  , mNotify(false)
  , mTerminated(false) {
  gAppShell = this;

}

nsAppShell::~nsAppShell() { gAppShell = NULL; }

/* wrapper to wake thread with NotifySDLThread */
class SDLEventsThread: public nsThread {
  bool mShutdown;
public:
  SDLEventsThread() : nsThread(nsThread::NOT_MAIN_THREAD, nsIThreadManager::DEFAULT_STACK_SIZE) {
    nsCOMPtr<nsIRunnable> runnable(new SDLThread());
    Init();
    nsThread::Dispatch(runnable, NS_DISPATCH_NORMAL);
  }
  NS_IMETHODIMP
  Dispatch(already_AddRefed<nsIRunnable> aRunnable, uint32_t aFlags) override {
    if (aFlags == NS_DISPATCH_SYNC) {
      nsIThread* thread;
      nsresult rv =  NS_GetCurrentThread(&thread);
      if (NS_FAILED(rv) || NS_WARN_IF(!thread)) {
        return NS_ERROR_NOT_AVAILABLE;
      }
      RefPtr<nsThreadSyncDispatch> wrapper = new nsThreadSyncDispatch(thread, mozilla::Move(aRunnable));
      nsresult ret = nsThread::Dispatch(wrapper, NS_DISPATCH_NORMAL);
      if (NS_FAILED(ret)) {
        // PutEvent leaked the wrapper runnable object on failure, so we
        // explicitly release this object once for that. Note that this
        // object will be released again soon because it exits the scope.
        wrapper.get()->Release();

      }
      while (wrapper->IsPending()) {
        NotifySDLThread(APPSHELL_SDL_NOTIFY_UI);
        NS_ProcessNextEvent(thread, true);
      }
      return ret;
    }
    nsresult ret = nsThread::Dispatch(mozilla::Move(aRunnable), aFlags);
    NotifySDLThread(APPSHELL_SDL_NOTIFY_UI);
    return ret;
  }
  NS_IMETHODIMP Shutdown() override {
    sThreadShutdown = true;
    NotifySDLThread(APPSHELL_SDL_NOTIFY_UI);
    return nsThread::Shutdown();
  }
};

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
  env = PR_GetEnv("MOZ_SDL_EMULATE_TOUCHSCREEN");
  if (env) {
    mEmulateTouchscreen = atoi(env);
  }

  return nsBaseAppShell::Init();
}

// cannot run do_GetService in constructor, so this is called later when creating any window or in Run()
void nsAppShell::EnsureInit(){
  MOZ_ASSERT(NS_IsMainThread());
  if (mScreenManager) {
    return;
  }
  nsCOMPtr<nsIScreenManager> ptr = do_GetService("@mozilla.org/gfx/screenmanager;1");
  mScreenManager = (nsScreenManager*)(nsIScreenManager *)ptr;
  if (mSeparateEventThread) {
    mozilla::MutexAutoLock lock(mNotifyMutex);
    mSDLEventsThread = new SDLEventsThread();

    while (!mNotify) {
      mNotifyCondVar.Wait();
    }

    mNotify = false;
  } else {
    InitSDL();
    // some events might be missed before SDL_Init, reschedule
    NotifySDLThread(APPSHELL_SDL_NOTIFY_NATICE_CB);
  }
}


NS_IMETHODIMP nsAppShell::Run() {
  EnsureInit();
  return nsBaseAppShell::Run();
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
      if (ev->mWindow) {
        ev->mWindow->OnEvent(&ev->mEvent);
      }
      else if (ev->mEvent.type == SDL_USEREVENT && ev->mEvent.user.code == APPSHELL_SDL_RUNNABLE_CB) {
        nsIRunnable *runnable = (nsIRunnable*)ev->mEvent.user.data1;
        runnable->Run();
        runnable->Release();
      }
      ret = true;
    }

    nsWindow::HandleInvalidWindows();
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
    HandleScreenEvents(&ev);
    if (ev.type == SDL_USEREVENT && ev.user.code == APPSHELL_SDL_NOTIFY_NATICE_CB) {
      NativeEventCallback();
    } else {
      nsWindow *win = nullptr;
      if (nsWindow::PreprocessEvent(&ev, &win) && win) {
        win->OnEvent(&ev);
      }
    }
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
  // if mScreenManager is null, SDL_Init was not called
  if (mScreenManager) {
    NotifySDLThread(APPSHELL_SDL_NOTIFY_NATICE_CB);
  }
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

SDL_Window* CreateOffscreenSDLWindow() {
  SDL_Window *ret = nullptr;
  nsAppShell::DispatchToSDLThread([&ret] {
   ret = SDL_CreateWindow("GLContextSDL", 0, 0, 100, 100, SDL_WINDOW_HIDDEN | SDL_WINDOW_OPENGL);
  });
  return ret;
}

void DestroyOffscreenSDLWindow(SDL_Window *aWin) {
  nsAppShell::DispatchToSDLThread([aWin] {
    nsAppShell::gAppShell->mDestroyedWindows.AppendElement(aWin);
  }, false);
}

} // namespace mozilla

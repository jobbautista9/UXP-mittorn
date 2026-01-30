/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAppShell.h"
#include "nsScreenManager.h"
#include "nsWindow.h"

nsAppShell *nsAppShell::gAppShell = NULL;

// user sdl events
#define APPSHELL_SDL_NOTIFY_NATICE_CB 1
#define APPSHELL_SDL_NOTIFY_UI 2

NS_IMETHODIMP
nsAppShell::ResumeNative(void) {
  return nsBaseAppShell::ResumeNative();
}

nsAppShell::nsAppShell()
  : mEventsMutex("SDL_Event mutex")
  , mTerminated(false) {
  gAppShell = this;
}

nsAppShell::~nsAppShell() { gAppShell = NULL; }

// Init
//
// public
nsresult nsAppShell::Init() {
  SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
  // prevent SDL from taking over opengl context,
  // without this WebGL breaks on basic layers
  SDL_SetHint(SDL_HINT_FRAMEBUFFER_ACCELERATION, "software");
  // workaround to allow other windows receive events during drag
  SDL_SetHint(SDL_HINT_MOUSE_AUTO_CAPTURE, "0");
  // allow ui interaction when accepting focus
  SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
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
  return nsBaseAppShell::Exit();
}

// ScheduleNativeEventCallback
//
// protected virtual
void nsAppShell::ScheduleNativeEventCallback() {
  // generate SDL_Event to wake up thread
  SDL_Event user_event;
  SDL_zero(user_event); /* SDL will copy this entire struct! Initialize to keep
                           memory checkers happy. */
  user_event.type = SDL_USEREVENT;
  user_event.user.code = APPSHELL_SDL_NOTIFY_NATICE_CB;
  user_event.user.data1 = NULL;
  user_event.user.data2 = NULL;
  SDL_PushEvent(&user_event);
}

// Used by IPC code
namespace mozilla {
// ui pump does not seems to be used now, but implement it just in case
bool ProcessNextEvent() {
  return nsAppShell::gAppShell->ProcessNextNativeEvent(true) ? true : false;
}

void NotifyEvent() {
  // notify nsAppShell thread from UI pump
  SDL_Event user_event;
  SDL_zero(user_event); /* SDL will copy this entire struct! Initialize to keep
                           memory checkers happy. */
  user_event.type = SDL_USEREVENT;
  user_event.user.code = APPSHELL_SDL_NOTIFY_UI;
  user_event.user.data1 = NULL;
  user_event.user.data2 = NULL;
  SDL_PushEvent(&user_event);
}

} // namespace mozilla

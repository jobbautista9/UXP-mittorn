/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsAppShell.h"
nsAppShell *nsAppShell::gAppShell = NULL;

#define ALOG(args...) fprintf(stderr, args); fprintf(stderr, "\n")


// nsAppShell implementation

NS_IMETHODIMP
nsAppShell::ResumeNative(void)
{
  return nsBaseAppShell::ResumeNative();
}

nsAppShell::nsAppShell()
  : mTerminated(false)
{
  gAppShell = this;
}

nsAppShell::~nsAppShell()
{
  gAppShell = NULL;
}

// Init
//
// public
nsresult
nsAppShell::Init()
{
  return nsBaseAppShell::Init();
}
// ScheduleNativeEventCallback
//
// protected virtual
void
nsAppShell::ScheduleNativeEventCallback()
{
}

// ProcessNextNativeEvent
//
// protected virtual
bool
nsAppShell::ProcessNextNativeEvent(bool aMayWait)
{
  if (mTerminated) {
    return false;
  }

  return false;
}

NS_IMETHODIMP
nsAppShell::Exit(void)
{
  if (mTerminated) {
    return NS_OK;
  }

  mTerminated = true;
  return nsBaseAppShell::Exit();
}


// Used by IPC code
namespace mozilla {

bool ProcessNextEvent()
{
    return nsAppShell::gAppShell->ProcessNextNativeEvent(true) ? true : false;
}

void NotifyEvent()
{
//    nsAppShell::gAppShell->NotifyNativeEvent();
}

}


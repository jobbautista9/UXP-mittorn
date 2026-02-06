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
#include "nsBaseAppShell.h"


class nsAppShell : public nsBaseAppShell
{
public:
  NS_IMETHOD ResumeNative(void);

  nsAppShell();

  nsresult Init();

  NS_IMETHOD Exit(void);

  static nsAppShell* gAppShell;
  void NotifyEvent();

protected:
  virtual ~nsAppShell();

  virtual void ScheduleNativeEventCallback();
public:
  virtual bool ProcessNextNativeEvent(bool aMayWait);
protected:
  mozilla::Mutex mNotifyMutex;
  mozilla::CondVar mNotifyCondVar;
  unsigned short mNativeEventCount;
  bool mNotify;
  bool mTerminated;
};

#endif // nsAppShell_h_

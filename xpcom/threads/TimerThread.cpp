/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2001
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <pavlov@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsTimerImpl.h"
#include "TimerThread.h"

#include "nsThreadUtils.h"
#include "pratom.h"

#include "nsIObserverService.h"
#include "nsIServiceManager.h"
#include "nsIProxyObjectManager.h"
#include "mozilla/Services.h"

#include <math.h>

using namespace mozilla;

NS_IMPL_THREADSAFE_ISUPPORTS2(TimerThread, nsIRunnable, nsIObserver)

TimerThread::TimerThread() :
  mInitInProgress(0),
  mInitialized(PR_FALSE),
  mMonitor("TimerThread.mMonitor"),
  mShutdown(PR_FALSE),
  mWaiting(PR_FALSE),
  mSleeping(PR_FALSE),
  mDelayLineCounter(0),
  mMinTimerPeriod(0)
{
}

TimerThread::~TimerThread()
{
  mThread = nsnull;

  NS_ASSERTION(mTimers.IsEmpty(), "Timers remain in TimerThread::~TimerThread");
}

nsresult
TimerThread::InitLocks()
{
  return NS_OK;
}

nsresult TimerThread::Init()
{
  PR_LOG(gTimerLog, PR_LOG_DEBUG, ("TimerThread::Init [%d]\n", mInitialized));

  if (mInitialized) {
    if (!mThread)
      return NS_ERROR_FAILURE;

    return NS_OK;
  }

  if (PR_ATOMIC_SET(&mInitInProgress, 1) == 0) {
    // We hold on to mThread to keep the thread alive.
    nsresult rv = NS_NewThread(getter_AddRefs(mThread), this);
    if (NS_FAILED(rv)) {
      mThread = nsnull;
    }
    else {
      nsCOMPtr<nsIObserverService> observerService =
          mozilla::services::GetObserverService();
      // We must not use the observer service from a background thread!
      if (observerService && !NS_IsMainThread()) {
        nsCOMPtr<nsIObserverService> result = nsnull;
        NS_GetProxyForObject(NS_PROXY_TO_MAIN_THREAD,
                             NS_GET_IID(nsIObserverService),
                             observerService, NS_PROXY_ASYNC,
                             getter_AddRefs(result));
        observerService.swap(result);
      }
      // We'll be released at xpcom shutdown
      if (observerService) {
        observerService->AddObserver(this, "sleep_notification", PR_FALSE);
        observerService->AddObserver(this, "wake_notification", PR_FALSE);
      }
    }

    {
      MonitorAutoLock lock(mMonitor);
      mInitialized = PR_TRUE;
      mMonitor.NotifyAll();
    }
  }
  else {
    MonitorAutoLock lock(mMonitor);
    while (!mInitialized) {
      mMonitor.Wait();
    }
  }

  if (!mThread)
    return NS_ERROR_FAILURE;

  return NS_OK;
}

nsresult TimerThread::Shutdown()
{
  PR_LOG(gTimerLog, PR_LOG_DEBUG, ("TimerThread::Shutdown begin\n"));

  if (!mThread)
    return NS_ERROR_NOT_INITIALIZED;

  nsTArray<nsTimerImpl*> timers;
  {   // lock scope
    MonitorAutoLock lock(mMonitor);

    mShutdown = PR_TRUE;

    // notify the cond var so that Run() can return
    if (mWaiting)
      mMonitor.Notify();

    // Need to copy content of mTimers array to a local array
    // because call to timers' ReleaseCallback() (and release its self)
    // must not be done under the lock. Destructor of a callback
    // might potentially call some code reentering the same lock
    // that leads to unexpected behavior or deadlock.
    // See bug 422472.
    timers.AppendElements(mTimers);
    mTimers.Clear();
  }

  PRUint32 timersCount = timers.Length();
  for (PRUint32 i = 0; i < timersCount; i++) {
    nsTimerImpl *timer = timers[i];
    timer->ReleaseCallback();
    ReleaseTimerInternal(timer);
  }

  mThread->Shutdown();    // wait for the thread to die

  PR_LOG(gTimerLog, PR_LOG_DEBUG, ("TimerThread::Shutdown end\n"));
  return NS_OK;
}

// Keep track of how early (positive slack) or late (negative slack) timers
// are running, and use the filtered slack number to adaptively estimate how
// early timers should fire to be "on time".
void TimerThread::UpdateFilter(PRUint32 aDelay, TimeStamp aTimeout,
                               TimeStamp aNow)
{
  TimeDuration slack = aTimeout - aNow;
  double smoothSlack = 0;
  PRUint32 i, filterLength;
  static TimeDuration kFilterFeedbackMaxTicks =
    TimeDuration::FromMilliseconds(FILTER_FEEDBACK_MAX);
  static TimeDuration kFilterFeedbackMinTicks =
    TimeDuration::FromMilliseconds(-FILTER_FEEDBACK_MAX);

  if (slack > kFilterFeedbackMaxTicks)
    slack = kFilterFeedbackMaxTicks;
  else if (slack < kFilterFeedbackMinTicks)
    slack = kFilterFeedbackMinTicks;

  mDelayLine[mDelayLineCounter & DELAY_LINE_LENGTH_MASK] =
    slack.ToMilliseconds();
  if (++mDelayLineCounter < DELAY_LINE_LENGTH) {
    // Startup mode: accumulate a full delay line before filtering.
    PR_ASSERT(mTimeoutAdjustment.ToSeconds() == 0);
    filterLength = 0;
  } else {
    // Past startup: compute number of filter taps based on mMinTimerPeriod.
    if (mMinTimerPeriod == 0) {
      mMinTimerPeriod = (aDelay != 0) ? aDelay : 1;
    } else if (aDelay != 0 && aDelay < mMinTimerPeriod) {
      mMinTimerPeriod = aDelay;
    }

    filterLength = (PRUint32) (FILTER_DURATION / mMinTimerPeriod);
    if (filterLength > DELAY_LINE_LENGTH)
      filterLength = DELAY_LINE_LENGTH;
    else if (filterLength < 4)
      filterLength = 4;

    for (i = 1; i <= filterLength; i++)
      smoothSlack += mDelayLine[(mDelayLineCounter-i) & DELAY_LINE_LENGTH_MASK];
    smoothSlack /= filterLength;

    // XXXbe do we need amplification?  hacking a fudge factor, need testing...
    mTimeoutAdjustment = TimeDuration::FromMilliseconds(smoothSlack * 1.5);
  }

#ifdef DEBUG_TIMERS
  PR_LOG(gTimerLog, PR_LOG_DEBUG,
         ("UpdateFilter: smoothSlack = %g, filterLength = %u\n",
          smoothSlack, filterLength));
#endif
}

/* void Run(); */
NS_IMETHODIMP TimerThread::Run()
{
  MonitorAutoLock lock(mMonitor);

  // We need to know how many microseconds give a positive PRIntervalTime. This
  // is platform-dependent, we calculate it at runtime now.
  // First we find a value such that PR_MicrosecondsToInterval(high) = 1
  PRInt32 low = 0, high = 1;
  while (PR_MicrosecondsToInterval(high) == 0)
    high <<= 1;
  // We now have
  //    PR_MicrosecondsToInterval(low)  = 0
  //    PR_MicrosecondsToInterval(high) = 1
  // and we can proceed to find the critical value using binary search
  while (high-low > 1) {
    PRInt32 mid = (high+low) >> 1;
    if (PR_MicrosecondsToInterval(mid) == 0)
      low = mid;
    else
      high = mid;
  }

  // Half of the amount of microseconds needed to get positive PRIntervalTime.
  // We use this to decide how to round our wait times later
  PRInt32 halfMicrosecondsIntervalResolution = high >> 1;

  while (!mShutdown) {
    // Have to use PRIntervalTime here, since PR_WaitCondVar takes it
    PRIntervalTime waitFor;

    if (mSleeping) {
      // Sleep for 0.1 seconds while not firing timers.
      waitFor = PR_MillisecondsToInterval(100);
    } else {
      waitFor = PR_INTERVAL_NO_TIMEOUT;
      TimeStamp now = TimeStamp::Now();
      nsTimerImpl *timer = nsnull;

      if (!mTimers.IsEmpty()) {
        timer = mTimers[0];

        if (now >= timer->mTimeout + mTimeoutAdjustment) {
    next:
          // NB: AddRef before the Release under RemoveTimerInternal to avoid
          // mRefCnt passing through zero, in case all other refs than the one
          // from mTimers have gone away (the last non-mTimers[i]-ref's Release
          // must be racing with us, blocked in gThread->RemoveTimer waiting
          // for TimerThread::mMonitor, under nsTimerImpl::Release.

          NS_ADDREF(timer);
          RemoveTimerInternal(timer);

          {
            // We release mMonitor around the Fire call to avoid deadlock.
            MonitorAutoUnlock unlock(mMonitor);

#ifdef DEBUG_TIMERS
            if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
              PR_LOG(gTimerLog, PR_LOG_DEBUG,
                     ("Timer thread woke up %fms from when it was supposed to\n",
                      fabs((now - timer->mTimeout).ToMilliseconds())));
            }
#endif

            // We are going to let the call to PostTimerEvent here handle the
            // release of the timer so that we don't end up releasing the timer
            // on the TimerThread instead of on the thread it targets.
            if (NS_FAILED(timer->PostTimerEvent())) {
              nsrefcnt rc;
              NS_RELEASE2(timer, rc);
            
              // The nsITimer interface requires that its users keep a reference
              // to the timers they use while those timers are initialized but
              // have not yet fired.  If this ever happens, it is a bug in the
              // code that created and used the timer.
              //
              // Further, note that this should never happen even with a
              // misbehaving user, because nsTimerImpl::Release checks for a
              // refcount of 1 with an armed timer (a timer whose only reference
              // is from the timer thread) and when it hits this will remove the
              // timer from the timer thread and thus destroy the last reference,
              // preventing this situation from occurring.
              NS_ASSERTION(rc != 0, "destroyed timer off its target thread!");
            }
            timer = nsnull;
          }

          if (mShutdown)
            break;

          // Update now, as PostTimerEvent plus the locking may have taken a
          // tick or two, and we may goto next below.
          now = TimeStamp::Now();
        }
      }

      if (!mTimers.IsEmpty()) {
        timer = mTimers[0];

        TimeStamp timeout = timer->mTimeout + mTimeoutAdjustment;

        // Don't wait at all (even for PR_INTERVAL_NO_WAIT) if the next timer
        // is due now or overdue.
        //
        // Note that we can only sleep for integer values of a certain
        // resolution. We use halfMicrosecondsIntervalResolution, calculated
        // before, to do the optimal rounding (i.e., of how to decide what
        // interval is so small we should not wait at all).
        double microseconds = (timeout - now).ToMilliseconds()*1000;
        if (microseconds < halfMicrosecondsIntervalResolution)
          goto next; // round down; execute event now
        waitFor = PR_MicrosecondsToInterval(microseconds);
        if (waitFor == 0)
          waitFor = 1; // round up, wait the minimum time we can wait
      }

#ifdef DEBUG_TIMERS
      if (PR_LOG_TEST(gTimerLog, PR_LOG_DEBUG)) {
        if (waitFor == PR_INTERVAL_NO_TIMEOUT)
          PR_LOG(gTimerLog, PR_LOG_DEBUG,
                 ("waiting for PR_INTERVAL_NO_TIMEOUT\n"));
        else
          PR_LOG(gTimerLog, PR_LOG_DEBUG,
                 ("waiting for %u\n", PR_IntervalToMilliseconds(waitFor)));
      }
#endif
    }

    mWaiting = PR_TRUE;
    mMonitor.Wait(waitFor);
    mWaiting = PR_FALSE;
  }

  return NS_OK;
}

nsresult TimerThread::AddTimer(nsTimerImpl *aTimer)
{
  MonitorAutoLock lock(mMonitor);

  // Add the timer to our list.
  PRInt32 i = AddTimerInternal(aTimer);
  if (i < 0)
    return NS_ERROR_OUT_OF_MEMORY;

  // Awaken the timer thread.
  if (mWaiting && i == 0)
    mMonitor.Notify();

  return NS_OK;
}

nsresult TimerThread::TimerDelayChanged(nsTimerImpl *aTimer)
{
  MonitorAutoLock lock(mMonitor);

  // Our caller has a strong ref to aTimer, so it can't go away here under
  // ReleaseTimerInternal.
  RemoveTimerInternal(aTimer);

  PRInt32 i = AddTimerInternal(aTimer);
  if (i < 0)
    return NS_ERROR_OUT_OF_MEMORY;

  // Awaken the timer thread.
  if (mWaiting && i == 0)
    mMonitor.Notify();

  return NS_OK;
}

nsresult TimerThread::RemoveTimer(nsTimerImpl *aTimer)
{
  MonitorAutoLock lock(mMonitor);

  // Remove the timer from our array.  Tell callers that aTimer was not found
  // by returning NS_ERROR_NOT_AVAILABLE.  Unlike the TimerDelayChanged case
  // immediately above, our caller may be passing a (now-)weak ref in via the
  // aTimer param, specifically when nsTimerImpl::Release loses a race with
  // TimerThread::Run, must wait for the mMonitor auto-lock here, and during the
  // wait Run drops the only remaining ref to aTimer via RemoveTimerInternal.

  if (!RemoveTimerInternal(aTimer))
    return NS_ERROR_NOT_AVAILABLE;

  // Awaken the timer thread.
  if (mWaiting)
    mMonitor.Notify();

  return NS_OK;
}

// This function must be called from within a lock
PRInt32 TimerThread::AddTimerInternal(nsTimerImpl *aTimer)
{
  if (mShutdown)
    return -1;

  TimeStamp now = TimeStamp::Now();
  PRUint32 count = mTimers.Length();
  PRUint32 i = 0;
  for (; i < count; i++) {
    nsTimerImpl *timer = mTimers[i];

    // Don't break till we have skipped any overdue timers.

    // XXXbz why?  Given our definition of overdue in terms of
    // mTimeoutAdjustment, aTimer might be overdue already!  Why not
    // just fire timers in order?

    // XXX does this hold for TYPE_REPEATING_PRECISE?  /be

    if (now < timer->mTimeout + mTimeoutAdjustment &&
        aTimer->mTimeout < timer->mTimeout) {
      break;
    }
  }

  if (!mTimers.InsertElementAt(i, aTimer))
    return -1;

  aTimer->mArmed = PR_TRUE;
  NS_ADDREF(aTimer);
  return i;
}

bool TimerThread::RemoveTimerInternal(nsTimerImpl *aTimer)
{
  if (!mTimers.RemoveElement(aTimer))
    return PR_FALSE;

  ReleaseTimerInternal(aTimer);
  return PR_TRUE;
}

void TimerThread::ReleaseTimerInternal(nsTimerImpl *aTimer)
{
  // Order is crucial here -- see nsTimerImpl::Release.
  aTimer->mArmed = PR_FALSE;
  NS_RELEASE(aTimer);
}

void TimerThread::DoBeforeSleep()
{
  mSleeping = PR_TRUE;
}

void TimerThread::DoAfterSleep()
{
  mSleeping = PR_TRUE; // wake may be notified without preceding sleep notification
  for (PRUint32 i = 0; i < mTimers.Length(); i ++) {
    nsTimerImpl *timer = mTimers[i];
    // get and set the delay to cause its timeout to be recomputed
    PRUint32 delay;
    timer->GetDelay(&delay);
    timer->SetDelay(delay);
  }

  // nuke the stored adjustments, so they get recalibrated
  mTimeoutAdjustment = TimeDuration(0);
  mDelayLineCounter = 0;
  mSleeping = PR_FALSE;
}


/* void observe (in nsISupports aSubject, in string aTopic, in wstring aData); */
NS_IMETHODIMP
TimerThread::Observe(nsISupports* /* aSubject */, const char *aTopic, const PRUnichar* /* aData */)
{
  if (strcmp(aTopic, "sleep_notification") == 0)
    DoBeforeSleep();
  else if (strcmp(aTopic, "wake_notification") == 0)
    DoAfterSleep();

  return NS_OK;
}

/*
 *  Portable Runtime System (PRS)
 *  Copyright (C) 2016  Alexandre Tremblay
 *  
 *  This file is part of PRS.
 *  
 *  PRS is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *  
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *  
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *  
 *  portableruntimesystem@gmail.com
 */

/**
 * \file
 * \brief
 *  This file contains the Windows programmable interrupt timer definitions.
 */

#include <windows.h>

#include <prs/pal/atomic.h>
#include <prs/pal/malloc.h>
#include <prs/pal/pit.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/log.h>
#include <prs/rtc.h>

#define PRS_USE_WINDOWS_MM_TIMER

static prs_int_t s_prs_pal_pit_begin_period = 0;

struct prs_pal_pit {
    void*                               userdata;
    void                                (*callback)(void* userdata);
    prs_ticks_t                         period;

#if defined(PRS_USE_WINDOWS_MM_TIMER)
    /*
     * The multimedia timer may not be rescheduled if it takes more than the period to execute. Therefore, we use a
     * second backup timer which verifies, through counters, if the first timer is executing properly.
     */
    UINT                                wintimer;
    UINT                                wintimer_backup;
    PRS_ATOMIC prs_uint_t               check0;
    PRS_ATOMIC prs_uint_t               check1;
#else
    HANDLE                              wintimer;
#endif
};

#if defined(PRS_USE_WINDOWS_MM_TIMER)
static VOID CALLBACK prs_pal_pit_windows_entry(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1,
    DWORD_PTR dw2)
{
    PRS_PRECONDITION(dwUser);

    struct prs_pal_pit* pit = (struct prs_pal_pit*)dwUser;
    pit->callback(pit->userdata);
    prs_pal_atomic_fetch_add(&pit->check0, 1);
}

static VOID CALLBACK prs_pal_pit_windows_backup_entry(UINT uTimerID, UINT uMsg, DWORD_PTR dwUser, DWORD_PTR dw1,
    DWORD_PTR dw2)
{
    PRS_PRECONDITION(dwUser);

    struct prs_pal_pit* pit = (struct prs_pal_pit*)dwUser;
    const prs_uint_t check0 = prs_pal_atomic_load(&pit->check0);
    const prs_uint_t check1 = prs_pal_atomic_fetch_add(&pit->check1, 1);
    const prs_uint_t diff = (check0 > check1 ? check0 - check1 : check1 - check0);
    if (diff >= 100) {
        prs_pal_atomic_store(&pit->check0, 0);
        prs_pal_atomic_store(&pit->check1, 0);
        timeKillEvent(pit->wintimer);
        pit->wintimer = timeSetEvent(PRS_TICKS_TO_MS(pit->period), 0, prs_pal_pit_windows_entry, (DWORD_PTR)pit,
            TIME_PERIODIC | TIME_CALLBACK_FUNCTION | TIME_KILL_SYNCHRONOUS);
        PRS_ERROR_WHEN(!pit->wintimer);
        PRS_FTRACE("Windows Multimedia timer failure, restarting");
    }
}
#else
static VOID CALLBACK prs_pal_pit_windows_entry(PVOID lpParam, BOOLEAN TimerOfWaitFired)
{
    PRS_PRECONDITION(lpParam);
    PRS_ASSERT(TimerOfWaitFired);

    struct prs_pal_pit* pit = lpParam;
    pit->callback(pit->userdata);
}
#endif

struct prs_pal_pit* prs_pal_pit_create(struct prs_pal_pit_create_params* params)
{
    struct prs_pal_pit* pit = prs_pal_malloc_zero(sizeof(*pit));
    if (!pit) {
        return 0;
    }

    pit->userdata = params->userdata;
    pit->callback = params->callback;
    pit->period = params->period;

#if defined(PRS_USE_WINDOWS_MM_TIMER)
    /* Verify that the minimum period is short enough for our timer tick */
    TIMECAPS tc;
    timeGetDevCaps(&tc, sizeof(tc));
    PRS_FATAL_WHEN(tc.wPeriodMin > 1000 / PRS_HZ);

    if (s_prs_pal_pit_begin_period++ == 0) {
        const MMRESULT mmresult = timeBeginPeriod(1);
        PRS_FATAL_WHEN(mmresult != TIMERR_NOERROR);
    }

    if (pit->wintimer) {
        timeKillEvent(pit->wintimer);
    }
    pit->wintimer = timeSetEvent(PRS_TICKS_TO_MS(pit->period), 0, prs_pal_pit_windows_entry, (DWORD_PTR)pit,
        TIME_PERIODIC | TIME_CALLBACK_FUNCTION | TIME_KILL_SYNCHRONOUS);
    PRS_ERROR_IF (!pit->wintimer) {
        goto cleanup;
    }
    pit->wintimer_backup = timeSetEvent(PRS_TICKS_TO_MS(pit->period), 0, prs_pal_pit_windows_backup_entry, (DWORD_PTR)pit,
        TIME_PERIODIC | TIME_CALLBACK_FUNCTION | TIME_KILL_SYNCHRONOUS);
    PRS_ERROR_IF (!pit->wintimer) {
        goto cleanup;
    }
#else
    if (pit->wintimer) {
        DeleteTimerQueueTimer(0, pit->wintimer, INVALID_HANDLE_VALUE);
    }
    const DWORD period = PRS_TICKS_TO_MS(params->period);
    const BOOL success = CreateTimerQueueTimer(&pit->wintimer, 0, prs_pal_pit_windows_entry, pit,
        period, period, WT_EXECUTEINTIMERTHREAD);
    PRS_ERROR_IF (!success) {
        goto cleanup;
    }
#endif

    return pit;

    cleanup:

    if (pit) {
        prs_pal_free(pit);
    }

    return 0;
}

void prs_pal_pit_destroy(struct prs_pal_pit* pit)
{
#if defined(PRS_USE_WINDOWS_MM_TIMER)
    if (pit->wintimer_backup) {
        timeKillEvent(pit->wintimer_backup);
    }
    if (pit->wintimer) {
        timeKillEvent(pit->wintimer);
    }

    if (--s_prs_pal_pit_begin_period == 0) {
        /*
         * Note: we would normally code the code below, but somehow I came across problems where it would take up to a
         * second to execute timeEndPeriod(). However, since we are most likely stopping the application, the OS should
         * take care to reset the period timer to a reasonable value when the process exits.
         */
        //const MMRESULT mmresult = timeEndPeriod(1);
        //PRS_FATAL_WHEN(mmresult != TIMERR_NOERROR);
    }
#else
    if (pit->wintimer) {
        DeleteTimerQueueTimer(0, pit->wintimer, INVALID_HANDLE_VALUE);
    }
#endif
    prs_pal_free(pit);
}


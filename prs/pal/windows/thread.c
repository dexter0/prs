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
 *  This file contains the Windows thread definitions.
 */

#include <process.h>
#include <windows.h>

#include <prs/pal/context.h>
#include <prs/pal/cycles.h>
#include <prs/pal/malloc.h>
#include <prs/pal/thread.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/log.h>


struct prs_pal_thread {
    prs_uintptr_t                       handle;
    prs_bool_t                          from_current;

    void*                               userdata;
    void                                (*entry)(void* userdata);
    void                                (*interrupt)(void* userdata, struct prs_pal_context* context);
    prs_bool_t                          (*interruptible)(void* userdata, struct prs_pal_context* context);

    prs_bool_t                          started;
    volatile prs_bool_t                 exited;

    struct prs_pal_context*             interrupt_context;

    HANDLE                              idle_event;
};

static int prs_pal_thread_prio_to_windows(enum prs_pal_thread_prio prio)
{
    int windows_prio = THREAD_PRIORITY_NORMAL;
    switch (prio) {
        case PRS_PAL_THREAD_PRIO_IDLE:
            windows_prio = THREAD_PRIORITY_IDLE;
            break;
        case PRS_PAL_THREAD_PRIO_LOW:
            windows_prio = THREAD_PRIORITY_LOWEST;
            break;
        case PRS_PAL_THREAD_PRIO_NORMAL:
            windows_prio = THREAD_PRIORITY_NORMAL;
            break;
        case PRS_PAL_THREAD_PRIO_HIGH:
            windows_prio = THREAD_PRIORITY_HIGHEST;
            break;
        case PRS_PAL_THREAD_PRIO_REALTIME:
            windows_prio = THREAD_PRIORITY_TIME_CRITICAL;
            break;
    }
    return windows_prio;
}

static unsigned __stdcall prs_pal_thread_entry(void* userdata)
{
    struct prs_pal_thread* pal_thread = userdata;

    pal_thread->entry(pal_thread->userdata);

    pal_thread->exited = PRS_TRUE;
    _endthreadex(0);
    return 0;
}

struct prs_pal_thread* prs_pal_thread_create(struct prs_pal_thread_create_params* params)
{
    PRS_PRECONDITION(params);

    struct prs_pal_thread* pal_thread = prs_pal_malloc_zero(sizeof(*pal_thread));
    if (!pal_thread) {
        goto cleanup;
    }

    pal_thread->interrupt_context = prs_pal_context_alloc();
    if (!pal_thread->interrupt_context) {
        goto cleanup;
    }

    pal_thread->idle_event = CreateEvent(0, TRUE, FALSE, 0);
    if (!pal_thread->idle_event) {
        goto cleanup;
    }

    pal_thread->from_current = params->from_current;
    if (pal_thread->from_current) {
        /*
         * Duplicate the GetCurrentThread() handle, as it cannot be used by other threads to refer to this thread. The
         * duplicated (pseudo) handle does not need to be freed.
         */
        const HANDLE handle = GetCurrentThread();
        HANDLE duplicated_handle = 0;
        const BOOL duplicate_result = DuplicateHandle(GetCurrentProcess(), handle, GetCurrentProcess(),
            &duplicated_handle, 0, FALSE, DUPLICATE_SAME_ACCESS);
        if (!duplicate_result) {
            goto cleanup;
        }
        pal_thread->handle = (prs_uintptr_t)duplicated_handle;
    } else {
        pal_thread->handle = _beginthreadex(
            NULL, params->stack_size, prs_pal_thread_entry, pal_thread, CREATE_SUSPENDED, NULL);
        if (!pal_thread->handle) {
            goto cleanup;
        }
    }

    const DWORD_PTR affinity_result = SetThreadAffinityMask((HANDLE)pal_thread->handle, params->affinity);
    if (!affinity_result) {
        goto cleanup;
    }

    const BOOL priority_result = SetThreadPriority((HANDLE)pal_thread->handle, prs_pal_thread_prio_to_windows(params->prio));
    if (!priority_result) {
        goto cleanup;
    }

    return pal_thread;

    cleanup:

    if (pal_thread) {
        if (pal_thread->handle) {
            CloseHandle((HANDLE)pal_thread->handle);
        }
        if (pal_thread->idle_event) {
            CloseHandle(pal_thread->idle_event);
        }
        if (pal_thread->interrupt_context) {
            prs_pal_context_free(pal_thread->interrupt_context);
        }
        prs_pal_free(pal_thread);
    }

    return 0;
}

prs_result_t prs_pal_thread_destroy(struct prs_pal_thread* pal_thread)
{
    PRS_PRECONDITION(pal_thread);

    if (pal_thread->started && !pal_thread->exited) {
        /* Has not exited yet. We cannot do this now. */
        /* Note: it's actually possible, but we don't want to. */
        return PRS_INVALID_STATE;
    }

    /* Started then exited, or not started. */
    CloseHandle((HANDLE)pal_thread->handle);

    CloseHandle(pal_thread->idle_event);
    prs_pal_context_free(pal_thread->interrupt_context);

    prs_pal_free(pal_thread);

    return PRS_OK;
}

prs_result_t prs_pal_thread_set_callback_params(struct prs_pal_thread* pal_thread,
    struct prs_pal_thread_callback_params* params)
{
    pal_thread->userdata = params->userdata;
    pal_thread->entry = params->entry;
    pal_thread->interrupt = params->interrupt;
    pal_thread->interruptible = params->interruptible;

    return PRS_OK;
}

prs_result_t prs_pal_thread_start(struct prs_pal_thread* pal_thread)
{
    PRS_PRECONDITION(pal_thread);
    PRS_PRECONDITION(!pal_thread->started);

    if (pal_thread->from_current) {
        pal_thread->started = PRS_TRUE;
        pal_thread->entry(pal_thread->userdata);
        pal_thread->exited = PRS_TRUE;
        pal_thread->started = PRS_FALSE;
        return PRS_OK;
    }

    const DWORD result = ResumeThread((HANDLE)pal_thread->handle);
    if (result == 1) {
        pal_thread->started = PRS_TRUE;
        return PRS_OK;
    }
    return PRS_PLATFORM_ERROR;
}

prs_result_t prs_pal_thread_join(struct prs_pal_thread* pal_thread)
{
    PRS_PRECONDITION(pal_thread);

    if (pal_thread->from_current) {
        /* No actual thread was spawned */
        if (!pal_thread->started || pal_thread->exited) {
            return PRS_OK;
        } else {
            return PRS_INVALID_STATE;
        }
    }

    if (!pal_thread->exited) {
        /*
         * We voluntarily do not check the return value here. WaitForSingleObject could return WAIT_FAILED if the thread
         * already exited.
         */
        const DWORD object = WaitForSingleObject((HANDLE)pal_thread->handle, INFINITE);
        if (object != WAIT_OBJECT_0) {
            PRS_ERROR("WaitForSingleObject failed");
            return PRS_PLATFORM_ERROR;
        }
    }

    PRS_POSTCONDITION(pal_thread->exited);
    return PRS_OK;
}

prs_result_t prs_pal_thread_interrupt(struct prs_pal_thread* pal_thread)
{
    PRS_PRECONDITION(pal_thread);
    PRS_PRECONDITION(pal_thread->started);
    PRS_PRECONDITION(!pal_thread->exited);
    prs_result_t result = PRS_OK;

    /* Only one thread can suspend at a time */
    const DWORD suspend_count = SuspendThread((HANDLE)pal_thread->handle);
    if (suspend_count < 0) {
        PRS_ERROR("SuspendThread failed");
        result = PRS_PLATFORM_ERROR;
        goto end;
    } else if (suspend_count > 0) {
        goto resume;
    }

    PCONTEXT wincontext = (PCONTEXT)pal_thread->interrupt_context;
    PRS_ASSERT(wincontext);
    wincontext->ContextFlags = CONTEXT_FULL;
    const BOOL get_context_result = GetThreadContext((HANDLE)pal_thread->handle, wincontext);
    if (!get_context_result) {
        PRS_ERROR("GetThreadContext failed");
        result = PRS_PLATFORM_ERROR;
        goto resume;
    }

    /* Verify if the thread is still interruptible */
    const prs_bool_t interruptible = pal_thread->interruptible(pal_thread->userdata, pal_thread->interrupt_context);
    if (!interruptible) {
        /*
         * Note: Do not return an error here, although we could. Other platforms may not support verifying the other
         * thread's context for interruptibility.
         */
        goto resume;
    }

    pal_thread->interrupt(pal_thread->userdata, pal_thread->interrupt_context);

    const BOOL set_context_result = SetThreadContext((HANDLE)pal_thread->handle, (PCONTEXT)pal_thread->interrupt_context);
    if (!set_context_result) {
        PRS_ERROR("SetThreadContext failed");
        result = PRS_PLATFORM_ERROR;
        goto resume;
    }

    resume:

    ResumeThread((HANDLE)pal_thread->handle);

    end:

    return result;
}

prs_result_t prs_pal_thread_suspend(struct prs_pal_thread* pal_thread)
{
    PRS_PRECONDITION(pal_thread);
    PRS_PRECONDITION(GetCurrentThreadId() == GetThreadId((HANDLE)pal_thread->handle));

    const DWORD object = WaitForSingleObject(pal_thread->idle_event, INFINITE);
    if (object != WAIT_OBJECT_0) {
        PRS_ERROR("WaitForSingleObject failed");
        return PRS_PLATFORM_ERROR;
    }
    const BOOL result = ResetEvent(pal_thread->idle_event);
    if (!result) {
        PRS_ERROR("ResetEvent failed");
        return PRS_PLATFORM_ERROR;
    }

    return PRS_OK;
}

prs_result_t prs_pal_thread_resume(struct prs_pal_thread* pal_thread)
{
    PRS_PRECONDITION(pal_thread);
    PRS_PRECONDITION(GetCurrentThreadId() != GetThreadId((HANDLE)pal_thread->handle));

    const BOOL result = SetEvent(pal_thread->idle_event);
    if (!result) {
        PRS_ERROR("SetEvent failed");
        return PRS_PLATFORM_ERROR;
    }

    return PRS_OK;
}

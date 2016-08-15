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
 *  This file contains the POSIX thread definitions.
 */

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>

#include <prs/pal/atomic.h>
#include <prs/pal/context.h>
#include <prs/pal/cycles.h>
#include <prs/pal/malloc.h>
#include <prs/pal/thread.h>
#include <prs/pal/tls.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/log.h>

#include "signal.h"

#define PRS_PAL_THREAD_INTERRUPT_SIGNAL SIGUSR1

static const int prs_pal_thread_handled_signals[] = {
    SIGUSR1
};
static const int prs_pal_thread_handled_signal_count =
    sizeof(prs_pal_thread_handled_signals) / sizeof(prs_pal_thread_handled_signals[0]);

static PRS_ATOMIC prs_bool_t s_prs_pal_thread_sigaction_done = PRS_FALSE;
static struct sigaction s_prs_pal_thread_prev_sigaction;

struct prs_pal_thread {
    pthread_t                           thread;

    enum prs_pal_thread_prio            prio;
    prs_core_mask_t                     affinity;
    prs_bool_t                          from_current;
    
    void*                               userdata;
    void                                (*entry)(void* userdata);
    void                                (*interrupt)(void* userdata, struct prs_pal_context* context);
    prs_bool_t                          (*interruptible)(void* userdata, struct prs_pal_context* context);

    volatile prs_bool_t                 started;
    volatile prs_bool_t                 exited;

    sem_t                               idle_sem;
};

static PRS_TLS struct prs_pal_thread* s_prs_pal_thread_current = 0;

static void prs_pal_thread_local_init(struct prs_pal_thread* pal_thread)
{
    PRS_PRECONDITION(pal_thread);
    PRS_PRECONDITION(pal_thread->thread == pthread_self());

    int error;

    PRS_ASSERT(!s_prs_pal_thread_current);
    s_prs_pal_thread_current = pal_thread;

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    for (int i = 0; i < PRS_MAX_CPU; ++i) {
        if (pal_thread->affinity & (1 << i)) {
            CPU_SET(i, &cpuset);
        }
    }
    
    error = pthread_setaffinity_np(pal_thread->thread, sizeof(cpuset), &cpuset);
    PRS_FATAL_WHEN(error);
    
    struct sched_param sp = {
        .sched_priority = 0
    };
    int policy = SCHED_OTHER;
    switch (pal_thread->prio) {
        case PRS_PAL_THREAD_PRIO_IDLE:
            policy = SCHED_IDLE;
            break;
        case PRS_PAL_THREAD_PRIO_LOW:
            policy = SCHED_BATCH;
            break;
        case PRS_PAL_THREAD_PRIO_NORMAL:
            break;
        case PRS_PAL_THREAD_PRIO_HIGH:
            policy = SCHED_RR;
            sp.sched_priority = sched_get_priority_min(SCHED_RR);
            break;
        case PRS_PAL_THREAD_PRIO_REALTIME:
            policy = SCHED_FIFO;
            sp.sched_priority = sched_get_priority_max(SCHED_FIFO);
            break;
    }

    /* Note: this may fail for SCHED_RR and SCHED_FIFO when not running as root */
    error = pthread_setschedparam(pal_thread->thread, policy, &sp);
    PRS_FATAL_WHEN(error);

    /* Mask all the signals except those that should be handled */
    for (int i = 0; i < prs_pal_thread_handled_signal_count; ++i) {
        const int signum = prs_pal_thread_handled_signals[i];
        const prs_result_t result = prs_pal_signal_unblock(signum);
        PRS_FATAL_WHEN(result != PRS_OK);
    }
}

static void prs_pal_thread_sighandler(int signo, siginfo_t* sinfo, void* ucontext)
{
    PRS_PRECONDITION(ucontext);

    struct prs_pal_thread* pal_thread = s_prs_pal_thread_current;
    if (!pal_thread) {
        prs_pal_signal_chain(signo, sinfo, ucontext, &s_prs_pal_thread_prev_sigaction);
        return;
    }

#if defined(PRS_ASSERTIONS)
    /* Verify that the received signal is one that we expect */
    prs_bool_t found = PRS_FALSE;
    for (int i = 0; i < prs_pal_thread_handled_signal_count; ++i) {
        const int signum = prs_pal_thread_handled_signals[i];
        if (signum == signo) {
            found = PRS_TRUE;
            break;
        }
    }
    PRS_ASSERT(found);
#endif /* PRS_ASSERTIONS */

    struct prs_pal_context* context = ucontext;
    
    /* Verify if the thread is still interruptible */
    const prs_bool_t interruptible = pal_thread->interruptible(pal_thread->userdata, context);
    if (interruptible) {
        pal_thread->interrupt(pal_thread->userdata, context);
    }
}

static void* prs_pal_thread_entry(void* userdata)
{
    struct prs_pal_thread* pal_thread = userdata;
    
    prs_pal_thread_local_init(pal_thread);
    
    /*
     * Since the thread starts immediately after pthread_create() is started, we must wait that prs_pal_thread_start()
     * is called before executing the entry point.
     */
    prs_pal_thread_suspend(pal_thread);
    pal_thread->started = PRS_TRUE;
    
    pal_thread->entry(pal_thread->userdata);
    pal_thread->exited = PRS_TRUE;
    return 0;
}

struct prs_pal_thread* prs_pal_thread_create(struct prs_pal_thread_create_params* params)
{
    PRS_PRECONDITION(params);
    
    int error;

    struct prs_pal_thread* pal_thread = prs_pal_malloc_zero(sizeof(*pal_thread));
    if (!pal_thread) {
        goto cleanup;
    }

    error = sem_init(&pal_thread->idle_sem, 0, 0);
    if (error) {
        goto cleanup;
    }

    pal_thread->prio = params->prio;
    pal_thread->affinity = params->affinity;
    pal_thread->from_current = params->from_current;
    
    if (!prs_pal_atomic_exchange(&s_prs_pal_thread_sigaction_done, PRS_TRUE)) {
        for (int i = 0; i < prs_pal_thread_handled_signal_count; ++i) {
            const int signum = prs_pal_thread_handled_signals[i];
            const prs_result_t result = prs_pal_signal_action(signum, prs_pal_thread_sighandler,
                &s_prs_pal_thread_prev_sigaction);
            if (result != PRS_OK) {
                goto cleanup;
            }
        }
    }
    
    if (pal_thread->from_current) {
        pal_thread->thread = pthread_self();
    } else {
        error = pthread_create(&pal_thread->thread, 0, prs_pal_thread_entry, pal_thread);
        if (error) {
            goto cleanup;
        }
    }

    return pal_thread;

    cleanup:
    
    if (pal_thread) {
        sem_destroy(&pal_thread->idle_sem);
        prs_pal_free(pal_thread);
    }

    return 0;
}

prs_result_t prs_pal_thread_destroy(struct prs_pal_thread* pal_thread)
{
    PRS_PRECONDITION(pal_thread);

    if (pal_thread->started) {
        if (!pal_thread->exited) {
            /* Has not exited yet. We cannot do this now. */
            /* Note: it's actually possible, but we don't want to. */
            return PRS_INVALID_STATE;
        }
    } else {
        /* Not started, we can't destroy it yet */
        return PRS_INVALID_STATE;
    }
    
    pthread_join(pal_thread->thread, 0);

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
        prs_pal_thread_local_init(pal_thread);
        pal_thread->entry(pal_thread->userdata);
        pal_thread->exited = PRS_TRUE;
        pal_thread->started = PRS_FALSE;
        return PRS_OK;
    }

    return prs_pal_thread_resume(pal_thread);
}

prs_result_t prs_pal_thread_join(struct prs_pal_thread* pal_thread)
{
    PRS_PRECONDITION(pal_thread);

    if (pal_thread->from_current) {
        /* No actual thread was spawned */
        if (pal_thread->exited) {
            return PRS_OK;
        } else {
            return PRS_INVALID_STATE;
        }
    }

    if (!pal_thread->exited) {
        /*
         * We voluntarily do not check the return value here. pthread_join() could return an error if the thread
         * already exited.
         */
        pthread_join(pal_thread->thread, 0);
    }

    PRS_POSTCONDITION(pal_thread->exited);
    return PRS_OK;
}

prs_result_t prs_pal_thread_interrupt(struct prs_pal_thread* pal_thread)
{
    PRS_PRECONDITION(pal_thread);
    PRS_PRECONDITION(pal_thread->started);
    PRS_PRECONDITION(!pal_thread->exited);

    const int error = pthread_kill(pal_thread->thread, PRS_PAL_THREAD_INTERRUPT_SIGNAL);
    if (error) {
        if (error == ESRCH) {
            return PRS_INVALID_STATE;
        } else {
            return PRS_PLATFORM_ERROR;
        }
    }

    return PRS_OK;
}

prs_result_t prs_pal_thread_suspend(struct prs_pal_thread* pal_thread)
{
    PRS_PRECONDITION(pal_thread);
    PRS_PRECONDITION(pthread_self() == pal_thread->thread);

    for (;;) {
        const int error = sem_wait(&pal_thread->idle_sem);
        if (error) {
            if (errno == EINTR) {
                continue;
            }
            return PRS_PLATFORM_ERROR;
        }
        break;
    }

    return PRS_OK;
}

prs_result_t prs_pal_thread_resume(struct prs_pal_thread* pal_thread)
{
    PRS_PRECONDITION(pal_thread);
    /* On Linux, it's possible for a signal handler running on this thread to resume it */
    //PRS_PRECONDITION(pthread_self() != pal_thread->thread);

    const int error = sem_post(&pal_thread->idle_sem);
    if (error) {
        return PRS_PLATFORM_ERROR;
    }

    return PRS_OK;
}

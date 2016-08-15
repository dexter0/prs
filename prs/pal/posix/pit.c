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
 *  This file contains the POSIX programmable interrupt timer definitions.
 */

#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <signal.h>
#include <sys/time.h>

#include <prs/pal/atomic.h>
#include <prs/pal/malloc.h>
#include <prs/pal/pit.h>
#include <prs/pal/thread.h>
#include <prs/pal/tls.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/log.h>
#include <prs/rtc.h>

#include "signal.h"

static const int prs_pal_pit_handled_signals[] = {
    SIGALRM
};
static const int prs_pal_pit_handled_signal_count =
    sizeof(prs_pal_pit_handled_signals) / sizeof(prs_pal_pit_handled_signals[0]);

struct prs_pal_pit {
    void*                               userdata;
    void                                (*callback)(void* userdata);
    prs_ticks_t                         period;

    prs_bool_t                          use_current_thread;
    struct prs_pal_thread*              thread;
    sem_t                               exit_sem;
};

static PRS_TLS struct prs_pal_pit* s_prs_pal_pit = 0;

static PRS_ATOMIC prs_bool_t s_prs_pal_pit_sigaction_done = PRS_FALSE;
static struct sigaction s_prs_pal_pit_prev_sigaction;

static void prs_pal_pit_sighandler(int signo, siginfo_t* sinfo, void* ucontext)
{
    struct prs_pal_pit* pit = s_prs_pal_pit;
    if (!pit) {
        prs_pal_signal_chain(signo, sinfo, ucontext, &s_prs_pal_pit_prev_sigaction);
        return;
    }

    pit->callback(pit->userdata);
}

static prs_result_t prs_pal_pit_init_timer(struct prs_pal_pit* pit)
{
    for (int i = 0; i < prs_pal_pit_handled_signal_count; ++i) {
        const int signum = prs_pal_pit_handled_signals[i];
        const prs_result_t result = prs_pal_signal_unblock(signum);
        if (result != PRS_OK) {
            return result;
        }
    }

    struct itimerval val = {
        .it_interval = {
            .tv_sec = 0,
            .tv_usec = PRS_TICKS_TO_US(pit->period)
        },
        .it_value = {
            .tv_sec = 0,
            .tv_usec = PRS_TICKS_TO_US(pit->period)
        }
    };

    const int error = setitimer(ITIMER_REAL, &val, 0);
    if (error) {
        return PRS_PLATFORM_ERROR;
    }
    return PRS_OK;
}

static prs_result_t prs_pal_pit_uninit_timer(struct prs_pal_pit* pit)
{
    struct itimerval val = {
        .it_interval = {
            .tv_sec = 0,
            .tv_usec = 0
        },
        .it_value = {
            .tv_sec = 0,
            .tv_usec = 0
        }
    };

    const int error = setitimer(ITIMER_REAL, &val, 0);
    if (error) {
        return PRS_PLATFORM_ERROR;
    }

    for (int i = 0; i < prs_pal_pit_handled_signal_count; ++i) {
        const int signum = prs_pal_pit_handled_signals[i];
        const prs_result_t result = prs_pal_signal_block(signum);
        if (result != PRS_OK) {
            return result;
        }
    }

    return PRS_OK;
}

static void prs_pal_pit_thread_entry(void* userdata)
{
    struct prs_pal_pit* pit = userdata;
    s_prs_pal_pit = pit;

    const prs_result_t result = prs_pal_pit_init_timer(pit);
    PRS_FATAL_WHEN(result != PRS_OK);

    for (;;) {
        const int error = sem_wait(&pit->exit_sem);
        if (error) {
            if (errno == EINTR) {
                continue;
            }
        }
        break;
    }
    
    prs_pal_pit_uninit_timer(pit);
}

struct prs_pal_pit* prs_pal_pit_create(struct prs_pal_pit_create_params* params)
{
    struct prs_pal_pit* pit = prs_pal_malloc_zero(sizeof(*pit));
    if (!pit) {
        return 0;
    }

    pit->userdata = params->userdata;
    pit->callback = params->callback;
    pit->period = params->period;
    pit->use_current_thread = params->use_current_thread;

    if (!prs_pal_atomic_exchange(&s_prs_pal_pit_sigaction_done, PRS_TRUE)) {
        for (int i = 0; i < prs_pal_pit_handled_signal_count; ++i) {
            const int signum = prs_pal_pit_handled_signals[i];
            const prs_result_t result = prs_pal_signal_action(signum, prs_pal_pit_sighandler,
                &s_prs_pal_pit_prev_sigaction);
            if (result != PRS_OK) {
                goto cleanup;
            }
        }
    }

    if (pit->use_current_thread) {
        s_prs_pal_pit = pit;
        const prs_result_t result = prs_pal_pit_init_timer(pit);
        if (result != PRS_OK) {
            goto cleanup;
        }
    } else {
        const int error = sem_init(&pit->exit_sem, 0, 0);
        if (error) {
            goto cleanup;
        }

        struct prs_pal_thread_create_params pal_thread_params = {
            .stack_size = 4096,
            .prio = params->prio,
            .affinity = params->affinity,
            .from_current = PRS_FALSE
        };
        pit->thread = prs_pal_thread_create(&pal_thread_params);
        if (!pit->thread) {
            sem_close(&pit->exit_sem);
            goto cleanup;
        }

        struct prs_pal_thread_callback_params callback_params = {
            .userdata = pit,
            .entry = prs_pal_pit_thread_entry,
            .interrupt = 0,
            .interruptible = 0
        };
        prs_pal_thread_set_callback_params(pit->thread, &callback_params);
        prs_pal_thread_start(pit->thread);
    }

    return pit;

    cleanup:

    if (pit) {
        if (pit->use_current_thread) {
            prs_pal_pit_uninit_timer(pit);
        }
        prs_pal_free(pit);
    }

    return 0;
}

void prs_pal_pit_destroy(struct prs_pal_pit* pit)
{
    if (pit->use_current_thread) {
        prs_pal_pit_uninit_timer(pit);
    } else {
        sem_post(&pit->exit_sem);
        prs_pal_thread_destroy(pit->thread);
        sem_close(&pit->exit_sem);
    }
    prs_pal_free(pit);
}


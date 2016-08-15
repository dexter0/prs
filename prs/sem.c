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
 *  This file contains semaphore definitions.
 */

#include <prs/pal/atomic.h>
#include <prs/pal/malloc.h>
#include <prs/clock.h>
#include <prs/error.h>
#include <prs/event.h>
#include <prs/god.h>
#include <prs/gpd.h>
#include <prs/mpmcq.h>
#include <prs/rtc.h>
#include <prs/sem.h>
#include <prs/timer.h>
#include <prs/worker.h>

#include "task.h"

#define PRS_SEM_EVENT_TYPE_SIGNAL       1
#define PRS_SEM_EVENT_TYPE_TIMEOUT      2

struct prs_sem {
    prs_sem_id_t                        id;
    prs_int_t                           max_count;
    PRS_ATOMIC prs_int_t                count;
    struct prs_mpmcq*                   waitq;
};

static void prs_sem_object_destroy(void* object)
{
    prs_sem_destroy(object);
}

static void prs_sem_object_free(void* object)
{
    struct prs_sem* sem = object;

    if (sem->waitq) {
        prs_mpmcq_destroy(sem->waitq);
    }
    prs_pal_free(sem);
}

static void prs_sem_object_print(void* object, void* userdata, void (*fct)(void*, const char*, ...))
{
    struct prs_sem* sem = object;

    fct(userdata, "Sem id=%u max_count=%d count=%d\n",
        sem->id,
        sem->max_count,
        prs_pal_atomic_load(&sem->count));
}

static struct prs_object_ops s_prs_sem_object_ops = {
    .destroy = prs_sem_object_destroy,
    .free = prs_sem_object_free,
    .print = prs_sem_object_print
};

/**
 * \brief
 *  Creates a semaphore.
 * \param params
 *  Semaphore parameters.
 */
struct prs_sem* prs_sem_create(struct prs_sem_create_params* params)
{
    struct prs_sem* sem = prs_pal_malloc_zero(sizeof(*sem));
    if (!sem) {
        goto cleanup;
    }

    struct prs_mpmcq_create_params mpmcq_params = {
        .area = 0,
        .pd = prs_gpd_get(),
        .pd_params = 0
    };
    sem->waitq = prs_mpmcq_create(&mpmcq_params);
    if (!sem->waitq) {
        goto cleanup;
    }

    sem->id = prs_god_alloc_and_lock(sem, &s_prs_sem_object_ops);
    if (sem->id == PRS_OBJECT_ID_INVALID) {
        goto cleanup;
    }

    sem->max_count = params->max_count;
    prs_pal_atomic_store(&sem->count, params->initial_count);

    return sem;

cleanup:

    if (sem) {
        if (sem->id) {
            prs_god_unlock(sem->id);
        } else {
            prs_pal_free(sem);
        }
    }

    return 0;
}

/**
 * \brief
 *  Destroys a semaphore.
 * \param sem
 *  Semaphore to destroy.
 */
void prs_sem_destroy(struct prs_sem* sem)
{
    prs_god_unlock(sem->id);
}

static prs_bool_t prs_sem_signal_once(struct prs_sem* sem, struct prs_event* self_event)
{
    for (;;) {
        struct prs_event* pop_event = 0;
        const prs_result_t result = prs_mpmcq_pop(sem->waitq, &pop_event);
        if (result != PRS_OK || !pop_event) {
            /*
             * If self_event wasn't null, this means that the event was popped from the wait queue by another worker
             * before we could get here.
             */
            return PRS_FALSE;
        }

        const prs_event_state_t event_state = prs_event_signal(pop_event, PRS_SEM_EVENT_TYPE_SIGNAL);
        if (!(event_state & PRS_EVENT_STATE_SIGNALED)) {
            if (pop_event == self_event) {
                /* Cancel the remaining references if they exist */
                if (!(event_state & PRS_EVENT_STATE_FREED)) {
                    prs_event_cancel(self_event);
                }

                /*
                 * We successfully signaled (and possibly canceled) our own event, which means that we can return
                 * immediately from the wait() function call.
                 */
                return PRS_TRUE;
            }

            /*
             * We signaled the event or another worker, so if we were called from a wait() function, we must call the
             * scheduler.
             */
            return PRS_FALSE;
        }
    }
}

/**
 * \brief
 *  Decrements the semaphore. If the semaphore count is below zero, wait in queue for the count to increment.
 * \param sem
 *  Semaphore to wait for.
 */
void prs_sem_wait(struct prs_sem* sem)
{
    struct prs_worker* worker = prs_worker_current();
    PRS_ASSERT(worker);
    struct prs_task* task = prs_worker_get_current_task(worker);
    PRS_ASSERT(task);

    struct prs_event* push_event = prs_event_create(task, 1);
    PRS_FATAL_WHEN(!push_event);

    const prs_result_t result = prs_mpmcq_push(sem->waitq, push_event);
    PRS_KILL_TASK_WHEN(result != PRS_OK);

    prs_int_t value = prs_pal_atomic_fetch_sub(&sem->count, 1);
    if (value > 0) {
        const prs_result_t signaled = prs_sem_signal_once(sem, push_event);
        if (signaled) {
            return;
        }
    }

    prs_sched_schedule();
}

/**
 * \brief
 *  Decrements the semaphore. If the semaphore count is below zero, wait in queue for the count to increment or for
 *  the specified timeout to occur.
 * \param sem
 *  Semaphore to wait for.
 * \param timeout
 *  Time to wait, in ticks.
 * \return
 *  \ref PRS_TIMEOUT if the timeout occurred.
 *  \ref PRS_OK if the semaphore was signaled before the timeout.
 */
prs_result_t prs_sem_wait_timeout(struct prs_sem* sem, prs_ticks_t timeout)
{
    struct prs_worker* worker = prs_worker_current();
    PRS_ASSERT(worker);
    struct prs_task* task = prs_worker_get_current_task(worker);
    PRS_ASSERT(task);

    struct prs_event* push_event = prs_event_create(task, 2);
    PRS_FATAL_WHEN(!push_event);

    const prs_result_t push_result = prs_mpmcq_push(sem->waitq, push_event);
    PRS_KILL_TASK_WHEN(push_result != PRS_OK);

    prs_int_t value = prs_pal_atomic_fetch_sub(&sem->count, 1);
    if (value > 0) {
        const prs_result_t signaled = prs_sem_signal_once(sem, push_event);
        if (signaled) {
            return PRS_OK;
        }
    }

    struct prs_timer_entry* timer_entry = prs_timer_queue(prs_clock_timer(), push_event, PRS_SEM_EVENT_TYPE_TIMEOUT, timeout);
    PRS_ASSERT(timer_entry);
    prs_sched_schedule();
    prs_timer_cancel(prs_clock_timer(), timer_entry);

    const prs_event_type_t event_type = (prs_event_type_t)prs_task_get_last_unblock_cause(task);
    if (event_type == PRS_SEM_EVENT_TYPE_SIGNAL) {
        return PRS_OK;
    } else {
        PRS_ASSERT(event_type == PRS_SEM_EVENT_TYPE_TIMEOUT);
        return PRS_TIMEOUT;
    }
}

/**
 * \brief
 *  Increments the semaphore and signals a waiting task if the count was negative.
 * \param sem
 *  Semaphore to signal for.
 */
void prs_sem_signal(struct prs_sem* sem)
{
    prs_int_t value = prs_pal_atomic_fetch_add(&sem->count, 1);
    if (value < 0) {
        prs_sem_signal_once(sem, 0);
    }
}

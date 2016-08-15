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
 *  This file contains the definitions for a single worker priority scheduler.
 *
 *  This preemptive scheduler will interrupt the current task's execution when one of its higher-priority tasks
 *  transition from the \ref PRS_TASK_STATE_BLOCKED state to \ref PRS_TASK_STATE_READY.
 *
 *  The scheduler supports up to \ref PRS_MAX_TASK_PRIO levels of priority. The lowest priority value has the highest
 *  priority, i.e. will be executed first when possible.
 */

#include <stddef.h>

#include <prs/pal/bitops.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/dllist.h>
#include <prs/error.h>
#include <prs/god.h>
#include <prs/log.h>
#include <prs/mpsciq.h>
#include <prs/rtc.h>
#include <prs/task.h>
#include <prs/worker.h>

#include "../task.h"

#define prs_sched_swprio_for_each_prio(prio) \
    for (prs_task_prio_t prio = 0; prio < PRS_MAX_TASK_PRIO; ++prio)

struct prs_sched_task_userdata {
    struct prs_task*                    task;
    struct prs_mpsciq_node              ready_node;

    struct prs_mpsciq_node              remove_node;
};

struct prs_sched_swprio {
    PRS_ATOMIC prs_task_prio_t          ready_mask;
    struct prs_mpsciq*                  readyq[PRS_MAX_TASK_PRIO];

    struct prs_mpsciq*                  removeq;
};
static prs_result_t prs_sched_swprio_init(struct prs_sched_data* sched_data, void* userdata)
{
    prs_result_t result = PRS_OK;
    struct prs_sched_swprio* sched = prs_pal_malloc_zero(sizeof(*sched));
    if (!sched) {
        result = PRS_OUT_OF_MEMORY;
        goto cleanup;
    }

    prs_sched_swprio_for_each_prio(prio) {
        struct prs_mpsciq** readyq = &sched->readyq[prio];

        struct prs_mpsciq_create_params mpsciq_params = {
            .node_offset = offsetof(struct prs_sched_task_userdata, ready_node)
        };
        *readyq = prs_mpsciq_create(&mpsciq_params);
        if (!*readyq) {
            result = PRS_OUT_OF_MEMORY;
            goto cleanup;
        }
    }

    struct prs_mpsciq_create_params mpsciq_params = {
        .node_offset = offsetof(struct prs_sched_task_userdata, remove_node)
    };
    sched->removeq = prs_mpsciq_create(&mpsciq_params);
    PRS_FATAL_WHEN(!sched->removeq);

    sched_data->userdata = sched;

    return result;

    cleanup:

    if (sched) {
        prs_sched_swprio_for_each_prio(prio) {
            struct prs_mpsciq* readyq = sched->readyq[prio];
            if (readyq) {
                prs_mpsciq_destroy(readyq);
            }
        }
        prs_pal_free(sched);
    }

    return result;
}

static prs_result_t prs_sched_swprio_uninit(struct prs_sched_data* sched_data)
{
    struct prs_sched_swprio* sched = sched_data->userdata;

    prs_mpsciq_destroy(sched->removeq);
    prs_sched_swprio_for_each_prio(prio) {
        struct prs_mpsciq* readyq = sched->readyq[prio];
        if (readyq) {
            prs_mpsciq_destroy(readyq);
        }
    }
    prs_pal_free(sched);

    return PRS_OK;
}

static prs_bool_t prs_sched_swprio_get_next(struct prs_sched_worker* sched_worker, struct prs_task* current_task,
    struct prs_task** task)
{
    struct prs_sched_data* sched_data = sched_worker->sched_data;
    struct prs_sched_swprio* sched = sched_data->userdata;

    struct prs_mpsciq_node* remove_node = prs_mpsciq_begin(sched->removeq);
    while (remove_node) {
        struct prs_sched_task_userdata* task_userdata = prs_mpsciq_get_data(sched->removeq, remove_node);
        struct prs_task* removed_task = task_userdata->task;
        if (current_task == removed_task) {
            /*
             * We can't remove this task now, as we are running in its register context. Return now to ask the worker
             * to change register contexts so we can safely unreference this task.
             */
            *task = 0;
            PRS_FTRACE("request other stack because task %s (%u) is being deleted", current_task->name, current_task->id);
            return PRS_FALSE;
        }
        /* Make sure the removed task is not in a ready queue */
        struct prs_mpsciq* readyq = sched->readyq[removed_task->prio];
        if (prs_mpsciq_is_inserted(readyq, &task_userdata->ready_node)) {
            prs_mpsciq_remove(readyq, &task_userdata->ready_node);
            if (!prs_mpsciq_begin(readyq)) {
                prs_pal_atomic_fetch_and(&sched->ready_mask, ~(1 << removed_task->prio));
                if (prs_mpsciq_begin(readyq)) {
                    prs_pal_atomic_fetch_or(&sched->ready_mask, (1 << removed_task->prio));
                }
            }
        }

        prs_task_change_state(removed_task, PRS_TASK_STATE_STOPPED, PRS_TASK_STATE_ZOMBIE);
        removed_task->sched_userdata = 0;
        prs_mpsciq_remove(sched->removeq, remove_node);
        prs_pal_free(task_userdata);
        prs_god_unlock(removed_task->id);
        remove_node = prs_mpsciq_begin(sched->removeq);
    }

    prs_task_prio_t next_prio = PRS_MAX_TASK_PRIO;
    struct prs_mpsciq_node* node = 0;
    struct prs_mpsciq* readyq = 0;
    prs_task_prio_t ready_mask = prs_pal_atomic_load(&sched->ready_mask);
    while (ready_mask) {
        const prs_int_t prio = prs_bitops_lsb_uint32(ready_mask);
        PRS_ASSERT(prio >= 0 && prio < PRS_MAX_TASK_PRIO);
        readyq = sched->readyq[prio];
        node = prs_mpsciq_begin(readyq);
        if (!node) {
            const prs_task_prio_t prio_mask = (1 << prio);
            prs_pal_atomic_fetch_and(&sched->ready_mask, ~prio_mask);
            node = prs_mpsciq_begin(readyq);
            if (node) {
                prs_pal_atomic_fetch_or(&sched->ready_mask, prio_mask);
            } else {
                ready_mask &= ~prio_mask;
                continue;
            }
        }
        next_prio = prio;
        break;
    }

    if (!node) {
        if (current_task && prs_task_get_state(current_task) == PRS_TASK_STATE_RUNNING) {
            *task = current_task;
        } else {
            *task = 0;
        }
        goto end;
    }

    /*
     * Special case when the current task is interrupted: if another task with a higher priority is ready, we must
     * preempt the current task.
     */
    if (current_task) {
        const enum prs_task_state current_task_state = prs_task_get_state(current_task);
        if (current_task_state == PRS_TASK_STATE_RUNNING) {
            if (current_task->prio > next_prio) {
                struct prs_sched_task_userdata* task_userdata = current_task->sched_userdata;
                PRS_FTRACE("preempt task %s (%u)", current_task->name, current_task->id);
                prs_mpsciq_push(sched->readyq[current_task->prio], &task_userdata->ready_node);
                prs_pal_atomic_fetch_or(&sched->ready_mask, (1 << current_task->prio));
                prs_task_change_state(current_task, current_task_state, PRS_TASK_STATE_READY);
            } else {
                /* Same priority - no need to switch tasks yet */
                *task = current_task;
                goto end;
            }
        }
    }

    struct prs_sched_task_userdata* userdata = prs_mpsciq_get_data(readyq, node);
    prs_mpsciq_remove(readyq, node);
    PRS_ASSERT(!node->next);
    PRS_ASSERT(!node->prev);
    struct prs_task* next_task = userdata->task;
    prs_task_change_state(next_task, PRS_TASK_STATE_READY, PRS_TASK_STATE_RUNNING);
    *task = next_task;

    end:

    PRS_POSTCONDITION(!*task || prs_task_get_state(*task) == PRS_TASK_STATE_RUNNING);

    return PRS_TRUE;
}

static prs_result_t prs_sched_swprio_ready(struct prs_sched_data* sched_data, struct prs_task* task)
{
    struct prs_sched_swprio* sched = sched_data->userdata;
    struct prs_sched_task_userdata* task_userdata = task->sched_userdata;

    PRS_FTRACE("task %s (%u)", task->name, task->id);

    prs_mpsciq_push(sched->readyq[task->prio], &task_userdata->ready_node);
    prs_pal_atomic_fetch_or(&sched->ready_mask, (1 << task->prio));

    struct prs_worker* lowest_worker = 0;
    prs_task_prio_t lowest_prio = 0;
    prs_dllist_foreach(sched_data->workers, node) {
        struct prs_sched_worker* sched_worker = prs_dllist_get_data(sched_data->workers, node);
        const prs_task_id_t current_task_id = prs_worker_get_current_task_id(sched_worker->worker);
        struct prs_task* current_task = 0;
        if (current_task_id != PRS_OBJECT_ID_INVALID) {
            current_task = prs_god_lock(current_task_id);
        }
        if (!current_task || current_task->prio > lowest_prio) {
            lowest_prio = (current_task ? current_task->prio : PRS_MAX_TASK_PRIO);
            lowest_worker = sched_worker->worker;
        }
        if (current_task) {
            prs_god_unlock(current_task_id);
        }
    }

    if (lowest_worker && task->prio < lowest_prio) {
        prs_worker_interrupt(lowest_worker);
    }

    return PRS_OK;
}

static prs_result_t prs_sched_swprio_add(struct prs_sched_data* sched_data, struct prs_task* task)
{
    PRS_PRECONDITION(sched_data);
    PRS_PRECONDITION(task);
    PRS_PRECONDITION(task->prio < PRS_MAX_TASK_PRIO);
    PRS_PRECONDITION(!task->sched_userdata);

    struct prs_sched_task_userdata* userdata = prs_pal_malloc_zero(sizeof(*userdata));
    if (!userdata) {
        return PRS_OUT_OF_MEMORY;
    }
    task->sched_userdata = userdata;
    userdata->task = task;

    struct prs_task* god_task = prs_god_lock(task->id);
    PRS_ASSERT(god_task == task);

    prs_task_change_state(task, PRS_TASK_STATE_STOPPED, PRS_TASK_STATE_READY);

    return prs_sched_swprio_ready(sched_data, task);
}

static prs_result_t prs_sched_swprio_remove(struct prs_sched_data* sched_data, struct prs_task* task)
{
    PRS_PRECONDITION(sched_data);
    PRS_PRECONDITION(task);
    PRS_PRECONDITION(task->sched_userdata);

    PRS_FTRACE("%s (%u)", task->name, task->id);

    const enum prs_task_state prev_state = prs_task_get_state(task);
    if (prev_state != PRS_TASK_STATE_STOPPED) {
        struct prs_sched_swprio* sched = sched_data->userdata;
        struct prs_sched_task_userdata* userdata = task->sched_userdata;
        prs_task_change_state(task, prev_state, PRS_TASK_STATE_STOPPED);
        prs_mpsciq_push(sched->removeq, &userdata->remove_node);

        if (prev_state == PRS_TASK_STATE_RUNNING) {
            prs_dllist_foreach(sched_data->workers, node) {
                struct prs_sched_worker* sched_worker = prs_dllist_get_data(sched_data->workers, node);
                const prs_task_id_t current_task_id = prs_worker_get_current_task_id(sched_worker->worker);
                if (current_task_id == task->id) {
                    prs_worker_interrupt(sched_worker->worker);
                    break;
                }
            }
        }
    }

    return PRS_OK;
}

struct prs_sched_ops* prs_sched_swprio_ops(void)
{
    static struct prs_sched_ops s_sched_swprio_ops = {
        .init = prs_sched_swprio_init,
        .uninit = prs_sched_swprio_uninit,
        .add = prs_sched_swprio_add,
        .remove = prs_sched_swprio_remove,
        .get_next = prs_sched_swprio_get_next,
        .ready = prs_sched_swprio_ready
    };
    return &s_sched_swprio_ops;
}

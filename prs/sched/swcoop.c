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
 *  This file contains the definitions for a single worker cooperative scheduler.
 *
 *  This cooperative scheduler only has one ready queue for all tasks that are running under itself. Because it is
 *  only cooperative, it will never interrupt a running task while it is running. A task will only yield its execution
 *  when blocked.
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

struct prs_sched_task_userdata {
    struct prs_task*                    task;
    struct prs_mpsciq_node              ready_node;
    struct prs_mpsciq_node              remove_node;
};

struct prs_sched_swcoop {
    struct prs_mpsciq*                  readyq;
    struct prs_mpsciq*                  removeq;
};
static prs_result_t prs_sched_swcoop_init(struct prs_sched_data* sched_data, void* userdata)
{
    prs_result_t result = PRS_OK;
    struct prs_sched_swcoop* sched = prs_pal_malloc_zero(sizeof(*sched));
    if (!sched) {
        result = PRS_OUT_OF_MEMORY;
        goto cleanup;
    }

    struct prs_mpsciq_create_params readyq_params = {
        .node_offset = offsetof(struct prs_sched_task_userdata, ready_node)
    };
    sched->readyq = prs_mpsciq_create(&readyq_params);
    if (!sched->readyq) {
        result = PRS_OUT_OF_MEMORY;
        goto cleanup;
    }

    struct prs_mpsciq_create_params removeq_params = {
        .node_offset = offsetof(struct prs_sched_task_userdata, remove_node)
    };
    sched->removeq = prs_mpsciq_create(&removeq_params);
    PRS_FATAL_WHEN(!sched->removeq);

    sched_data->userdata = sched;

    return result;

    cleanup:

    if (sched) {
        if (sched->readyq) {
            prs_mpsciq_destroy(sched->readyq);
        }
        prs_pal_free(sched);
    }

    return result;
}

static prs_result_t prs_sched_swcoop_uninit(struct prs_sched_data* sched_data)
{
    struct prs_sched_swcoop* sched = sched_data->userdata;

    prs_mpsciq_destroy(sched->removeq);
    prs_mpsciq_destroy(sched->readyq);
    prs_pal_free(sched);

    return PRS_OK;
}

static prs_bool_t prs_sched_swcoop_get_next(struct prs_sched_worker* sched_worker, struct prs_task* current_task,
    struct prs_task** task)
{
    struct prs_sched_data* sched_data = sched_worker->sched_data;
    struct prs_sched_swcoop* sched = sched_data->userdata;

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
        /* Make sure the removed task is not in the ready queue */
        if (prs_mpsciq_is_inserted(sched->readyq, &task_userdata->ready_node)) {
            prs_mpsciq_remove(sched->readyq, &task_userdata->ready_node);
        }

        prs_task_change_state(removed_task, PRS_TASK_STATE_STOPPED, PRS_TASK_STATE_ZOMBIE);
        removed_task->sched_userdata = 0;
        prs_mpsciq_remove(sched->removeq, remove_node);
        prs_pal_free(task_userdata);
        prs_god_unlock(removed_task->id);
        remove_node = prs_mpsciq_begin(sched->removeq);
    }

    if (current_task && prs_task_get_state(current_task) == PRS_TASK_STATE_RUNNING) {
        *task = current_task;
        goto end;
    }

    struct prs_mpsciq_node* node = prs_mpsciq_begin(sched->readyq);
    if (node) {
        struct prs_sched_task_userdata* userdata = prs_mpsciq_get_data(sched->readyq, node);
        prs_mpsciq_remove(sched->readyq, node);
        PRS_ASSERT(!node->next);
        PRS_ASSERT(!node->prev);
        struct prs_task* next_task = userdata->task;
        prs_task_change_state(next_task, PRS_TASK_STATE_READY, PRS_TASK_STATE_RUNNING);
        *task = next_task;
    } else {
        *task = 0;
    }

    end:

    PRS_POSTCONDITION(!*task || prs_task_get_state(*task) == PRS_TASK_STATE_RUNNING);

    return PRS_TRUE;
}

static prs_result_t prs_sched_swcoop_ready(struct prs_sched_data* sched_data, struct prs_task* task)
{
    struct prs_sched_swcoop* sched = sched_data->userdata;
    struct prs_sched_task_userdata* task_userdata = task->sched_userdata;

    PRS_FTRACE("task %s (%u)", task->name, task->id);

    prs_mpsciq_push(sched->readyq, &task_userdata->ready_node);

    struct prs_worker* worker = 0;
    prs_dllist_foreach(sched_data->workers, node) {
        struct prs_sched_worker* sched_worker = prs_dllist_get_data(sched_data->workers, node);
        PRS_ASSERT(!worker);
        worker = sched_worker->worker;
    }

    return prs_worker_signal(worker);
}

static prs_result_t prs_sched_swcoop_add(struct prs_sched_data* sched_data, struct prs_task* task)
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

    return prs_sched_swcoop_ready(sched_data, task);
}

static prs_result_t prs_sched_swcoop_remove(struct prs_sched_data* sched_data, struct prs_task* task)
{
    PRS_PRECONDITION(sched_data);
    PRS_PRECONDITION(task);
    PRS_PRECONDITION(task->sched_userdata);

    PRS_FTRACE("%s (%u)", task->name, task->id);

    const enum prs_task_state prev_state = prs_task_get_state(task);
    if (prev_state != PRS_TASK_STATE_STOPPED) {
        struct prs_sched_swcoop* sched = sched_data->userdata;
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

struct prs_sched_ops* prs_sched_swcoop_ops(void)
{
    static struct prs_sched_ops s_sched_swcoop_ops = {
        .init = prs_sched_swcoop_init,
        .uninit = prs_sched_swcoop_uninit,
        .add = prs_sched_swcoop_add,
        .remove = prs_sched_swcoop_remove,
        .get_next = prs_sched_swcoop_get_next,
        .ready = prs_sched_swcoop_ready
    };
    return &s_sched_swcoop_ops;
}



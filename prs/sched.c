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
 *  This file contains scheduler definitions.
 *
 *  In PRS, a scheduler runs on top of an operating system thread (worker) and runs multiple tasks in an order defined
 *  by the scheduling algorithm.
 *
 *  This file merely contains code to manage (wrap) the scheduling algorithm. The scheduling algorithm is chosen by the
 *  caller by specifying the scheduling operations (\ref prs_sched_ops) in the \p params field of the
 *  \ref prs_sched_create function.
 */

#include <stddef.h>

#include <prs/pal/malloc.h>
#include <prs/pal/thread.h>
#include <prs/assert.h>
#include <prs/clock.h>
#include <prs/dllist.h>
#include <prs/error.h>
#include <prs/god.h>
#include <prs/log.h>
#include <prs/name.h>
#include <prs/object.h>
#include <prs/sched.h>
#include <prs/str.h>
#include <prs/timer.h>
#include <prs/worker.h>

#include "task.h"

static struct prs_name* s_prs_sched_name = 0;

struct prs_sched {
    prs_sched_id_t                      id;
    char                                name[PRS_MAX_SCHED_NAME];

    struct prs_sched_ops                ops;

    struct prs_sched_data               sched_data;
};

static void prs_sched_object_free(void* object)
{
    struct prs_sched* sched = object;

    prs_pal_free(sched);
}

static void prs_sched_object_print(void* object, void* userdata, void (*fct)(void*, const char*, ...))
{
    struct prs_sched* sched = object;

    fct(userdata, "Sched %s id=%u\n",
        sched->name,
        sched->id);
}

static struct prs_object_ops s_prs_sched_object_ops = {
    .destroy = 0,
    .free = prs_sched_object_free,
    .print = prs_sched_object_print
};

static prs_bool_t prs_sched_get_next(void* userdata, struct prs_task* current_task, struct prs_task** next_task)
{
    struct prs_sched_worker* sched_worker = userdata;
    struct prs_sched* sched = sched_worker->sched;
    return sched->ops.get_next(sched_worker, current_task, next_task);
}

/**
 * \brief
 *  Creates a scheduler.
 * \param params
 *  Scheduler parameters.
 * \param id
 *  Pointer that will receive the object ID of the scheduler.
 */
prs_result_t prs_sched_create(struct prs_sched_create_params* params, prs_sched_id_t* id)
{
    if (!s_prs_sched_name) {
        struct prs_name_create_params name_params = {
            .max_entries = PRS_MAX_OBJECTS * 4,
            .string_offset = offsetof(struct prs_sched, name)
        };
        s_prs_sched_name = prs_name_create(&name_params);
    }

    prs_result_t result = PRS_OK;

    struct prs_sched* sched = prs_pal_malloc_zero(sizeof(struct prs_sched));
    if (!sched) {
        result = PRS_OUT_OF_MEMORY;
        goto cleanup;
    }
    sched->id = PRS_OBJECT_ID_INVALID;

    sched->sched_data.workers = prs_dllist_create();
    if (!sched->sched_data.workers) {
        result = PRS_OUT_OF_MEMORY;
        goto cleanup;
    }

    prs_str_copy(sched->name, params->name, sizeof(sched->name));
    sched->ops = params->ops;

    sched->id = prs_god_alloc_and_lock(sched, &s_prs_sched_object_ops);
    if (sched->id == PRS_OBJECT_ID_INVALID) {
        result = PRS_OUT_OF_MEMORY;
        goto cleanup;
    }

    result = prs_name_alloc(s_prs_sched_name, sched->id);
    if (result != PRS_OK) {
        goto cleanup;
    }

    result = sched->ops.init(&sched->sched_data, params->userdata);
    if (result != PRS_OK) {
        goto cleanup;
    }

    *id = sched->id;

    return result;

    cleanup:

    if (sched) {
        if (sched->id != PRS_OBJECT_ID_INVALID) {
            prs_name_free(s_prs_sched_name, sched->id);
            prs_god_unlock(sched->id);
        }
        if (sched->sched_data.workers) {
            prs_dllist_destroy(sched->sched_data.workers);
        }
        prs_pal_free(sched);
    }

    return result;
}

/**
 * \brief
 *  Stops a scheduler.
 * \param id
 *  Scheduler object ID.
 * \note
 *  A scheduler can only be stopped if it was started with \ref prs_sched_start beforehand.
 */
prs_result_t prs_sched_stop(prs_sched_id_t id)
{
    prs_result_t result = PRS_OK;
    struct prs_sched* sched = prs_god_lock(id);
    if (!sched) {
        return PRS_UNKNOWN;
    }

    prs_dllist_foreach(sched->sched_data.workers, node) {
        struct prs_sched_worker* sched_worker = prs_dllist_get_data(sched->sched_data.workers, node);
        prs_worker_stop(sched_worker->worker);
    }

    return result;
}

/**
 * \brief
 *  Destroys a scheduler.
 * \param id
 *  Scheduler object ID.
 * \note
 *  If the scheduler was started, it must be stopped with \ref prs_sched_stop beforehand.
 */
prs_result_t prs_sched_destroy(prs_sched_id_t id)
{
    prs_result_t result = PRS_OK;
    struct prs_sched* sched = prs_god_lock(id);
    if (!sched) {
        return PRS_UNKNOWN;
    }

    prs_dllist_foreach(sched->sched_data.workers, node) {
        struct prs_sched_worker* sched_worker = prs_dllist_get_data(sched->sched_data.workers, node);
        prs_worker_destroy(sched_worker->worker);
    }

    result = sched->ops.uninit(&sched->sched_data);

    prs_name_free(s_prs_sched_name, id);
    prs_god_unlock(id);
    prs_dllist_destroy(sched->sched_data.workers);
    prs_pal_free(sched);

    return result;
}

/**
 * \brief
 *  Starts a scheduler.
 * \param id
 *  Scheduler object ID.
 */
prs_result_t prs_sched_start(prs_sched_id_t id)
{
    prs_result_t result = PRS_OK;
    struct prs_sched* sched = prs_god_lock(id);
    if (!sched) {
        return PRS_UNKNOWN;
    }

    prs_dllist_foreach(sched->sched_data.workers, node) {
        struct prs_sched_worker* sched_worker = prs_dllist_get_data(sched->sched_data.workers, node);
        result = prs_worker_start(sched_worker->worker);
        if (result != PRS_OK) {
            goto end;
        }
    }

    end:

    prs_god_unlock(id);

    return result;
}

/**
 * \brief
 *  Adds a PAL thread to a scheduler.
 * \param id
 *  Scheduler object ID.
 * \param pal_thread
 *  PAL thread to add to the scheduler.
 */
prs_result_t prs_sched_add_thread(prs_sched_id_t id, struct prs_pal_thread* pal_thread)
{
    prs_result_t result = PRS_OK;
    struct prs_sched* sched = prs_god_lock(id);
    if (!sched) {
        return PRS_UNKNOWN;
    }

    struct prs_sched_worker* sched_worker = prs_pal_malloc_zero(sizeof(*sched_worker));
    if (!sched_worker) {
        result = PRS_OUT_OF_MEMORY;
        goto error;
    }

    result = prs_dllist_insert_before(sched->sched_data.workers, 0, sched_worker);
    if (result != PRS_OK) {
        goto cleanup;
    }

    sched_worker->sched = sched;

    struct prs_worker_create_params params = {
        .pal_thread = pal_thread,
        .userdata = sched_worker,
        .ops = {
            .get_next = prs_sched_get_next
        }
    };
    prs_worker_id_t worker_id;
    result = prs_worker_create(&params, &worker_id);
    if (result != PRS_OK) {
        goto error;
    }

    struct prs_worker* worker = prs_god_lock(worker_id);
    /*
     * Note: If can't get a reference to the worker here, we cannot destroy it. This shouldn't ever happen unless there
     * is a serious issue preventing the directory to work properly.
     */
    PRS_FATAL_WHEN(!worker);

    sched_worker->worker = worker;
    sched_worker->sched_data = &sched->sched_data;

    goto cleanup;

    error:

    if (sched_worker) {
        if (sched_worker->sched) {
            prs_dllist_remove(sched->sched_data.workers, prs_dllist_begin(sched->sched_data.workers));
        }
        prs_pal_free(sched_worker);
    }

    cleanup:

    prs_god_unlock(id);

    return result;
}

/**
 * \brief
 *  Adds a task to a scheduler.
 * \param id
 *  Scheduler object ID.
 * \param task_id
 *  Task object ID.
 * \note
 *  The task may or may not be executed immediately depending on the scheduler algorithm.
 */
prs_result_t prs_sched_add_task(prs_sched_id_t id, prs_task_id_t task_id)
{
    struct prs_task* task = 0;
    prs_result_t result = PRS_OK;

    struct prs_sched* sched = prs_god_lock(id);
    if (!sched) {
        result = PRS_UNKNOWN;
        goto cleanup;
    }

    task = prs_god_lock(task_id);
    if (!task) {
        result = PRS_UNKNOWN;
        goto cleanup;
    }

    result = sched->ops.add(&sched->sched_data, task);
    if (result != PRS_OK) {
        goto cleanup;
    }

    task->sched_id = sched->id;

    PRS_FTRACE("%s (%u): task %s (%u) prio %u", sched->name, sched->id, task->name, task->id, task->prio);

    return result;

    cleanup:

    if (task) {
        prs_god_unlock(task_id);
    }

    if (sched) {
        prs_god_unlock(id);
    }

    return result;
}

/**
 * \brief
 *  Removes a task to a scheduler.
 * \param id
 *  Scheduler object ID.
 * \param task_id
 *  Task object ID.
 * \note
 *  The task may or may not be removed immediately depending on the scheduler algorithm. It is not possible to move
 *  a task from a scheduler to another: once it has started executing to a scheduler, it has to finish its execution on
 *  the same scheduler or not be run ever again.
 */
prs_result_t prs_sched_remove_task(prs_sched_id_t id, prs_task_id_t task_id)
{
    prs_result_t result = PRS_OK;
    struct prs_task* task = 0;

    struct prs_sched* sched = prs_god_lock(id);
    if (!sched) {
        result = PRS_UNKNOWN;
        goto cleanup;
    }

    task = prs_god_lock(task_id);
    if (!task) {
        result = PRS_UNKNOWN;
        goto cleanup;
    }

    PRS_FTRACE("%s (%u): task %s (%u)", sched->name, sched->id, task->name, task->id);

    result = sched->ops.remove(&sched->sched_data, task);
    if (result != PRS_OK) {
        goto cleanup;
    }

    prs_god_unlock(task_id);
    prs_god_unlock(id);

    cleanup:

    if (task) {
        prs_god_unlock(task_id);
    }

    if (sched) {
        prs_god_unlock(id);
    }

    return result;
}

/**
 * \brief
 *  Run the scheduler algorithm to execute the next task. It will return once the current task is in the
 *  \ref PRS_TASK_STATE_READY state.
 * \note
 *  This function must be called after a task was put into the \ref PRS_TASK_STATE_BLOCKED state.
 */
void prs_sched_schedule(void)
{
    struct prs_worker* worker = prs_worker_current();
    prs_worker_schedule(worker);
}

/**
 * \brief
 *  Indicate to the scheduler that the specified task is now in the \ref PRS_TASK_STATE_READY state from the
 *  \ref PRS_TASK_STATE_BLOCKED state.
 */
prs_result_t prs_sched_ready(struct prs_task* task)
{
    struct prs_sched* sched = prs_god_lock(task->sched_id);
    PRS_ASSERT(sched);
    const prs_result_t result = sched->ops.ready(&sched->sched_data, task);
    prs_god_unlock(task->sched_id);
    return result;
}

/**
 * \brief
 *  Yield the current task so that the scheduler can choose another one to execute (if need be).
 */
void prs_sched_yield(void)
{
    struct prs_worker* worker = prs_worker_current();
    struct prs_task* task = prs_worker_get_current_task(worker);
    prs_task_block(task);
    prs_task_change_state(task, PRS_TASK_STATE_BLOCKED, PRS_TASK_STATE_READY);
    prs_sched_ready(task);
    prs_worker_schedule(worker);
}

/**
 * \brief
 *  Stop the current task execution for the number of ticks specified.
 */
void prs_sched_sleep(prs_ticks_t ticks)
{
    struct prs_worker* worker = prs_worker_current();
    struct prs_task* task = prs_worker_get_current_task(worker);
    struct prs_event* event = prs_event_create(task, 1);
    PRS_FATAL_WHEN(!event);
    struct prs_timer_entry* timer_entry = prs_timer_queue(prs_clock_timer(), event, 1, ticks);
    PRS_ASSERT(timer_entry);
    prs_worker_schedule(worker);
    prs_timer_cancel(prs_clock_timer(), timer_entry);
}

/**
 * \brief
 *  Stop the current task execution.
 */
void prs_sched_block(void)
{
    struct prs_worker* worker = prs_worker_current();
    struct prs_task* task = prs_worker_get_current_task(worker);
    prs_task_change_state(task, PRS_TASK_STATE_RUNNING, PRS_TASK_STATE_BLOCKED);
    prs_worker_schedule(worker);
}

/**
 * \brief
 *  Find a scheduler by name.
 */
prs_object_id_t prs_sched_find(const char* name)
{
    PRS_PRECONDITION(s_prs_sched_name);
    return prs_name_find(s_prs_sched_name, name);
}

/**
 * \brief
 *  Find a scheduler by name and lock it.
 */
struct prs_sched* prs_sched_find_and_lock(const char* name)
{
    PRS_PRECONDITION(s_prs_sched_name);
    const prs_object_id_t object_id = prs_name_find_and_lock(s_prs_sched_name, name);
    return prs_god_find(object_id);
}


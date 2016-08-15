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
 *  This file contains task definitions.
 *
 *  The task implements a user-space fiber meant to be executed by a scheduler. It owns its own stack and register
 *  context. It also has a dedicated message queue.
 *
 *  The task's stack has a fixed size that is decided by the user application. When the stack overflows, an exception
 *  is raised. The default exception handler will increase the stack size by a multiple of the system's page size.
 *
 *  Tasks are bound to a single scheduler for their full lifetime. It is not possible to migrate a task from a
 *  scheduler to another.
 *
 *  Tasks run in two mutually exclusive modes: interruptible and non-interruptible:
 *    - In interruptible mode, the task must run code that does not have run to completion requirements, i.e. that can
 *      be stopped at any time without causing a system failure. In this mode, the PRS scheduler can preempt the task.
 *      For example, code using only the PR API can run in interruptible mode. Code that is not in the PRS' processes
 *      address space cannot be run in interruptible mode, as it is not possible to guarantee that it is preemptible
 *      and non-blocking.
 *    - In non-interruptible mode, the task cannot be preempted and can run any code.
 *
 *  Switching between interruptible and non-interruptible modes is done by the \ref prs_worker_int_disable and
 *  \ref prs_worker_int_enable functions.
 *
 *  Tasks will often be blocked as they will be waiting for a message, semaphore or timer. A token is used as a state
 *  version number to ensure that other workers do not unblock the task for an event that the task is no longer waiting
 *  for. The \ref prs_task_block and \ref prs_task_unblock functions handle the state change and tokens for this case.
 */

#include <stddef.h>

#include <prs/alloc/stack.h>
#include <prs/pal/atomic.h>
#include <prs/pal/context.h>
#include <prs/pal/os.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/god.h>
#include <prs/gpd.h>
#include <prs/log.h>
#include <prs/name.h>
#include <prs/object.h>
#include <prs/pd.h>
#include <prs/proc.h>
#include <prs/str.h>
#include <prs/task.h>
#include <prs/worker.h>
#include <prs/svc/proc.msg>

#include "task.h"

#define PRS_TASK_STATE_BITS             4
#define PRS_TASK_CAUSE_BITS             8
#define PRS_TASK_COUNT_BITS             (sizeof(prs_task_token_t) * 8 - PRS_TASK_STATE_BITS - PRS_TASK_CAUSE_BITS)

#define PRS_TASK_STATE_MASK             ((prs_task_token_t)(1 << PRS_TASK_STATE_BITS) - 1)
#define PRS_TASK_CAUSE_MASK             (((prs_task_token_t)(1 << PRS_TASK_CAUSE_BITS) - 1) << PRS_TASK_STATE_BITS)
#define PRS_TASK_COUNT_MASK             (~(prs_task_token_t)(PRS_TASK_STATE_MASK | PRS_TASK_CAUSE_MASK))

#define PRS_TASK_GET_STATE(token)       ((enum prs_task_state)((token) & PRS_TASK_STATE_MASK))
#define PRS_TASK_GET_CAUSE(token)       ((prs_uint8_t)(((token) & PRS_TASK_CAUSE_MASK) >> PRS_TASK_STATE_BITS))
#define PRS_TASK_GET_COUNT(token)       ((token & PRS_TASK_COUNT_MASK) >> (PRS_TASK_STATE_BITS + PRS_TASK_CAUSE_BITS))
#define PRS_TASK_SET_TOKEN(s, ca, co)   (((prs_task_token_t)(co) << (PRS_TASK_STATE_BITS + PRS_TASK_CAUSE_BITS)) | \
                                            (((prs_task_token_t)(ca) << PRS_TASK_STATE_BITS) & PRS_TASK_CAUSE_MASK) | \
                                            ((prs_task_token_t)(s) & PRS_TASK_STATE_MASK))
#define PRS_TASK_COUNT_INCREMENT        ((prs_task_token_t)(1 << (PRS_TASK_STATE_BITS + PRS_TASK_CAUSE_BITS)))

static struct prs_name* s_prs_task_name = 0;

static void prs_task_object_destroy(void* object)
{
    prs_task_destroy(object);
}

static void prs_task_object_free(void* object)
{
    struct prs_task* task = object;

    if (task->msgq) {
        prs_msgq_destroy(task->msgq);
    }
    if (task->context) {
        prs_pal_context_free(task->context);
    }
    if (task->stack) {
        prs_stack_destroy(task->stack);
    }
    prs_pal_free(task);
}

static void prs_task_object_print(void* object, void* userdata, void (*fct)(void*, const char*, ...))
{
    struct prs_task* task = object;

    fct(userdata, "Task %s id=%u prio=%u state=%d sched_id=%u\n",
        task->name,
        task->id,
        task->prio,
        prs_task_get_state(task),
        task->sched_id);
}

static struct prs_object_ops s_prs_task_object_ops = {
    .destroy = prs_task_object_destroy,
    .free = prs_task_object_free,
    .print = prs_task_object_print
};

static void prs_task_entry(void* userdata)
{
    struct prs_worker* worker = prs_worker_current();
    PRS_ASSERT(worker);
    struct prs_task* task = userdata;
    PRS_ASSERT(task);

    PRS_FTRACE("%s (%u) entry=%p userdata=%p stack=%p", task->name, task->id, task->entry, task->userdata, task->stack);

    prs_worker_int_enable(worker);
    task->entry(task->userdata);
    prs_worker_int_disable(worker);

    prs_task_destroy(task);

    prs_worker_schedule(worker);
}

/**
 * \brief
 *  Creates a task.
 * \param params
 *  Task parameters.
 */
struct prs_task* prs_task_create(struct prs_task_create_params* params)
{
    if (!s_prs_task_name) {
        struct prs_name_create_params name_params = {
            .max_entries = PRS_MAX_OBJECTS * 4,
            .string_offset = offsetof(struct prs_task, name)
        };
        s_prs_task_name = prs_name_create(&name_params);
    }

    struct prs_task* task = prs_pal_malloc_zero(sizeof(*task));
    PRS_ERROR_IF (!task) {
        goto cleanup;
    }

    task->id = prs_god_alloc_and_lock(task, &s_prs_task_object_ops);
    PRS_ERROR_IF (task->id == PRS_OBJECT_ID_INVALID) {
        goto cleanup;
    }

    const prs_size_t stack_size = (params->stack_size ? params->stack_size : prs_pal_os_get_page_size());

    task->stack = prs_stack_create(stack_size, &task->stack_size);
    PRS_ERROR_IF (!task->stack) {
        goto cleanup;
    }

    task->context = prs_pal_context_alloc();
    PRS_ERROR_IF (!task->context) {
        goto cleanup;
    }

    struct prs_msgq_create_params msgq_params = {
        .pd = prs_gpd_get()
    };
    task->msgq = prs_msgq_create(&msgq_params);

    /* Inherit the process pointer. If there is no worker, this is probably the first task, ever. */
    struct prs_worker* worker = prs_worker_current();
    if (worker) {
        struct prs_task* current_task = prs_worker_get_current_task(worker);
        PRS_ASSERT(current_task);
        if (current_task->proc_id) {
            const prs_result_t result = prs_task_set_proc(task, current_task->proc_id);
            PRS_ERROR_IF (result != PRS_OK) {
                goto cleanup;
            }
        }
    }

    prs_str_copy(task->name, params->name, sizeof(task->name));
    task->prio = params->prio;
    task->userdata = params->userdata;
    task->entry = params->entry;
    prs_task_change_state(task, 0, PRS_TASK_STATE_STOPPED);

    prs_pal_context_make(task->context, task->stack, prs_task_entry, 1, task);

    prs_result_t result = prs_name_alloc(s_prs_task_name, task->id);
    if (result != PRS_OK) {
        goto cleanup;
    }

    return task;

    cleanup:

    if (task) {
        if (task->id) {
            prs_god_unlock(task->id);
        } else {
            prs_pal_free(task);
        }
    }

    return 0;
}

/**
 * \brief
 *  Returns the object ID of a task.
 * \param task
 *  Task to get the object ID from.
 */
prs_task_id_t prs_task_get_id(struct prs_task* task)
{
    return task->id;
}

/**
 * \brief
 *  Returns the current task.
 */
struct prs_task* prs_task_current(void)
{
    struct prs_worker* worker = prs_worker_current();
    if (worker) {
        return prs_worker_get_current_task(worker);
    }
    return 0;
}

/**
 * \brief
 *  Destroys a task.
 * \param task
 *  Task to destroy.
 */
void prs_task_destroy(struct prs_task* task)
{
    /* Use the name hash free result to find out if the task was already destroyed. */
    const prs_result_t result = prs_name_free(s_prs_task_name, task->id);
    if (result == PRS_OK) {
        PRS_FTRACE("%s (%u)", task->name, task->id);
        if (task->sched_id) {
            prs_sched_remove_task(task->sched_id, task->id);
        }
        if (task->proc_id) {
            struct prs_proc* proc = prs_god_lock(task->proc_id);
            PRS_ASSERT(proc);
            const prs_task_id_t proc_task_id = prs_task_find(PRS_PROC_SVC_NAME);
            PRS_FATAL_WHEN(!proc_task_id);
            if (proc_task_id == prs_task_get_id(prs_task_current())) {
                prs_proc_unregister_object(proc, task->id);
            } else {
                prs_proc_send_unreg_object(proc_task_id, task->proc_id, task->id, PRS_FALSE);
            }
            prs_god_unlock(task->proc_id);
            prs_god_unlock(task->proc_id);
        }
        prs_god_unlock(task->id);
    } else {
        PRS_FTRACE("%s (%u) already destroyed", task->name, task->id);
    }
}

/**
 * \brief
 *  Returns the priority of a task.
 * \param task
 *  Task to get the priority from.
 */
prs_task_prio_t prs_task_get_prio(struct prs_task* task)
{
    return task->prio;
}

/**
 * \brief
 *  Changes the priority of a task.
 * \param task
 *  Task to set the priority to.
 * \param prio
 *  Priority to set.
 * \note
 *  Has no effect if the scheduler for the task does not support priorities. Only works if \p task is the current
 *  running task.
 */
void prs_task_set_prio(struct prs_task* task, prs_task_prio_t prio)
{
    task->prio = prio;

    struct prs_worker* worker = prs_worker_current();
    PRS_ASSERT(worker);
    struct prs_task* current_task = prs_worker_get_current_task(worker);
    PRS_ASSERT(current_task);

    if (task == current_task) {
        /*
         * prs_worker_signal() only sets the pending interrupt flag. Before control is returned to the
         * prs_task_set_prio() caller, interrupts will be enabled again, which will call the task prologue that will
         * re-schedule tasks according to the priority change above.
         */
        prs_worker_signal(worker);
    } else {
        /*
         * There is no mechanism at this point to change the priority of a task that is currently running on another
         * worker.
         */
        PRS_ERROR("ERROR: prs_task_set_prio() can only be called from the target task");
    }
}

/**
 * \brief
 *  Finds a task by name.
 * \param name
 *  Name of the task to search for.
 * \return
 *  Task object ID that has the specified name, or \ref PRS_OBJECT_ID_INVALID if it was not found.
 */
prs_object_id_t prs_task_find(const char* name)
{
    PRS_PRECONDITION(s_prs_task_name);
    return prs_name_find(s_prs_task_name, name);
}

/**
 * \brief
 *  Finds a task by name and locks it.
 * \param name
 *  Name of the task to search for.
 * \return
 *  Task object ID that has the specified name, or \ref PRS_OBJECT_ID_INVALID if it was not found.
 */
struct prs_task* prs_task_find_and_lock(const char* name)
{
    PRS_PRECONDITION(s_prs_task_name);
    const prs_object_id_t object_id = prs_name_find_and_lock(s_prs_task_name, name);
    return prs_god_find(object_id);
}

/**
 * \brief
 *  Returns the state of the specified task.
 * \param task
 *  Task to get the state from.
 */
enum prs_task_state prs_task_get_state(struct prs_task* task)
{
    PRS_PRECONDITION(task);
    const prs_task_token_t token = prs_pal_atomic_load(&task->state);
    return PRS_TASK_GET_STATE(token);
}

/**
 * \brief
 *  Changes the state of the specified task.
 * \param task
 *  Task for which the state will be modified.
 * \param expected_state
 *  Expected task state before the change.
 * \param new_state
 *  New task state.
 * \note
 *  The function uses \ref PRS_ASSERT to validate that the expected state (\p expected_state) is indeed the correct
 *  state before the change.
 */
void prs_task_change_state(struct prs_task* task, enum prs_task_state expected_state, enum prs_task_state new_state)
{
    PRS_PRECONDITION(task);
    const prs_task_token_t token = prs_pal_atomic_load(&task->state);
    const enum prs_task_state state_before = PRS_TASK_GET_STATE(token);
    PRS_ASSERT(state_before == expected_state);
    const prs_task_token_t new_token =
        PRS_TASK_SET_TOKEN(new_state, PRS_TASK_GET_CAUSE(token), PRS_TASK_GET_COUNT(token) + PRS_TASK_COUNT_INCREMENT);
    prs_pal_atomic_store(&task->state, new_token);
}

/**
 * \brief
 *  Sets the specified task's state to \ref PRS_TASK_STATE_BLOCKED and returns a token.
 * \param task
 *  Task for which the state will be modified.
 * \return
 *  The token that must be passed to \ref prs_task_unblock to unblock the task.
 * \note
 *  The function uses \ref PRS_ASSERT to validate that the expected state (\p expected_state) is not
 *  \ref PRS_TASK_STATE_BLOCKED.
 */
prs_task_token_t prs_task_block(struct prs_task* task)
{
    PRS_PRECONDITION(task);

    const prs_task_token_t token = prs_pal_atomic_load(&task->state);
    const enum prs_task_state state_before = PRS_TASK_GET_STATE(token);
    PRS_ASSERT(state_before != PRS_TASK_STATE_BLOCKED);

    const prs_task_token_t new_token =
        PRS_TASK_SET_TOKEN(PRS_TASK_STATE_BLOCKED, 0, PRS_TASK_GET_COUNT(token) + PRS_TASK_COUNT_INCREMENT);
    prs_pal_atomic_store(&task->state, new_token);

    return new_token;
}

/**
 * \brief
 *  Sets the specified task's state to \ref PRS_TASK_STATE_READY and puts it back in the scheduler ready queue if the
 *  token matches the task's current state.
 * \param task
 *  Task for which the state will be modified.
 * \param token
 *  The token obtained from \ref prs_task_block.
 * \param cause
 *  An indentifier representing the source of the event that causes the task to unblock.
 * \return
 *  \ref PRS_TRUE if the token matched the task's current state and the task was unblocked.
 *  \ref PRS_FALSE if the token did not match the task's current state and the task was not unblocked.
 */
prs_bool_t prs_task_unblock(struct prs_task* task, prs_task_token_t token, prs_uint8_t cause)
{
    PRS_PRECONDITION(task);

    const prs_bool_t is_current_task = PRS_BOOL(task == prs_task_current());
    const enum prs_task_state new_state = is_current_task ? PRS_TASK_STATE_RUNNING : PRS_TASK_STATE_READY;

    prs_task_token_t expected_token = token;
    const prs_task_token_t new_token =
        PRS_TASK_SET_TOKEN(new_state, cause, PRS_TASK_GET_COUNT(token) + PRS_TASK_COUNT_INCREMENT);
    const prs_bool_t result = PRS_BOOL(prs_pal_atomic_compare_exchange_strong(&task->state, &expected_token, new_token));
    if (result) {
        if (!is_current_task) {
            prs_sched_ready(task);
        }
    }
    return result;
}

/**
 * \brief
 *  Returns the last cause identifier that was set by \ref prs_task_unblock.
 */
prs_uint8_t prs_task_get_last_unblock_cause(struct prs_task* task)
{
    PRS_PRECONDITION(task);
    const prs_task_token_t token = prs_pal_atomic_load(&task->state);
    return PRS_TASK_GET_CAUSE(token);
}

/**
 * \brief
 *  Sets the process to which the specified task belongs to.
 *
 *  When no more tasks are running in a process, that process can be destroyed.
 * \param task
 *  Task to set the owning process for.
 * \param proc_id
 *  Object ID of the process.
 */
prs_result_t prs_task_set_proc(struct prs_task* task, prs_proc_id_t proc_id)
{
    struct prs_proc* proc = prs_god_lock(proc_id);
    if (proc) {
        if (task->proc_id) {
            const prs_task_id_t proc_task_id = prs_task_find(PRS_PROC_SVC_NAME);
            PRS_FATAL_WHEN(!proc_task_id);
            if (proc_task_id == prs_task_get_id(prs_task_current())) {
                prs_proc_unregister_object(proc, task->id);
            } else {
                prs_proc_send_unreg_object(proc_task_id, proc_id, task->id, PRS_FALSE);
            }
            prs_god_unlock(task->proc_id);
        }
        prs_proc_register_object(proc, task->id);
        task->proc_id = proc_id;
        return PRS_OK;
    } else {
        return PRS_NOT_FOUND;
    }
}

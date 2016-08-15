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
 *  This file contains the worker definitions.
 *
 *  In PRS, the worker handles the general execution flow of the scheduler on a PAL thread. It executes  scheduler
 *  requests and does the actual register context switches. It puts the thread into an idle mode when no tasks are left
 *  to run. It also handles switches between interruptible and non-interruptible execution modes. Finally and most
 *  importantly, it processes interrupt requests from other workers.
 *
 *  Context switches are only done when requested by the scheduler. The scheduler is only invoked in the following
 *  scenarios:
 *    - When the worker is executing for the first time.
 *    - When a task is blocked or has no more code to execute.
 *    - When the worker exits idle mode.
 *    - When interruptible mode is to be enabled but the interrupt pending flag is raised.
 *
 *  When the scheduler is out of tasks to run, the worker goes into idle mode. In idle mode, an operating system
 *  primitive (for example, a POSIX semaphore on Linux) is used to stop the PAL thread from executing. When an event
 *  occurs on another worker (for example, a message sent to the message queue on one of the scheduler's tasks), it
 *  calls the ready callback on the scheduler which in turn, instead of interrupting the worker, wakes it up from idle
 *  mode.
 *
 *  When the worker is already busy executing other tasks and an event occurs on a task belonging to the same
 *  scheduler, the worker can be interrupted when in interruptible mode. Interrupting the worker stops its execution in
 *  its current context, puts the worker in non-interruptible mode and adds a new stack frame on top of the running
 *  code. This new stack frame contains the information necessary for the running task to invoke the scheduler and
 *  return to interruptible mode. Once in interruptible mode, the task may resume its execution.
 *
 *  Non-interruptible mode has three uses:
 *    -# Protect PRS code from being interrupted within the same worker. As such, it acts like a critical section.
 *    -# Lets the application define critical sections within the same worker that cannot be interrupted by PRS (but
 *       still can be interrupted by the underlying operating system).
 *    -# Prevents external code, such as syscalls, to be interrupted by PRS. External code could hold a mutex which
 *       would prevent other workers from calling the same code without blocking. Also, it's usually not possible to
 *       add a stack frame on top of syscalls as they are executing in kernel mode.
 *
 *  Using thread-local storage, the worker currently executing code is accessible from anywhere by calling
 *  \ref prs_worker_current.
 */

#include <prs/pal/atomic.h>
#include <prs/pal/context.h>
#include <prs/pal/cycles.h>
#include <prs/pal/excp.h>
#include <prs/pal/malloc.h>
#include <prs/pal/wls.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/god.h>
#include <prs/log.h>
#include <prs/proc.h>
#include <prs/rtc.h>
#include <prs/worker.h>

#include "task.h"

#define PRS_WORKER_FLAG_INTERRUPTIBLE   ((prs_worker_flags_t)0x00000001)
#define PRS_WORKER_FLAG_INTERRUPT_PENDING \
                                        ((prs_worker_flags_t)0x00000002)
#define PRS_WORKER_FLAG_IDLE            ((prs_worker_flags_t)0x00000004)
#define PRS_WORKER_FLAG_STOP            ((prs_worker_flags_t)0x00000008)

typedef prs_uint32_t prs_worker_flags_t;

struct prs_worker {
    prs_worker_id_t                     id;

    struct prs_pal_thread*              pal_thread;

    void*                               userdata;
    struct prs_worker_ops               ops;

    PRS_ATOMIC prs_worker_flags_t       flags;

    PRS_ATOMIC prs_task_id_t            current_task_id;
    struct prs_task*                    current_task;

    struct prs_pal_context*             exit_context;
};

static void prs_worker_object_free(void* object)
{
    struct prs_worker* worker = object;

    prs_pal_free(worker);
}

static void prs_worker_object_print(void* object, void* userdata, void (*fct)(void*, const char*, ...))
{
    struct prs_worker* worker = object;

    fct(userdata, "Worker id=%u\n",
        worker->id);
}

static struct prs_object_ops s_prs_worker_object_ops = {
    .destroy = 0,
    .free = prs_worker_object_free,
    .print = prs_worker_object_print
};

static void prs_worker_idle(struct prs_worker* worker)
{
    prs_worker_flags_t flags = 0;
    const prs_worker_flags_t new_flags = PRS_WORKER_FLAG_IDLE | PRS_WORKER_FLAG_INTERRUPT_PENDING;
    if (prs_pal_atomic_compare_exchange_strong(&worker->flags, &flags, new_flags)) {
        PRS_FTRACE("(%u) suspend thread", worker->id);
        prs_pal_thread_suspend(worker->pal_thread);
        PRS_FTRACE("(%u) back from suspend thread", worker->id);
        PRS_ASSERT(!(prs_pal_atomic_load(&worker->flags) & PRS_WORKER_FLAG_IDLE));
    }
}

static struct prs_task* prs_worker_set_current_task(struct prs_worker* worker, struct prs_task* task)
{
    struct prs_task* prev_task = worker->current_task;
    worker->current_task = task;
    prs_pal_atomic_store(&worker->current_task_id, task ? task->id : PRS_OBJECT_ID_INVALID);
    return prev_task;
}

static void prs_worker_schedule_internal(struct prs_worker* worker, prs_bool_t check_flags,
    struct prs_pal_context* exit_context)
{
    struct prs_pal_context* prev_context = 0;
    prs_bool_t check = check_flags;
    for (;;) {
        prs_worker_flags_t flags = PRS_WORKER_FLAG_INTERRUPT_PENDING;
        if (check) {
            flags = prs_pal_atomic_fetch_and(&worker->flags, ~PRS_WORKER_FLAG_INTERRUPT_PENDING);
        }
        check = PRS_TRUE;
        if (flags & PRS_WORKER_FLAG_STOP) {
            PRS_FTRACE("(%u) stop flag set, switch to worker thread stack", worker->id);
            if (exit_context) {
                break;
            } else {
                prs_pal_context_swap(0, worker->exit_context);
            }
            PRS_ASSERT(PRS_FALSE);
        } else if (flags & PRS_WORKER_FLAG_INTERRUPT_PENDING) {
            /*
             * Here, we set the current task to zero while we compute the next task to schedule. This is necessary,
             * otherwise other workers wanting to interrupt this worker would not be able to know the actual priority
             * level of the current task (which is none while we are re-scheduling).
             */
            struct prs_task* prev_task = prs_worker_set_current_task(worker, 0);
            if (prev_task) {
                prev_context = prev_task->context;
            }
            struct prs_task* next_task = 0;
            const prs_bool_t switch_to_next = worker->ops.get_next(worker->userdata, prev_task, &next_task);
            if (next_task) {
                prs_worker_set_current_task(worker, next_task);
                if (prev_task != next_task) {
                    struct prs_pal_context* save_context = prev_context ? prev_context : exit_context;
                    if (prev_task) {
                        PRS_FTRACE("(%u) switching to task %s (%u)", worker->id, next_task->name, next_task->id);
                    } else {
                        PRS_FTRACE("(%u) entry to task %s (%u)", worker->id, next_task->name, next_task->id);
                    }
                    if (save_context == exit_context) {
                        PRS_FTRACE("(%u) save to exit context", worker->id);
                    }
                    prs_pal_context_swap(save_context, next_task->context);
                    if (save_context == exit_context) {
                        PRS_FTRACE("(%u) back from exit context", worker->id);
                    }
                }
            } else {
                if (switch_to_next) {
                    prs_worker_idle(worker);
                    continue;
                } else {
                    PRS_FTRACE("(%u) switch to worker thread stack as requested by scheduler", worker->id);
                    prs_pal_context_swap(0, worker->exit_context);
                    PRS_ASSERT(PRS_FALSE);
                }
            }
        }
        break;
    }
}

static void prs_worker_entry(void* userdata)
{
    struct prs_worker* worker = userdata;
    PRS_ASSERT(worker);

    prs_wls_worker_init(worker);
    prs_wls_set(worker);
    prs_pal_excp_init_worker(worker);
    worker->exit_context = prs_pal_context_alloc();

    PRS_FTRACE("(%u) entry", worker->id);

    for (;;) {
        const prs_worker_flags_t flags = prs_pal_atomic_load(&worker->flags);
        if (flags & PRS_WORKER_FLAG_STOP) {
            break;
        }
        prs_worker_schedule_internal(worker, PRS_FALSE, worker->exit_context);
    }

    PRS_FTRACE("(%u) exit", worker->id);

    prs_pal_context_free(worker->exit_context);
    worker->exit_context = 0;

    prs_pal_excp_uninit_worker(worker);
    prs_wls_worker_uninit(worker);
}

static void prs_worker_task_prologue(struct prs_worker* worker)
{
    PRS_PRECONDITION(worker);
    PRS_PRECONDITION(!(prs_pal_atomic_load(&worker->flags) & PRS_WORKER_FLAG_INTERRUPTIBLE));

    for (;;) {
        prs_worker_schedule_internal(worker, PRS_TRUE, 0);
        prs_worker_flags_t flags = 0;
        const prs_bool_t result = prs_pal_atomic_compare_exchange_strong(&worker->flags, &flags, PRS_WORKER_FLAG_INTERRUPTIBLE);
        if (result) {
            break;
        }
    }

    PRS_ASSERT(prs_pal_atomic_load(&worker->flags) & PRS_WORKER_FLAG_INTERRUPTIBLE);
}

static void prs_worker_interrupt_entry(void* userdata, struct prs_pal_context* context)
{
    PRS_PRECONDITION(userdata);
    struct prs_worker* worker = userdata;

    /* Is the task running with interrupts enabled? If so, disable them. */
    const prs_bool_t interrupts_enabled = prs_worker_int_disable(worker);
    if (interrupts_enabled) {
        /* Are we running out of the task's register context? */
        if (context) {
            /*
             * Add the task prologue to the task's context (and stack) so that it calls the scheduler back to find the
             * next task to run.
             */
            prs_pal_context_add(context, prs_worker_task_prologue, 1, worker);
        } else {
            /* If we're already running from the task's context, just run the prologue to re-enable interrupts. */
            prs_worker_task_prologue(worker);
        }
    } else {
        /*
         * Interrupts disabled. Nothing to do, as the thread will select a new task to run by itself when interrupts
         * are re-enabled.
         */
    }
}

static prs_bool_t prs_worker_interruptible(void* userdata, struct prs_pal_context* context)
{
    PRS_PRECONDITION(userdata);
    struct prs_worker* worker = userdata;

    const prs_worker_flags_t flags = prs_pal_atomic_load(&worker->flags);
    prs_bool_t result = PRS_BOOL(flags & PRS_WORKER_FLAG_INTERRUPTIBLE);
#if defined(PRS_RUN_TIME_CHECKING)
    if (result) {
        /* Verify if the instruction pointer is in kernel or PRS address space. If so, we can't interrupt. */
        if (context) {
            void* ptr = prs_pal_context_get_ip(context);
            /*
             * We can't use PRS_RTC_IF here, as it may raise an assertion depending on the configuration, which would
             * prevent PRS from exiting gracefully on Windows.
             */
            result = prs_proc_is_user_text(ptr);
            if (!result) {
                /*
                 * The following warning notifies the developer that some code that was interrupted was not protected
                 * by the prs_worker_int_disable()/prs_worker_int_enable() functions.
                 * Some non-PRS code cannot be guaranteed to be safe to use when interrupted abruptly by a signal or
                 * Windows' SuspendThread().
                 */
                prs_log_print("Warning: code at %p was not interruptible", ptr);
            }
        }
    }
#endif /* defined(PRS_RUN_TIME_CHECKING) */
    return result;
}

/**
 * \brief
 *  Creates a worker.
 * \param params
 *  Worker parameters.
 * \param id
 *  Pointer to the worker's object ID.
 */
prs_result_t prs_worker_create(struct prs_worker_create_params* params, prs_worker_id_t* id)
{
    prs_result_t result = PRS_OK;

    struct prs_worker* worker = prs_pal_malloc_zero(sizeof(struct prs_worker));
    if (!worker) {
        result = PRS_OUT_OF_MEMORY;
        goto cleanup;
    }

    worker->userdata = params->userdata;
    worker->ops = params->ops;
    worker->pal_thread = params->pal_thread;

    worker->id = prs_god_alloc_and_lock(worker, &s_prs_worker_object_ops);
    if (worker->id == PRS_OBJECT_ID_INVALID) {
        result = PRS_OUT_OF_MEMORY;
        goto cleanup;
    }

    *id = worker->id;

    return result;

    cleanup:

    if (worker) {
        prs_pal_free(worker);
    }

    return result;
}

/**
 * \brief
 *  Destroys a worker.
 */
prs_result_t prs_worker_destroy(struct prs_worker* worker)
{
    return PRS_NOT_IMPLEMENTED;
}

/**
 * \brief
 *  Starts a worker.
 * \param worker
 *  Worker to start.
 */
prs_result_t prs_worker_start(struct prs_worker* worker)
{
    struct prs_pal_thread_callback_params params = {
        .userdata = worker,
        .entry = prs_worker_entry,
        .interrupt = prs_worker_interrupt_entry,
        .interruptible = prs_worker_interruptible
    };
    prs_result_t result = prs_pal_thread_set_callback_params(worker->pal_thread, &params);
    if (result != PRS_OK) {
        goto cleanup;
    }

    prs_pal_atomic_store(&worker->flags, 0);

    result = prs_pal_thread_start(worker->pal_thread);

    cleanup:

    return result;
}

/**
 * \brief
 *  Stops a worker.
 * \param worker
 *  Worker to stop.
 */
prs_result_t prs_worker_stop(struct prs_worker* worker)
{
    PRS_FTRACE("(%u) stop", worker->id);

    prs_pal_atomic_fetch_or(&worker->flags, PRS_WORKER_FLAG_STOP);

    if (worker == prs_worker_current()) {
        PRS_FTRACE("(%u) swap exit current", worker->id);
        prs_pal_context_swap(0, worker->exit_context);
        return PRS_OK; /* Silence warning */
    } else {
        PRS_FTRACE("(%u) call interrupt for stop", worker->id);
        prs_result_t result = prs_worker_interrupt(worker);
        if (result == PRS_OK) {
            PRS_FTRACE("(%u) join for stop", worker->id);
            result = prs_worker_join(worker);
        } else {
            prs_log_print("prs_worker_stop(): couldn't interrupt worker");
        }

        return result;
    }
}

/**
 * \brief
 *  Waits for a worker to stop executing.
 * \param worker
 *  Worker to wait for.
 */
prs_result_t prs_worker_join(struct prs_worker* worker)
{
    return prs_pal_thread_join(worker->pal_thread);
}

static prs_result_t prs_worker_post(struct prs_worker* worker, prs_bool_t interrupt)
{
    PRS_PRECONDITION(worker);

    prs_worker_flags_t flags = prs_pal_atomic_fetch_or(&worker->flags, PRS_WORKER_FLAG_INTERRUPT_PENDING);
    if (flags & PRS_WORKER_FLAG_IDLE) {
        flags = prs_pal_atomic_fetch_and(&worker->flags, ~PRS_WORKER_FLAG_IDLE);
        if (flags & PRS_WORKER_FLAG_IDLE) {
            PRS_FTRACE("(%u) resume thread", worker->id);
            return prs_pal_thread_resume(worker->pal_thread);
        }
    }

    if (interrupt) {
        const prs_bool_t interruptible = PRS_BOOL(flags & PRS_WORKER_FLAG_INTERRUPTIBLE);
        if (interruptible) {
            if (!(flags & PRS_WORKER_FLAG_INTERRUPT_PENDING)) {
                /*
                 * We should never interrupt the current worker. If this code is called, we should be in a
                 * non-interruptible section. However, the assertion cannot be used because we can still be in a
                 * worker's context in a signal handler.
                 */
                //PRS_ASSERT(worker != prs_worker_current());

                PRS_FTRACE("(%u) interrupt thread", worker->id);
                return prs_pal_thread_interrupt(worker->pal_thread);
            }
        }
        PRS_FTRACE("(%u) not interrupting - %s", worker->id,
            interruptible ? "interrupt already pending" : "not interruptible");
        return interruptible ? PRS_ALREADY_EXISTS : PRS_LOCKED;
    } else {
        PRS_FTRACE("(%u) only set flag", worker->id);
    }

    return PRS_OK;
}

/**
 * \brief
 *  Interrupts a worker.
 * \param worker
 *  Worker to interrupt.
 */
prs_result_t prs_worker_interrupt(struct prs_worker* worker)
{
    return prs_worker_post(worker, PRS_TRUE);
}

/**
 * \brief
 *  Signals a worker.
 *
 *  Signaling a worker only sets the interrupt pending flag in the worker, as opposed to \ref prs_worker_interrupt
 *  which sets the flag in addition to interrupting the flow of code of the worker.
 * \param worker
 *  Worker to signal.
 */
prs_result_t prs_worker_signal(struct prs_worker* worker)
{
    return prs_worker_post(worker, PRS_FALSE);
}

/**
 * \brief
 *  Disables interrupts (sets non-interruptible mode).
 * \param worker
 *  Worker to disable interrupts on.
 * \return
 *  \ref PRS_TRUE if the worker was in interruptible mode.
 *  \ref PRS_FALSE if the worker was in non-interruptible mode.
 */
prs_bool_t prs_worker_int_disable(struct prs_worker* worker)
{
    PRS_PRECONDITION(worker);
    const prs_worker_flags_t flags = prs_pal_atomic_fetch_and(&worker->flags, ~PRS_WORKER_FLAG_INTERRUPTIBLE);
    PRS_ASSERT(!(prs_pal_atomic_load(&worker->flags) & PRS_WORKER_FLAG_INTERRUPTIBLE));
    return PRS_BOOL(flags & PRS_WORKER_FLAG_INTERRUPTIBLE);
}

/**
 * \brief
 *  Enables interrupts (sets interruptible mode).
 * \param worker
 *  Worker to enable interrupts on.
 * \note
 *  This function may invoke the scheduler if the interrupt pending flag is set.
 */
void prs_worker_int_enable(struct prs_worker* worker)
{
    PRS_PRECONDITION(worker);
    PRS_ASSERT(!(prs_pal_atomic_load(&worker->flags) & PRS_WORKER_FLAG_INTERRUPTIBLE));
    prs_worker_task_prologue(worker);
    PRS_ASSERT(prs_pal_atomic_load(&worker->flags) & PRS_WORKER_FLAG_INTERRUPTIBLE);
}

/**
 * \brief
 *  Returns if the worker has interrupts enabled (interruptible mode).
 * \param worker
 *  Worker to check.
 */
prs_bool_t prs_worker_int_enabled(struct prs_worker* worker)
{
    PRS_PRECONDITION(worker);
    return PRS_BOOL(prs_pal_atomic_load(&worker->flags) & PRS_WORKER_FLAG_INTERRUPTIBLE);
}

/**
 * \brief
 *  Invoke the scheduler.
 * \param worker
 *  Worker to invoke the scheduler for. Must be the current worker.
 */
void prs_worker_schedule(struct prs_worker* worker)
{
    prs_worker_schedule_internal(worker, PRS_FALSE, 0);
}

/**
 * \brief
 *  Returns the current worker.
 */
struct prs_worker* prs_worker_current(void)
{
    return prs_wls_get();
}

/**
 * \brief
 *  Returns the current task for the specified worker.
 * \param worker
 *  Worker to get the current task from.
 * \return
 *  Returns the current task for the specified worker. In some cases, there may be no task currently executing and this
 *  function returns \p null.
 */
struct prs_task* prs_worker_get_current_task(struct prs_worker* worker)
{
    return worker->current_task;
}

/**
 * \brief
 *  Returns the current task object ID for the specified worker.
 * \param worker
 *  Worker to get the current task object ID from.
 * \return
 *  Returns the current task object ID for the specified worker. In some cases, there may be no task currently
 *  executing and this function returns \ref PRS_OBJECT_ID_INVALID.
 */
prs_task_id_t prs_worker_get_current_task_id(struct prs_worker* worker)
{
    return prs_pal_atomic_load(&worker->current_task_id);
}

/**
 * \brief
 *  Returns userdata that was set in the \ref prs_worker_create parameters.
 * \param worker
 *  Worker to retrieve the userdata from.
 */
void* prs_worker_get_userdata(struct prs_worker* worker)
{
    return worker->userdata;
}

/**
 * \brief
 *  Copies the initial register context that was saved when the worker started.
 *
 *  This is effectively used to make the worker return from the first context switch and exit its operating system
 *  thread.
 * \param worker
 *  Worker to retrieve the context from.
 * \param context
 *  Context to copy to.
 */
void prs_worker_restore_context(struct prs_worker* worker, struct prs_pal_context* context)
{
    prs_pal_context_copy(context, worker->exit_context);
}

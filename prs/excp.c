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
 *  This file contains the exception module definitions.
 *
 *  Exceptions can be raised through \ref prs_excp_raise and handled with exception handlers registered through
 *  \ref prs_excp_register_handler. Handlers are called in a LIFO order. If no handlers are registered and if no
 *  handlers processed the exception, a default handler will be called.
 *
 *  Depending on the nature of the exception, the faulty task will be terminated or PRS will exit. One exception to
 *  this is stack overflows: the default handler grows the faulty task's stack.
 */

#include <stdlib.h>

#include <prs/alloc/stack.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/excp.h>
#include <prs/init.h>
#include <prs/log.h>
#include <prs/mpscq.h>
#include <prs/proc.h>
#include <prs/task.h>
#include <prs/types.h>
#include <prs/worker.h>

#include "task.h"

struct prs_excp {
    struct prs_mpscq*                   handlers;
};

static struct prs_excp* s_prs_excp = 0;

static const char* prs_excp_type_to_string(enum prs_excp_type excp)
{
    switch (excp) {
        case PRS_EXCP_TYPE_UNKNOWN:
            return "unknown";
        case PRS_EXCP_TYPE_ASSERT:
            return "assert";
        case PRS_EXCP_TYPE_USER:
            return "user-generated";
        case PRS_EXCP_TYPE_PRS:
            return "PRS";
        case PRS_EXCP_TYPE_OS:
            return "operating system";
        case PRS_EXCP_TYPE_STACK_OVERFLOW:
            return "stack overflow";
        case PRS_EXCP_TYPE_SEGMENTATION_FAULT:
            return "segmentation fault";
        case PRS_EXCP_TYPE_ILLEGAL_INSTRUCTION:
            return "illegal instruction";
        case PRS_EXCP_TYPE_INTEGER:
            return "integer";
        case PRS_EXCP_TYPE_FLOATING_POINT:
            return "floating point";
            break;
        case PRS_EXCP_TYPE_BUS:
            return "bus error";
            break;
        case PRS_EXCP_TYPE_USER_INTERRUPT:
            return "user interrupt";
            break;
    }

    return "<unknown>";
}

static const char* prs_excp_task_name(struct prs_task* task)
{
    return task ? task->name : "<no task>";
}

static void prs_excp_print(enum prs_excp_type excp, void* extra, struct prs_worker* worker, struct prs_task* task,
    struct prs_pal_context* context)
{
    if (extra &&
        (excp == PRS_EXCP_TYPE_ASSERT ||
        excp == PRS_EXCP_TYPE_USER ||
        excp == PRS_EXCP_TYPE_PRS ||
        excp == PRS_EXCP_TYPE_OS)) {
        struct prs_excp_raise_info* info = extra;
        prs_log_print("Exception %u (%s) in '%s': expr='%s', %s:%d", excp, prs_excp_type_to_string(excp), prs_excp_task_name(task),
            info->expr, info->file, info->line);
    } else {
        prs_log_print("Exception %u (%s) in '%s': extra=%p", excp, prs_excp_type_to_string(excp), prs_excp_task_name(task), extra);
    }
}

static enum prs_excp_result prs_excp_default_handler(enum prs_excp_type excp, void* extra, struct prs_worker* worker,
    struct prs_task* task, struct prs_pal_context* context)
{
    enum prs_excp_result result = PRS_EXCP_RESULT_CONTINUE;

    prs_excp_print(excp, extra, worker, task, context);

    switch (excp) {
        case PRS_EXCP_TYPE_UNKNOWN:
            result = PRS_EXCP_RESULT_EXIT;
            break;
        case PRS_EXCP_TYPE_ASSERT:
            result = PRS_EXCP_RESULT_EXIT;
            break;
        case PRS_EXCP_TYPE_USER:
            break;
        case PRS_EXCP_TYPE_PRS: {
            struct prs_excp_raise_info* info = extra;
            result = info->behavior;
        }   break;
        case PRS_EXCP_TYPE_OS:
            result = PRS_EXCP_RESULT_EXIT;
            break;
        case PRS_EXCP_TYPE_STACK_OVERFLOW:
            /* Is it the current worker's stack? */
            if (task && prs_stack_address_in_range(task->stack, extra)) {
                const prs_bool_t grown = prs_stack_grow(task->stack, task->stack_size, extra, &task->stack_size);
                if (!grown) {
                    result = PRS_EXCP_RESULT_EXIT;
                }
            } else {
                PRS_FTRACE("unknown stack at %p", extra);
                result = PRS_EXCP_RESULT_EXIT;
            }
            break;
        case PRS_EXCP_TYPE_SEGMENTATION_FAULT:
        case PRS_EXCP_TYPE_ILLEGAL_INSTRUCTION:
        case PRS_EXCP_TYPE_INTEGER:
        case PRS_EXCP_TYPE_FLOATING_POINT:
        case PRS_EXCP_TYPE_BUS:
            /* Verify instruction pointer: if not in range of user code, exit */
            if (task && prs_proc_is_user_text(extra)) {
                result = PRS_EXCP_RESULT_KILL_TASK;
            } else {
                result = PRS_EXCP_RESULT_EXIT;
            }
            break;
        case PRS_EXCP_TYPE_USER_INTERRUPT:
            PRS_FTRACE("User interrupt");
            result = PRS_EXCP_RESULT_EXIT;
            break;
    }

    return result;
}

static prs_bool_t prs_excp_process(prs_excp_handler_t handler, enum prs_excp_type excp, void* extra,
    struct prs_worker* worker, struct prs_task* task, struct prs_pal_context* context)
{
    enum prs_excp_result result = handler(excp, extra, worker, task, context);

    switch (result) {
        case PRS_EXCP_RESULT_EXIT:
            PRS_FTRACE("exiting");
            prs_exit_from_excp(-1);
            return PRS_FALSE;
        case PRS_EXCP_RESULT_KILL_TASK:
            PRS_FTRACE("kill task %s", prs_excp_task_name(task));
            prs_task_destroy(task);
            if (context) {
                prs_worker_restore_context(worker, context);
            } else {
                prs_worker_schedule(worker);
            }
            return PRS_FALSE;
        case PRS_EXCP_RESULT_CONTINUE:
            PRS_FTRACE("continue");
            return PRS_FALSE;
        case PRS_EXCP_RESULT_FORWARD:
            return PRS_TRUE;
    }

    PRS_ASSERT(PRS_FALSE);
    return PRS_FALSE;
}

/**
 * \brief
 *  Raises an exception. The exception is processed by exception handlers.
 * \param excp
 *  Type of the exception.
 * \param extra
 *  Complementary information. Can be \p null.
 * \param worker
 *  Worker on which the exception occurred.
 * \param context
 *  Register context on which the exception occurred. When \p null, the exception occurred on the current executing
 *  context.
 * \note
 *  This should never be called directly by code.
 */
void prs_excp_raise(enum prs_excp_type excp, void* extra, struct prs_worker* worker, struct prs_pal_context* context)
{
    PRS_PRECONDITION(s_prs_excp);

    struct prs_task* task = 0;
    if (worker) {
        task = prs_worker_get_current_task(worker);
    }

    prs_bool_t next = PRS_TRUE;
    if (s_prs_excp) {
        prs_mpscq_rforeach(s_prs_excp->handlers, node) {
            prs_excp_handler_t handler = prs_mpscq_get_data(s_prs_excp->handlers, node);
            next = prs_excp_process(handler, excp, extra, worker, task, context);
            if (!next) {
                break;
            }
        }
    }

    if (next) {
        const prs_bool_t next = prs_excp_process(prs_excp_default_handler, excp, extra, worker, task, context);
        if (next) {
            PRS_FTRACE("no handler found for this exception, exiting");
            prs_exit_from_excp(-1);
        }
    }
}

/**
 * \brief
 *  Registers and exception handler.
 * \param handler
 *  Exception handler to register.
 */
prs_result_t prs_excp_register_handler(prs_excp_handler_t handler)
{
    PRS_PRECONDITION(s_prs_excp);
    PRS_PRECONDITION(handler);

    return prs_mpscq_push(s_prs_excp->handlers, handler);
}

/**
 * \brief
 *  Initializes the exception module.
 */
void prs_excp_init(void)
{
    PRS_PRECONDITION(!s_prs_excp);

    s_prs_excp = prs_pal_malloc_zero(sizeof(*s_prs_excp));
    PRS_FATAL_WHEN(!s_prs_excp);

    s_prs_excp->handlers = prs_mpscq_create();
    PRS_FATAL_WHEN(!s_prs_excp->handlers);
}

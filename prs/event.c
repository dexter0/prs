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
 *  This file contains the definitions for a task blocking event object.
 *
 *  An event object must be created with \ref prs_event_create when a task needs to block its own execution from
 *  continuing until one of many specific events occur. The event object is passed to the event sources which call
 *  \ref prs_event_signal or \ref prs_event_unref when the event respectively occurs or is canceled.
 *
 *  Only the first caller of \ref prs_event_signal actually unblocks the task. Other event sources can still call
 *  \ref prs_event_signal as long as the reference count is above zero. Once the reference count of the object
 *  reaches zero, the object is destroyed. It is possible to directly destroy the event object by calling
 *  \ref prs_event cancel while being careful that no other event sources will signal it after it was canceled.
 *
 *  The \p type parameter of the \ref prs_event_signal function is a small integer that allows the event creator to
 *  retrieve the source of the first event signaling through \ref prs_task_get_last_unblock_cause.
 *
 * \note
 *  Only one event object can be associated to a task at a time.
 */

#include <prs/pal/atomic.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/event.h>
#include <prs/god.h>
#include <prs/task.h>
#include <prs/types.h>

#include "task.h"

struct prs_event {
    prs_task_id_t                       task_id;
    prs_task_token_t                    token;
    prs_uint_t                          initial_refcnt;
    PRS_ATOMIC prs_uint_t               refcnt;
};

/**
 * \brief
 *  Creates a blocking event for the provided \p task.
 * \param task
 *  Task that must be blocked.
 * \param refcnt
 *  Number of event sources that are expected to reference this object and call \ref prs_event_signal or
 *  \ref prs_event_unref.
 */
struct prs_event* prs_event_create(struct prs_task* task, prs_uint_t refcnt)
{
    PRS_PRECONDITION(task);
    
    struct prs_event* event = prs_pal_malloc(sizeof(*event));
    PRS_FATAL_WHEN(!event);
    
    event->task_id = task->id;
    event->token = prs_task_block(task);
    event->initial_refcnt = refcnt;
    prs_pal_atomic_store(&event->refcnt, refcnt);
    
    return event;
}

static void prs_event_destroy(struct prs_event* event)
{
    PRS_PRECONDITION(event);
    prs_pal_free(event);
}

static prs_event_state_t prs_event_unref_internal(struct prs_event* event, prs_bool_t signal, prs_event_type_t type)
{
    PRS_PRECONDITION(event);
    prs_event_state_t state = 0;

    const prs_task_id_t task_id = event->task_id;
    const prs_task_token_t token = event->token;
    const prs_uint_t initial_refcnt = event->initial_refcnt;
    const prs_uint_t refcnt = prs_pal_atomic_fetch_sub(&event->refcnt, 1);
    PRS_ASSERT(refcnt > 0);
    PRS_ASSERT(refcnt <= initial_refcnt);
    if (refcnt == 1) {
        state |= PRS_EVENT_STATE_FREED;
        prs_event_destroy(event);
    }

    if (signal) {
        prs_bool_t success = PRS_FALSE;
        struct prs_task* task = prs_god_lock(task_id);
        if (task) {
            success = prs_task_unblock(task, token, type);
            prs_god_unlock(task_id);
        }
        if (!success) {
            state |= PRS_EVENT_STATE_SIGNALED;
        }
    }
    
    return state;
}

/**
 * \brief
 *  Signals the event to unblock the task that is waiting for it.
 * \param event
 *  Event to signal.
 * \param type
 *  Source of the event.
 * \return
 *  The state of the event object (can be one or many of the following):
 *  \ref PRS_EVENT_STATE_SIGNALED: the event object was already signaled by another source.
 *  \ref PRS_EVENT_STATE_FREED: the event object is freed.
 */
prs_event_state_t prs_event_signal(struct prs_event* event, prs_event_type_t type)
{
    return prs_event_unref_internal(event, PRS_TRUE, type);
}

/**
 * \brief
 *  Decrements the reference count of the event object.
 * \param event
 *  Event to unreference.
 * \return
 *  The state of the event object (can be one or many of the following):
 *  \ref PRS_EVENT_STATE_SIGNALED: the event object was already signaled by another source.
 *  \ref PRS_EVENT_STATE_FREED: the event object is freed.
 */
prs_event_state_t prs_event_unref(struct prs_event* event)
{
    return prs_event_unref_internal(event, PRS_FALSE, 0);
}

/**
 * \brief
 *  Cancels the event object.
 * \param event
 *  Event to cancel.
 */
void prs_event_cancel(struct prs_event* event)
{
    PRS_PRECONDITION(event);
    prs_event_destroy(event);
}

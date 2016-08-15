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
 *  This file contains the message queue definitions.
 *
 *  The message queue is multi-producer and single-consumer. Typically, one queue is created per task and is referenced
 *  only from the \ref prs_task structure.
 *
 *  The implementation supports filters and timeouts. Filters can be used to match any data part of the message. Filter
 *  userdata is copied at run-time into a separate buffer to avoid race conditions where the receive operation is done
 *  but workers are still trying to access the filter.
 *
 *  Receiving a message is typically a blocking operation for the task that calls \ref prs_msgq_recv,
 *  \ref prs_msgq_recv_filter, \ref prs_msgq_recv_timeout or \ref prs_msgq_recv_filter_timeout. However, if a matching
 *  message is found in the queue at the time of the call, blocking may not occur.
 *
 *  Sending a message through \ref prs_msgq_send is always non-blocking, unless the message recipient is run by the
 *  same scheduler and has a higher priority.
 */

#include <stddef.h>
#include <string.h>

#include <prs/pal/atomic.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/clock.h>
#include <prs/error.h>
#include <prs/gpd.h>
#include <prs/log.h>
#include <prs/mpsciq.h>
#include <prs/msg.h>
#include <prs/msgq.h>
#include <prs/pd.h>
#include <prs/result.h>
#include <prs/rtc.h>
#include <prs/task.h>
#include <prs/ticks.h>
#include <prs/timer.h>
#include <prs/worker.h>

#include "task.h"

#define PRS_MSGQ_EVENT_TYPE_SEND        1
#define PRS_MSGQ_EVENT_TYPE_TIMEOUT     2
#define PRS_MSGQ_EVENT_TYPE_FREE        3

struct prs_msgq_filter {
    prs_pd_id_t                         id;
    struct prs_event* PRS_ATOMIC        event;
    void*                               userdata;
    prs_size_t                          userdata_size;
    prs_msgq_filter_function_t          function;
};

struct prs_msgq {
    struct prs_mpsciq*                  queue;
    struct prs_pd*                      pd;
    PRS_ATOMIC prs_pd_id_t              filter_id;
};

static struct prs_msgq_filter* prs_msgq_filter_create(struct prs_msgq* msgq, void* userdata, prs_size_t userdata_size,
    prs_msgq_filter_function_t function)
{
    PRS_PRECONDITION(msgq);

    struct prs_msgq_filter* filter = prs_pal_malloc_zero(sizeof(*filter));
    PRS_FATAL_WHEN(!filter);

    const prs_result_t result = prs_pd_alloc_and_lock(msgq->pd, filter, &filter->id);
    PRS_FATAL_WHEN(result != PRS_OK);

    filter->function = function;
    if (function) {
        filter->userdata_size = userdata_size;
        if (filter->userdata_size > 0) {
            filter->userdata = prs_pal_malloc(userdata_size);
            PRS_FATAL_WHEN(!filter->userdata);
            memcpy(filter->userdata, userdata, userdata_size);
        } else {
            filter->userdata = userdata;
        }
    }

    return filter;
}

static void prs_msgq_filter_unref(struct prs_msgq* msgq, struct prs_msgq_filter* filter)
{
    PRS_PRECONDITION(msgq);
    PRS_PRECONDITION(filter);

    prs_bool_t must_free = prs_pd_unlock(msgq->pd, filter->id);
    if (must_free) {
        if (filter->function) {
            if (filter->userdata_size > 0) {
                prs_pal_free(filter->userdata);
            }
        }
        prs_pal_free(filter);
    }
}

static struct prs_event* prs_msgq_alloc_event(struct prs_msgq_filter* filter, prs_uint_t event_sources)
{
    struct prs_worker* worker = prs_worker_current();
    PRS_ASSERT(worker);
    struct prs_task* task = prs_worker_get_current_task(worker);
    PRS_ASSERT(task);

    /*
     * The event_sources parameter provides us with the number of sources which can trigger the event and unblock the
     * task. The sources can be the following:
     * - prs_msgq_send()
     * - a timeout for the prs_msgq_recv_timeout() and prs_msgq_recv_filter_timeout() functions
     * We add one to this count, so that the prs_msgq_recv functions can try to unblock themselves in the case where
     * a matching message was already found in the queue before the task was rescheduled.
     */
    struct prs_event* event = prs_event_create(task, event_sources + 1);
    PRS_FATAL_WHEN(!event);

    prs_pal_atomic_store(&filter->event, event);

    return event;
}

static prs_bool_t prs_msgq_free_event(struct prs_msgq_filter* filter, struct prs_event* event)
{
    struct prs_event* filter_event = prs_pal_atomic_exchange(&filter->event, 0);
    if (filter_event) {
        PRS_ASSERT(filter_event == event);

        /* The prs_msgq_send() function did not reach the event, so we can manually decrement the reference counter */
        prs_event_unref(event);
    }

    /* This will change the task's state from blocked to ready if the task was never rescheduled */
    const prs_event_state_t event_state = prs_event_signal(event, PRS_MSGQ_EVENT_TYPE_FREE);
    return PRS_BOOL(event_state & PRS_EVENT_STATE_SIGNALED);
}

/**
 * \brief
 *  Creates a message queue.
 * \param params
 *  Message queue parameters.
 */
struct prs_msgq* prs_msgq_create(struct prs_msgq_create_params* params)
{
    PRS_PRECONDITION(params);

    struct prs_msgq* msgq = prs_pal_malloc_zero(sizeof(*msgq));
    if (!msgq) {
        goto cleanup;
    }

    struct prs_mpsciq_create_params mpsciq_params = {
        .node_offset = offsetof(struct prs_msg, node)
    };
    msgq->queue = prs_mpsciq_create(&mpsciq_params);
    if (!msgq->queue) {
        goto cleanup;
    }

    msgq->pd = params->pd;
    if (!msgq->pd) {
        msgq->pd = prs_gpd_get();
        if (!msgq->pd) {
            goto cleanup;
        }
    }

    return msgq;

    cleanup:

    if (msgq) {
        if (msgq->queue) {
            prs_mpsciq_destroy(msgq->queue);
        }
        prs_pal_free(msgq);
    }

    return 0;
}

/**
 * \brief
 *  Destroys a message queue.
 * \param msgq
 *  Message queue to destroy.
 */
void prs_msgq_destroy(struct prs_msgq* msgq)
{
    PRS_PRECONDITION(msgq);

    prs_mpsciq_destroy(msgq->queue);
    prs_pal_free(msgq);
}

/**
 * \brief
 *  Sends a message to the message queue.
 * \param msgq
 *  Message queue to send the message to.
 * \param msg
 *  Message to send to the message queue.
 */
void prs_msgq_send(struct prs_msgq* msgq, struct prs_msg* msg)
{
    PRS_PRECONDITION(msgq);
    PRS_PRECONDITION(msg);

    /*
     * BUG: It is possible that by using the following algorithm, the message will already be consumed by the time we
     * try to lock the filter. This means that we would signal a message that is not in the queue to the receiver.
     *
     * Locking the filter before the push presents the same issue: the filter might be empty or being destroyed, and
     * the pushed message may not reach the next filter as it would be initialized during the push.
     *
     * The workaround for this issue is implemented in prs_msg_recv_internal().
     */
    prs_mpsciq_push(msgq->queue, &msg->node);

    struct prs_msgq_filter* filter = 0;
    const prs_pd_id_t filter_id = prs_pal_atomic_load(&msgq->filter_id);
    if (filter_id != PRS_PD_ID_INVALID) {
        filter = prs_pd_lock(msgq->pd, filter_id);
    }

    if (filter) {
        struct prs_event* event = prs_pal_atomic_load(&filter->event);
        if (event) {
            prs_bool_t match = PRS_FALSE;
            if (filter->function) {
                match = filter->function(filter->userdata, msg);
            } else {
                match = PRS_TRUE;
            }
            if (match) {
                event = prs_pal_atomic_exchange(&filter->event, 0);
                if (event) {
                    prs_event_signal(event, PRS_MSGQ_EVENT_TYPE_SEND);
                }
            }
        }

        prs_msgq_filter_unref(msgq, filter);
    }
}

static void prs_msgq_filter_reset(struct prs_msgq* msgq, struct prs_msgq_filter* filter)
{
    PRS_PRECONDITION(msgq);
    PRS_PRECONDITION(filter);

    const prs_pd_id_t filter_id = prs_pal_atomic_exchange(&msgq->filter_id, PRS_PD_ID_INVALID);
    PRS_ASSERT(filter_id == filter->id);
    prs_msgq_filter_unref(msgq, filter);
}

static void prs_msgq_filter_set(struct prs_msgq* msgq, struct prs_msgq_filter* filter)
{
    PRS_PRECONDITION(msgq);
    PRS_PRECONDITION(filter);

    prs_pal_atomic_store(&msgq->filter_id, filter->id);
}

static struct prs_msg* prs_msgq_search(struct prs_msgq* msgq, void* userdata, prs_msgq_filter_function_t function,
    struct prs_mpsciq_node** hint)
{
    PRS_PRECONDITION(msgq);

    /* prs_mpsciq_begin will build the 'prev' links in mpsciq that are required for calling prs_mpsciq_next */
    struct prs_mpsciq_node* node = prs_mpsciq_begin(msgq->queue);
    if (hint && *hint) {
        node = *hint;
    }

    struct prs_msg* result = 0;
    struct prs_mpsciq_node* prev = node;
    while (node) {
        struct prs_msg* msg = prs_mpsciq_get_data(msgq->queue, node);
        if (function) {
            const prs_bool_t match = function(userdata, msg);
            if (match) {
                result = msg;
                break;
            }
        } else {
            result = msg;
            break;
        }
        prev = node;
        node = prs_mpsciq_next(msgq->queue, node);
    }

    if (!result && hint) {
        *hint = prev;
    }

    return result;
}

static struct prs_msg* prs_msgq_recv_internal(struct prs_msgq* msgq, void* userdata, prs_size_t userdata_size,
    prs_msgq_filter_function_t function, prs_ticks_t timeout, prs_bool_t use_timeout)
{
    PRS_PRECONDITION(msgq);

    /* Search for the message in the queue now, before trying more expensive operations */
    struct prs_mpsciq_node* hint = 0;
    struct prs_msg* msg = prs_msgq_search(msgq, userdata, function, &hint);

    /*
     * BUG: Here, we work around a bug (which is also explained in prs_msgq_send()) by repeating the recv operation
     * until a timeout occurs or a message is actually caught.
     */

    prs_ticks_t wait_left = timeout;
    while (!msg) {
        const prs_bool_t timeout_active = PRS_BOOL(use_timeout && wait_left > 0);
        if (use_timeout && !timeout_active) {
            break;
        }
        struct prs_msgq_filter* filter = prs_msgq_filter_create(msgq, userdata, userdata_size, function);
        PRS_ASSERT(filter);
        struct prs_event* event = prs_msgq_alloc_event(filter, timeout_active ? 2 : 1);
        PRS_ASSERT(event);
        prs_msgq_filter_set(msgq, filter);

        msg = prs_msgq_search(msgq, userdata, function, &hint);
        prs_bool_t msg_expected = PRS_TRUE;
        if (msg) {
            const prs_bool_t already_signaled = prs_msgq_free_event(filter, event);
            if (already_signaled) {
                prs_sched_schedule();
            }
        } else {
            struct prs_timer_entry* timer_entry = 0;
            if (timeout_active) {
                timer_entry = prs_timer_queue(prs_clock_timer(), event, PRS_MSGQ_EVENT_TYPE_TIMEOUT, wait_left);
                PRS_ASSERT(timer_entry);
            }
            prs_sched_schedule();
            if (timeout_active) {
                const prs_ticks_t diff = prs_clock_get() - prs_timer_get_start(timer_entry);
                if (diff >= wait_left) {
                    wait_left = 0;
                } else {
                    wait_left -= diff;
                }
                prs_timer_cancel(prs_clock_timer(), timer_entry);
            }

            struct prs_task* task = prs_task_current();
            PRS_ASSERT(task);
            const prs_event_type_t event_type = (prs_event_type_t)prs_task_get_last_unblock_cause(task);
            if (event_type == PRS_MSGQ_EVENT_TYPE_SEND) {
                msg = prs_msgq_search(msgq, userdata, function, &hint);
            } else {
                msg_expected = PRS_FALSE;
                PRS_ASSERT(event_type == PRS_MSGQ_EVENT_TYPE_TIMEOUT);
            }

            prs_msgq_free_event(filter, event);
        }

        prs_msgq_filter_reset(msgq, filter);

        if (!msg && !msg_expected) {
            break;
        }
    }

    if (msg) {
        prs_mpsciq_remove(msgq->queue, &msg->node);
    }

    PRS_POSTCONDITION(msg || use_timeout);

    return msg;
}

/**
 * \brief
 *  Receive a message from the message queue.
 * \param msgq
 *  Message queue to receive the message from.
 * \return
 *  The received message. Cannot be \p null.
 */
struct prs_msg* prs_msgq_recv(struct prs_msgq* msgq)
{
    return prs_msgq_recv_internal(msgq, 0, 0, 0, 0, PRS_FALSE);
}

/**
 * \brief
 *  Receive a message from the message queue with a timeout.
 * \param msgq
 *  Message queue to receive the message from.
 * \param timeout
 *  Timeout in ticks.
 * \return
 *  The received message, or \p null if a timeout occurred.
 */
struct prs_msg* prs_msgq_recv_timeout(struct prs_msgq* msgq, prs_ticks_t timeout)
{
    return prs_msgq_recv_internal(msgq, 0, 0, 0, timeout, PRS_TRUE);
}

/**
 * \brief
 *  Receive a message from the message queue with a filter.
 * \param msgq
 *  Message queue to receive the message from.
 * \param userdata
 *  Pointer to data that will be passed to the filter function.
 * \param userdata_size
 *  Size of the data to be passed to the filter function.
 * \param function
 *  Filter function.
 * \return
 *  The received message. Cannot be \p null.
 */
struct prs_msg* prs_msgq_recv_filter(struct prs_msgq* msgq, void* userdata, prs_size_t userdata_size,
    prs_msgq_filter_function_t function)
{
    return prs_msgq_recv_internal(msgq, userdata, userdata_size, function, 0, PRS_FALSE);
}

/**
 * \brief
 *  Receive a message from the message queue with a filter and with a timeout.
 * \param msgq
 *  Message queue to receive the message from.
 * \param userdata
 *  Pointer to data that will be passed to the filter function.
 * \param userdata_size
 *  Size of the data to be passed to the filter function.
 * \param function
 *  Filter function.
 * \param timeout
 *  Timeout in ticks.
 * \return
 *  The received message, or \p null if a timeout occurred.
 */
struct prs_msg* prs_msgq_recv_filter_timeout(struct prs_msgq* msgq, void* userdata, prs_size_t userdata_size,
    prs_msgq_filter_function_t function, prs_ticks_t timeout)
{
    return prs_msgq_recv_internal(msgq, userdata, userdata_size, function, timeout, PRS_TRUE);
}

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
 *  This file contains the timer module definitions.
 *
 *  The timer module handles timeouts requested by tasks. At every system tick, it uses a wheel algorithm to signal
 *  events to unblock tasks for which the timeouts expired.
 *
 *  Tasks request to be unblocked after a delay using the \ref prs_timer_queue function. They must end the request
 *  using the \ref prs_timer_cancel function after the timeout has expired or another unblocking event has occurred.
 *
 *  The clock module calls \ref prs_timer_tick at every system tick.
 */

#include <stddef.h>

#include <prs/pal/bitops.h>
#include <prs/pal/cycles.h>
#include <prs/pal/inline.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/event.h>
#include <prs/error.h>
#include <prs/god.h>
#include <prs/idllist.h>
#include <prs/log.h>
#include <prs/task.h>
#include <prs/clock.h>
#include <prs/mpsciq.h>
#include <prs/pool.h>
#include <prs/timer.h>

#include "task.h"

#define PRS_MAX_TIMER_WHEELS            (sizeof(prs_ticks_t))
#define PRS_MAX_TIMER_BITS_PER_WHEEL    ((sizeof(prs_ticks_t) * 8) / PRS_MAX_TIMER_WHEELS)
#define PRS_MAX_TIMER_SLOTS_PER_WHEEL   (1 << PRS_MAX_TIMER_BITS_PER_WHEEL)
#define PRS_TIMER_TICKS_MASK            (PRS_MAX_TIMER_WHEELS * PRS_MAX_TIMER_BITS_PER_WHEEL)

struct prs_timer {
    struct prs_idllist*                 lists[PRS_MAX_TIMER_WHEELS][PRS_MAX_TIMER_SLOTS_PER_WHEEL];
    struct prs_idllist*                 tmp_list;

    struct prs_pool*                    pool;

    struct prs_mpsciq*                  queued;

    prs_ticks_t                         now;
};

struct prs_timer_entry {
    /* The following two nodes cannot be in a union because prs_mpsciq_node uses atomic fields */
    struct prs_idllist_node             idllist;
    struct prs_mpsciq_node              mpsciq;
    prs_ticks_t                         start;
    prs_ticks_t                         end;
    struct prs_event*                   event;
    prs_event_type_t                    event_type;
};

/**
 * \brief
 *  Creates a timer.
 */
struct prs_timer* prs_timer_create(void)
{
    struct prs_timer* timer = prs_pal_malloc(sizeof(*timer));
    if (!timer) {
        return 0;
    }

    struct prs_idllist_create_params idllist_params = {
        .node_offset = offsetof(struct prs_timer_entry, idllist)
    };
    for (int wheel = 0; wheel < PRS_MAX_TIMER_WHEELS; ++wheel) {
        for (int slot = 0; slot < PRS_MAX_TIMER_SLOTS_PER_WHEEL; ++slot) {
            struct prs_idllist* idllist = prs_idllist_create(&idllist_params);
            if (!idllist) {
                goto cleanup;
            }
            timer->lists[wheel][slot] = idllist;
        }
    }

    timer->tmp_list = prs_idllist_create(&idllist_params);
    if (!timer->tmp_list) {
        goto cleanup;
    }

    struct prs_pool_create_params pool_params = {
        /*
         * There won't be more than PRS_MAX_OBJECT tasks, but a single task could start a few timers before the tick is
         * called.
         */
        .max_entries = PRS_MAX_OBJECTS * 16,
        .data_size = sizeof(struct prs_timer_entry),
        .area = 0
    };
    timer->pool = prs_pool_create(&pool_params);
    if (!timer->pool) {
        goto cleanup;
    }

    struct prs_mpsciq_create_params mpsciq_params = {
        .node_offset = offsetof(struct prs_timer_entry, mpsciq)
    };
    timer->queued = prs_mpsciq_create(&mpsciq_params);
    if (!timer->queued) {
        goto cleanup;
    }

    timer->now = prs_clock_get();

    return timer;

    cleanup:

    if (timer) {
        if (timer->queued) {
            prs_mpsciq_destroy(timer->queued);
        }
        if (timer->pool) {
            prs_pool_destroy(timer->pool);
        }
        if (timer->tmp_list) {
            prs_idllist_destroy(timer->tmp_list);
        }
        for (int wheel = 0; wheel < PRS_MAX_TIMER_WHEELS; ++wheel) {
            for (int slot = 0; slot < PRS_MAX_TIMER_SLOTS_PER_WHEEL; ++slot) {
                struct prs_idllist* idllist = timer->lists[wheel][slot];
                if (idllist) {
                    prs_idllist_destroy(idllist);
                }
            }
        }
        prs_pal_free(timer);
    }

    return 0;
}

/**
 * \brief
 *  Destroys a timer.
 */
void prs_timer_destroy(struct prs_timer* timer)
{
    prs_mpsciq_destroy(timer->queued);
    prs_idllist_destroy(timer->tmp_list);
    for (int wheel = 0; wheel < PRS_MAX_TIMER_WHEELS; ++wheel) {
        for (int slot = 0; slot < PRS_MAX_TIMER_SLOTS_PER_WHEEL; ++slot) {
            /* Note: elements in each list are part of the pool, so there is nothing to free there */
            struct prs_idllist* idllist = timer->lists[wheel][slot];
            prs_idllist_destroy(idllist);
        }
    }
    prs_pool_destroy(timer->pool);
    prs_pal_free(timer);
}

/**
 * \brief
 *  Queues a timeout request.
 * \param timer
 *  Timer for which the timeout request must be queued.
 * \param event
 *  Event that will be signaled through \ref prs_event_signal when the timeout occurs. If the request is canceled,
 *  the event will be unreferenced through \ref prs_event_unref.
 * \param event_type
 *  Event type that will be passed to \ref prs_event_signal when the timeout occurs.
 * \param timeout
 *  Timeout in ticks.
 * \return
 *  The timeout entry that must be passed as a parameter to \ref prs_timer_cancel after the task is unblocked.
 */
struct prs_timer_entry* prs_timer_queue(struct prs_timer* timer, struct prs_event* event, prs_event_type_t event_type,
    prs_ticks_t timeout)
{
    prs_pool_id_t entry_id = PRS_POOL_ID_INVALID;
    struct prs_timer_entry* entry = prs_pool_alloc(timer->pool, &entry_id);
    PRS_FATAL_WHEN(!entry || entry_id == PRS_POOL_ID_INVALID);

    entry->event = event;
    entry->event_type = event_type;
    entry->start = prs_clock_get();
    entry->end = entry->start + timeout;

    prs_pool_lock_first(timer->pool, entry_id);

    /*
     * Here, we lock the entry a second time. One reference belongs to the caller while one reference will be handled
     * by the timer tick.
     */
    struct prs_timer_entry* locked_entry = prs_pool_lock(timer->pool, entry_id);
    PRS_ASSERT(locked_entry == entry);

    prs_mpsciq_push(timer->queued, &entry->mpsciq);

    return entry;
}

/**
 * \brief
 *  Returns the system tick value that was recorded when \ref prs_timer_queue created the timer entry.
 * \param
 *  Timer entry returned by \ref prs_timer_queue.
 */
prs_ticks_t prs_timer_get_start(struct prs_timer_entry* entry)
{
    return entry->start;
}

/**
 * \brief
 *  Cancels the timer request. This must be called after every \ref prs_timer_queue call.
 * \param timer
 *  Timer module for which to cancel the timer entry.
 * \param entry
 *  Timer entry returned by \ref prs_timer_queue.
 */
void prs_timer_cancel(struct prs_timer* timer, struct prs_timer_entry* entry)
{
    prs_pool_unlock(timer->pool, prs_pool_get_id(timer->pool, entry));
}

static PRS_INLINE int prs_timer_hsb(prs_ticks_t ticks)
{
    if (sizeof(ticks) == 4) {
        return prs_bitops_hsb_uint32(ticks);
    } else if (sizeof(ticks) == 8) {
        return prs_bitops_hsb_uint64(ticks);
    } else {
        PRS_ASSERT(PRS_FALSE);
        return 0;
    }
}

static void prs_timer_event_destructor(void* userdata, void* data)
{
    struct prs_timer* timer = userdata;
    PRS_ASSERT(timer);

    struct prs_timer_entry* entry = data;
    PRS_ASSERT(entry);

    prs_event_unref(entry->event);
}

static void prs_timer_queue_internal(struct prs_timer* timer, struct prs_timer_entry* entry)
{
    const prs_pool_id_t entry_id = prs_pool_get_id(timer->pool, entry);
    const prs_int_t delay = entry->end - timer->now;
    if (delay <= 0) {
        /*
         * Here, the delay should be zero, but sometimes it can be a negative number because the clock tick can be read
         * just before it is incremented, and then sent as the delay in the timer queuing request.
         * PRS_ASSERT(delay == 0);
         */
        prs_event_signal(entry->event, entry->event_type);
        prs_pool_unlock(timer->pool, entry_id);
        return;
    }

    /* If we can free the entry, it means it was canceled. */
    const prs_bool_t canceled = prs_pool_try_unlock_final_dest(timer->pool, entry_id, prs_timer_event_destructor, timer);
    if (canceled) {
        return;
    }

    const prs_int_t hsb = prs_timer_hsb((prs_ticks_t)delay); /* Cast to remove signed and avoid GCC warning */
    PRS_ASSERT(hsb >= 0);
    const prs_int_t wheel = hsb / PRS_MAX_TIMER_BITS_PER_WHEEL;
    PRS_ASSERT(wheel < PRS_MAX_TIMER_WHEELS);
    const prs_int_t wheel_shift = wheel * PRS_MAX_TIMER_BITS_PER_WHEEL;
    const prs_uint_t slot = ((entry->end >> wheel_shift) - !!wheel) & (PRS_MAX_TIMER_SLOTS_PER_WHEEL - 1);
    PRS_ASSERT(slot < PRS_MAX_TIMER_SLOTS_PER_WHEEL);

    struct prs_idllist* list = timer->lists[wheel][slot];
    prs_idllist_insert_before(list, 0, &entry->idllist);
}

/**
 * \brief
 *  Process elapsed time and signal events if timeouts occurred.
 * \param timer
 *  Timer module.
 */
void prs_timer_tick(struct prs_timer* timer)
{
    const prs_ticks_t now = prs_clock_get();
    const prs_ticks_t elapsed = now - timer->now;
    const prs_ticks_t changed = now ^ timer->now;
    const int max_wheel = prs_timer_hsb(changed) / PRS_MAX_TIMER_BITS_PER_WHEEL + 1;
    for (int wheel = 0; wheel < max_wheel; ++wheel) {
        const int slot_mask = PRS_MAX_TIMER_SLOTS_PER_WHEEL - 1;
        const int first_slot = (timer->now >> (wheel * PRS_MAX_TIMER_BITS_PER_WHEEL)) & slot_mask;
        int elapsed_slots = (elapsed >> (wheel * PRS_MAX_TIMER_SLOTS_PER_WHEEL));
        if (elapsed_slots > PRS_MAX_TIMER_SLOTS_PER_WHEEL) {
            elapsed_slots = PRS_MAX_TIMER_SLOTS_PER_WHEEL;
        }
        for (int i = 0; i < elapsed_slots; ++i) {
            const int slot = first_slot + i;
            PRS_ASSERT(wheel < PRS_MAX_TIMER_WHEELS);
            PRS_ASSERT(slot < PRS_MAX_TIMER_SLOTS_PER_WHEEL);
            struct prs_idllist* list = timer->lists[wheel][slot];
            struct prs_idllist_node* idllist_node;
            while ((idllist_node = prs_idllist_begin(list)) != 0) {
                prs_idllist_remove(list, idllist_node);
                prs_idllist_insert_before(timer->tmp_list, 0, idllist_node);
            }
            PRS_ASSERT(prs_idllist_empty(list));
        }
    }
    struct prs_idllist_node* tmp_entry;
    while ((tmp_entry = prs_idllist_begin(timer->tmp_list)) != 0) {
        struct prs_timer_entry* entry = prs_idllist_get_data(timer->tmp_list, tmp_entry);
        prs_idllist_remove(timer->tmp_list, tmp_entry);
        prs_timer_queue_internal(timer, entry);
    }
    PRS_ASSERT(prs_idllist_empty(timer->tmp_list));
    timer->now = now;

    struct prs_mpsciq_node* node;
    while ((node = prs_mpsciq_begin(timer->queued)) != 0) {
        struct prs_timer_entry* entry = prs_mpsciq_get_data(timer->queued, node);
        prs_mpsciq_remove(timer->queued, node);
        prs_timer_queue_internal(timer, entry);
    }
}

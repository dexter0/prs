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
 *  This file contains the clock module definitions.
 *  The clock module is responsible for providing the clock tick through \ref prs_clock_get and calling the timer
 *  module.
 */

#include <prs/pal/atomic.h>
#include <prs/pal/malloc.h>
#include <prs/pal/pit.h>
#include <prs/assert.h>
#include <prs/clock.h>
#include <prs/error.h>
#include <prs/timer.h>
#include <prs/result.h>
#include <prs/rtc.h>
#include <prs/spinlock.h>

struct prs_clock {
    struct prs_pal_pit*                 pit;
    struct prs_timer*                   timer;

    void*                               userdata;
    void                                (*callback)(void* userdata);

    struct prs_spinlock*                spinlock;
};

static struct prs_clock* s_prs_clock = 0;
static PRS_ATOMIC prs_ticks_t s_prs_ticks;

static void prs_clock_entry(void* userdata)
{
    struct prs_clock* clock = userdata;
    PRS_ASSERT(clock);

    const prs_bool_t locked = prs_spinlock_try_lock(clock->spinlock);
    if (!locked) {
        PRS_FATAL("Double clock entry");
    }

    prs_pal_atomic_fetch_add(&s_prs_ticks, 1);
    prs_timer_tick(clock->timer);

    prs_spinlock_unlock(clock->spinlock);
}

/**
 * \brief
 *  Initializes the clock module.
 * \param params
 *  Parameters.
 * \see
 *  prs_clock_init_params
 */
prs_result_t prs_clock_init(struct prs_clock_init_params* params)
{
    PRS_PRECONDITION(!s_prs_clock);

    prs_result_t result = PRS_OK;

    struct prs_clock* clock = prs_pal_malloc_zero(sizeof(*s_prs_clock));
    PRS_ERROR_IF (!clock) {
        result = PRS_OUT_OF_MEMORY;
        goto cleanup;
    }

    prs_pal_atomic_store(&s_prs_ticks, 0);

    clock->timer = prs_timer_create();
    PRS_ERROR_IF (!clock->timer) {
        result = PRS_UNKNOWN;
        goto cleanup;
    }

    clock->spinlock = prs_spinlock_create();
    PRS_ERROR_IF (!clock->spinlock) {
        result = PRS_UNKNOWN;
        goto cleanup;
    }

    struct prs_pal_pit_create_params pit_params = {
        .period = PRS_TICKS_FROM_US(1000000 * 1 / PRS_HZ),
        .use_current_thread = params->use_current_thread,
        .affinity = params->affinity,
        .prio = params->prio,
        .userdata = clock,
        .callback = prs_clock_entry
    };
    clock->pit = prs_pal_pit_create(&pit_params);
    PRS_ERROR_IF (!clock->pit) {
        result = PRS_UNKNOWN;
        goto cleanup;
    }

    s_prs_clock = clock;

    return PRS_OK;

    cleanup:

    if (clock) {
        prs_pal_free(clock);
    }

    return result;
}

/**
 * \brief
 *  Uninitializes the clock module.
 */
void prs_clock_uninit(void)
{
    PRS_PRECONDITION(s_prs_clock);
    struct prs_clock* clock = s_prs_clock;
    prs_pal_pit_destroy(clock->pit);
    prs_spinlock_destroy(clock->spinlock);
    /* Do not destroy the timer here, as it can be used by remaining tasks that are still running */
    //prs_timer_destroy(clock->timer);
    //prs_pal_free(clock);
}

/**
 * \brief
 *  Returns the global \ref prs_timer instance.
 */
struct prs_timer* prs_clock_timer(void)
{
    PRS_PRECONDITION(s_prs_clock);
    struct prs_clock* clock = s_prs_clock;
    return clock->timer;
}

/**
 * \brief
 *  Returns the number of ticks elapsed since \ref prs_clock_init was called.
 * \note
 *  This function is safe to be called in an interruptible section.
 */
prs_ticks_t prs_clock_get(void)
{
    return prs_pal_atomic_load(&s_prs_ticks);
}

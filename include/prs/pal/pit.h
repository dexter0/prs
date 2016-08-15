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
 *  This file contains the PAL programmable interrupt timer declarations.
 *
 *  The programmable interrupt timer (PIT) is an operating system facility that is used to generate periodic function
 *  calls at precise intervals.
 */

#ifndef _PRS_PAL_PIT_H
#define _PRS_PAL_PIT_H

#include <prs/pal/thread.h>
#include <prs/ticks.h>

struct prs_pal_pit;

/**
 * \brief
 *  This structure provides the PAL PIT's parameters that should be passed to \ref prs_pal_pit_create.
 */
struct prs_pal_pit_create_params {
    /** \brief Period, in ticks, at which the call to \p callback should be made. */
    prs_ticks_t                         period;

    /**
     * \brief
     *  If the clock module should use the current thread to use interrupts. This has a varying effect depending on the
     *  underlying operating system.
     */
    prs_bool_t                          use_current_thread;
    /** \brief Affinity of the clock module. This is only used if \ref use_current_thread is set to \p true. */
    prs_core_mask_t                     affinity;
    /** \brief Priority of the clock module. This is only used if \ref use_current_thread is set to \p true. */
    enum prs_pal_thread_prio            prio;

    /** \brief Data that should be passed to \p callback when the period expires. */
    void*                               userdata;
    /** \brief Callback to use when the period expires. */
    void                                (*callback)(void* userdata);
};

/**
 * \brief
 *  Creates and starts a programmable interrupt timer.
 */
struct prs_pal_pit* prs_pal_pit_create(struct prs_pal_pit_create_params* params);

/**
 * \brief
 *  Stops and destroys a programmable interrupt timer.
 */
void prs_pal_pit_destroy(struct prs_pal_pit* pal_pit);

#endif /* _PRS_PAL_PIT_H */

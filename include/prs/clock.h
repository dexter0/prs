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
 *  This file contains the clock module declarations.
 */

#ifndef _PRS_CLOCK_H
#define _PRS_CLOCK_H

#include <prs/pal/thread.h>
#include <prs/result.h>
#include <prs/ticks.h>

/**
 * \brief
 *  This structure provides the clock module's parameters that should be passed to \ref prs_clock_init.
 */
struct prs_clock_init_params {
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
};

prs_result_t prs_clock_init(struct prs_clock_init_params* params);
void prs_clock_uninit(void);

struct prs_timer* prs_clock_timer(void);
prs_ticks_t prs_clock_get(void);

#endif /* _PRS_CLOCK_H */

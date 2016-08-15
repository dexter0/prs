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
 *  This file contains the timer module declarations.
 */

#ifndef _PRS_TIMER_H
#define _PRS_TIMER_H

#include <prs/event.h>
#include <prs/task.h>
#include <prs/ticks.h>

struct prs_timer;
struct prs_timer_entry;

struct prs_timer* prs_timer_create(void);
void prs_timer_destroy(struct prs_timer* timer);

struct prs_timer_entry* prs_timer_queue(struct prs_timer* timer, struct prs_event* event, prs_event_type_t event_type,
    prs_ticks_t timeout);
prs_ticks_t prs_timer_get_start(struct prs_timer_entry* entry);
void prs_timer_cancel(struct prs_timer* timer, struct prs_timer_entry* entry);

void prs_timer_tick(struct prs_timer* timer);

#endif /* _PRS_TIMER_H */

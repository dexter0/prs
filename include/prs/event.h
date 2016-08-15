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
 *  This file contains the declarations for a task blocking event object.
 */

#ifndef _PRS_EVENT_H
#define _PRS_EVENT_H

#include <prs/task.h>
#include <prs/types.h>

struct prs_event;

/** \brief Type containing the source of an event. */
typedef prs_uint8_t prs_event_type_t;

/** \brief Indicates that the event object was already signaled by another source. */
#define PRS_EVENT_STATE_SIGNALED        0x1
/** \brief Indicates that the event object was freed by the returning call. */
#define PRS_EVENT_STATE_FREED           0x2

/** \brief Type containing the state of an event object. */
typedef prs_uint_t prs_event_state_t;

struct prs_event* prs_event_create(struct prs_task* task, prs_uint_t refcnt);

prs_event_state_t prs_event_signal(struct prs_event* event, prs_event_type_t type);
prs_event_state_t prs_event_unref(struct prs_event* event);
void prs_event_cancel(struct prs_event* event);

#endif /* _PRS_EVENT_H */

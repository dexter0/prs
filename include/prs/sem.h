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
 *  This file contains semaphore declarations.
 */

#ifndef _PRS_SEM_H
#define _PRS_SEM_H

#include <prs/object.h>
#include <prs/result.h>
#include <prs/ticks.h>
#include <prs/types.h>

struct prs_sem;

/**
 * \brief
 *  Semaphore creation parameters.
 */
struct prs_sem_create_params {
    /** \brief Maximum count of the semaphore. */
    prs_int_t                           max_count;
    /** \brief Initial count of the semaphore. */
    prs_int_t                           initial_count;
};

struct prs_sem* prs_sem_create(struct prs_sem_create_params* params);
void prs_sem_destroy(struct prs_sem* sem);

void prs_sem_wait(struct prs_sem* sem);
prs_result_t prs_sem_wait_timeout(struct prs_sem* sem, prs_ticks_t timeout);
void prs_sem_signal(struct prs_sem* sem);


#endif /* _PRS_SEM_H */

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
 *  This file contains spinlock definitions.
 */

#ifndef _PRS_SPINLOCK_H
#define _PRS_SPINLOCK_H

#include <prs/types.h>

struct prs_spinlock;

struct prs_spinlock* prs_spinlock_create(void);
void prs_spinlock_destroy(struct prs_spinlock* spinlock);

void prs_spinlock_lock(struct prs_spinlock* spinlock);
prs_bool_t prs_spinlock_try_lock(struct prs_spinlock* spinlock);

void prs_spinlock_unlock(struct prs_spinlock* spinlock);

#endif /* _PRS_SPINLOCK_H */
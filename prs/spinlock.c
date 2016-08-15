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
 *
 *  The spinlock is implemented using a simple exchange operation. The spinlock waits indefinitely for the resource to
 *  be released. There is no queue for the workers that are waiting for the lock. This lock is meant to be used in
 *  scenarios where mutually exclusion is critical but cannot be scheduled later.
 */

#include <prs/pal/atomic.h>
#include <prs/pal/malloc.h>
#include <prs/error.h>
#include <prs/spinlock.h>

struct prs_spinlock {
    PRS_ATOMIC prs_bool_t               lock;
};

/**
 * \brief
 *  Creates a spinlock.
 */
struct prs_spinlock* prs_spinlock_create(void)
{
    struct prs_spinlock* spinlock = prs_pal_malloc(sizeof(*spinlock));
    PRS_FATAL_WHEN(!spinlock);
    
    prs_pal_atomic_store(&spinlock->lock, PRS_FALSE);
    
    return spinlock;
}

/**
 * \brief
 *  Destroys a spinlock.
 * \param spinlock
 *  Spinlock to destroy.
 */
void prs_spinlock_destroy(struct prs_spinlock* spinlock)
{
    prs_pal_free(spinlock);
}

/**
 * \brief
 *  Acquire the lock. If it is not available, wait for it.
 * \param spinlock
 *  Spinlock to wait for.
 */
void prs_spinlock_lock(struct prs_spinlock* spinlock)
{
    while (!prs_spinlock_try_lock(spinlock))
    {
    }
}

/**
 * \brief
 *  Tries to acquire the lock.
 * \param spinlock
 *  Spinlock to lock.
 * \return
 *  \ref PRS_TRUE if the spinlock was effectively locked.
 *  \ref PRS_FALSE if the spinlock was not locked.
 */
prs_bool_t prs_spinlock_try_lock(struct prs_spinlock* spinlock)
{
    return PRS_BOOL(!prs_pal_atomic_exchange(&spinlock->lock, PRS_TRUE));
}

/**
 * \brief
 *  Releases a spinlock.
 * \param spinlock
 *  Spinlock to release.
 */
void prs_spinlock_unlock(struct prs_spinlock* spinlock)
{
    prs_pal_atomic_store(&spinlock->lock, 0);
}

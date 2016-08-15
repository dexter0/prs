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
 *  This file contains the PAL worker local storage definitions.
 *
 *  Worker local storage (WLS) is the same as thread local storage, except that is optimized for faster worker pointer
 *  access.
 */

#ifndef _PRS_PAL_WLS_H
#define _PRS_PAL_WLS_H

#include <prs/types.h>
#include <prs/result.h>

struct prs_worker;

/**
 * \brief
 *  Initializes the worker local storage module.
 */
prs_result_t prs_wls_init(void);

/**
 * \brief
 *  Uninitializes the worker local storage module.
 */
void prs_wls_uninit(void);

/**
 * \brief
 *  Initializes the worker local storage module for the specified worker.
 */
prs_result_t prs_wls_worker_init(struct prs_worker* worker);

/**
 * \brief
 *  Uninitializes the worker local storage module for the specified worker.
 */
void prs_wls_worker_uninit(struct prs_worker* worker);

/**
 * \brief
 *  Sets the worker local storage value for the current thread.
 */
void prs_wls_set(void* data);

/**
 * \brief
 *  Gets the worker local storage value for the current thread.
 */
void* prs_wls_get(void);

#endif /* _PRS_PAL_WLS_H */

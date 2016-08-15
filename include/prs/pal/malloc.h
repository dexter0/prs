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
 *  This file contains the PAL memory allocation declarations.
 */

#ifndef _PRS_PAL_MALLOC_H
#define _PRS_PAL_MALLOC_H

#include <prs/result.h>
#include <prs/types.h>

/**
 * \brief
 *  This function is called by PRS to allocate memory from the operating system's heap.
 * \param size
 *  Number of bytes to allocate.
 * \return
 *  A pointer to the allocated memory area.
 */
void* prs_pal_malloc(prs_size_t size);

/**
 * \brief
 *  This function is called by PRS to allocate zeroed memory from the operating system's heap.
 * \param size
 *  Number of bytes to allocate.
 * \return
 *  A pointer to the allocated memory area.
 */
void* prs_pal_malloc_zero(prs_size_t size);

/**
 * \brief
 *  This function is called by PRS to free memory to the operating system's heap.
 * \param data
 *  Memory area to free. It has to have been allocated by \ref prs_pal_malloc or \ref prs_pal_malloc_zero firsthand.
 */
void prs_pal_free(void* data);

#endif /* _PRS_PAL_MALLOC_H */

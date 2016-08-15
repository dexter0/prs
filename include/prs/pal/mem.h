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
 *  This file contains the PAL virtual memory declarations.
 *
 *  Virtual memory should be allocated with \ref prs_pal_mem_map and freed using \ref prs_pal_mem_unmap. The number of
 *  bytes should be a multiple of the operating system's page size (or huge page size, if specified).
 *
 *  Once memory is allocated (reserved), it must be committed in order to be usable. Otherwise, accesses to the
 *  reserved memory will trigger a segmentation fault. Use \ref prs_pal_mem_commit to commit memory. It is also
 *  possible to immediately commit reserved memory by using the \ref PRS_PAL_MEM_FLAG_COMMIT flag when calling
 *  \ref prs_pal_mem_map.
 *
 *  Other flags specify the read, write and execute rights to the virtual memory area. Guard pages are also allowed on
 *  the Windows platform.
 *
 *  Virtual memory can also be locked into physical memory to avoid page faults.
 */

#ifndef _PRS_PAL_MEM_H
#define _PRS_PAL_MEM_H

#include <prs/result.h>
#include <prs/types.h>

/**
 * \brief
 *  Virtual memory flags type.
 */
typedef prs_uint32_t prs_pal_mem_flags_t;

/** \brief No flags. */
#define PRS_PAL_MEM_FLAG_NONE           0x00000000
/** \brief Read access. */
#define PRS_PAL_MEM_FLAG_READ           0x00000001
/** \brief Write access. */
#define PRS_PAL_MEM_FLAG_WRITE          0x00000002
/** \brief Execute access. */
#define PRS_PAL_MEM_FLAG_EXECUTE        0x00000004
/** \brief Guard page. Do not use this definition directly, use \ref PRS_PAL_MEM_FLAG_GUARD instead. */
#define PRS_PAL_MEM_FLAG_GUARD_ONLY     0x00000008
/** \brief Guard page (Windows only). An access to a guard page triggers an exception only the first time it is accessed. */
#define PRS_PAL_MEM_FLAG_GUARD          (PRS_PAL_MEM_FLAG_GUARD_ONLY | PRS_PAL_MEM_FLAG_READ | PRS_PAL_MEM_FLAG_WRITE)
/** \brief Commit the specified memory area. */
#define PRS_PAL_MEM_FLAG_COMMIT         0x80000000
/** \brief Lock the specified memory area. */
#define PRS_PAL_MEM_FLAG_LOCK           0x40000000
/** \brief Use huge pages for the specified memory area. */
#define PRS_PAL_MEM_FLAG_LARGE_PAGE     0x20000000

/** \brief Mask that covers all the memory protection flags. Do not use directly. */
#define PRS_PAL_MEM_FLAG_ALL_PROTECTION (PRS_PAL_MEM_FLAG_READ | \
                                         PRS_PAL_MEM_FLAG_WRITE | \
                                         PRS_PAL_MEM_FLAG_EXECUTE | \
                                         PRS_PAL_MEM_FLAG_GUARD_ONLY)

/**
 * \brief
 *  Maps virtual memory to the current operating system process.
 * \param size
 *  Number of bytes to map. Must be a multiple of the operating system's page size.
 * \param flags
 *  Virtual memory flags to apply to the mapped memory.
 * \return
 *  A pointer to the virtual memory area that was reserved, or \p null if the operation failed.
 */
void* prs_pal_mem_map(prs_size_t size, prs_pal_mem_flags_t flags);

/**
 * \brief
 *  Unmaps virtual memory from the current operating system process.
 * \param ptr
 *  Area of memory that was allocated by \ref prs_pal_mem_map.
 * \param size
 *  Number of bytes to unmap. Must be a multiple of the operating system's page size.
 */
void prs_pal_mem_unmap(void* ptr, prs_size_t size);

/**
 * \brief
 *  Commits virtual memory.
 * \param ptr
 *  Area of memory that was allocated by \ref prs_pal_mem_map.
 * \param size
 *  Number of bytes to commit. Must be a multiple of the operating system's page size.
 * \param flags
 *  Virtual memory flags to apply to the mapped memory.
 */
void prs_pal_mem_commit(void* ptr, prs_size_t size, prs_pal_mem_flags_t flags);

/**
 * \brief
 *  Uncommits virtual memory.
 * \param ptr
 *  Area of memory that was allocated by \ref prs_pal_mem_map and committed with \ref prs_pal_mem_commit.
 * \param size
 *  Number of bytes to commit. Must be a multiple of the operating system's page size.
 */
void prs_pal_mem_uncommit(void* ptr, prs_size_t size);

/**
 * \brief
 *  Locks virtual memory into physical memory.
 * \param ptr
 *  Area of memory that was allocated by \ref prs_pal_mem_map and committed with \ref prs_pal_mem_commit.
 * \param size
 *  Number of bytes to lock. Must be a multiple of the operating system's page size.
 */
void prs_pal_mem_lock(void* ptr, prs_size_t size);

/**
 * \brief
 *  Unlocks virtual memory from physical memory.
 * \param ptr
 *  Area of memory that was locked by \ref prs_pal_mem_lock.
 * \param size
 *  Number of bytes to unlock. Must be a multiple of the operating system's page size.
 */
void prs_pal_mem_unlock(void* ptr, prs_size_t size);

/**
 * \brief
 *  Change virtual memory access rights.
 * \param ptr
 *  Area of memory that was allocated by \ref prs_pal_mem_map and committed with \ref prs_pal_mem_commit.
 * \param size
 *  Number of bytes to change access rights for. Must be a multiple of the operating system's page size.
 * \param flags
 *  Virtual memory flags to apply to the mapped memory.
 */
void prs_pal_mem_protect(void* ptr, prs_size_t size, prs_pal_mem_flags_t flags);

#endif /* _PRS_PAL_MEM_H */

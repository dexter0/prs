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
 *  This file contains the POSIX virtual memory definitions.
 */

#include <sys/mman.h>
#if PRS_PAL_OS == PRS_PAL_OS_LINUX
/* Required on some distributions for MAP_HUGE_SHIFT */
#include <linux/mman.h>
#endif

#include <prs/pal/bitops.h>
#include <prs/pal/mem.h>
#include <prs/pal/os.h>
#include <prs/assert.h>
#include <prs/error.h>

static int prs_pal_mem_protect_flags(prs_pal_mem_flags_t flags)
{
    int prot = PROT_NONE;

    if (flags & PRS_PAL_MEM_FLAG_READ) {
        prot |= PROT_READ;
    }
    if (flags & PRS_PAL_MEM_FLAG_WRITE) {
        prot |= PROT_WRITE;
    }
    if (flags & PRS_PAL_MEM_FLAG_EXECUTE) {
        prot |= PROT_EXEC;
    }

    return prot;
}

void* prs_pal_mem_map(prs_size_t size, prs_pal_mem_flags_t flags)
{
    PRS_ASSERT(!(flags & PRS_PAL_MEM_FLAG_ALL_PROTECTION) || (flags & PRS_PAL_MEM_FLAG_COMMIT));
    PRS_ASSERT(!(flags & PRS_PAL_MEM_FLAG_LARGE_PAGE) || (flags & PRS_PAL_MEM_FLAG_COMMIT));
    
    int prot = PROT_NONE;
    int mmap_flags = MAP_PRIVATE | MAP_ANONYMOUS;

#if defined(PRS_ASSERTIONS)
    const prs_size_t page_size = (flags & PRS_PAL_MEM_FLAG_LARGE_PAGE) ?
        prs_pal_os_get_huge_page_size() :
        prs_pal_os_get_page_size();
    PRS_ASSERT((size % page_size) == 0);
#endif

    if (flags & PRS_PAL_MEM_FLAG_COMMIT) {
        if (flags & PRS_PAL_MEM_FLAG_ALL_PROTECTION) {
            prot = prs_pal_mem_protect_flags(flags);
        }
    }

    if (flags & PRS_PAL_MEM_FLAG_LARGE_PAGE) {
        mmap_flags |= MAP_HUGETLB;
        mmap_flags |= prs_bitops_hsb_uint(prs_pal_os_get_huge_page_size()) << MAP_HUGE_SHIFT;
    }
    
    if (flags & PRS_PAL_MEM_FLAG_LOCK) {
        mmap_flags |= MAP_LOCKED;
    }

    void* result = mmap(0, size, prot, mmap_flags, 0, 0);
    PRS_ERROR_WHEN(!result);
    return result;
}

void prs_pal_mem_unmap(void* ptr, prs_size_t size)
{
    const int error = munmap(ptr, size);
    PRS_ERROR_WHEN(error);
}

void prs_pal_mem_commit(void* ptr, prs_size_t size, prs_pal_mem_flags_t flags)
{
    PRS_ASSERT(ptr);
    const int prot = prs_pal_mem_protect_flags(flags);
    const int error = mprotect(ptr, size, prot);
    PRS_ERROR_WHEN(error);
}

void prs_pal_mem_uncommit(void* ptr, prs_size_t size)
{
    PRS_ASSERT(ptr);
    const int error = mprotect(ptr, size, PROT_NONE);
    PRS_ERROR_WHEN(error);
}

void prs_pal_mem_lock(void* ptr, prs_size_t size)
{
    PRS_ASSERT(ptr);
    const int error = mlock(ptr, size);
    PRS_ERROR_WHEN(error);
}

void prs_pal_mem_unlock(void* ptr, prs_size_t size)
{
    PRS_ASSERT(ptr);
    const int error = munlock(ptr, size);
    PRS_ERROR_WHEN(error);
}

void prs_pal_mem_protect(void* ptr, prs_size_t size, prs_pal_mem_flags_t flags)
{
    PRS_ASSERT(ptr);
    const int prot = prs_pal_mem_protect_flags(flags);
    const int error = mprotect(ptr, size, prot);
    PRS_ERROR_WHEN(error);
}

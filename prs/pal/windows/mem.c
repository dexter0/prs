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
 *  This file contains the Windows virtual memory definitions.
 */

#include <windows.h>

#include <prs/pal/mem.h>
#include <prs/pal/os.h>
#include <prs/assert.h>
#include <prs/error.h>

static DWORD prs_pal_mem_protect_flags(prs_pal_mem_flags_t flags)
{
    DWORD protect = PAGE_NOACCESS;

    if (flags & PRS_PAL_MEM_FLAG_READ) {
        if (flags & PRS_PAL_MEM_FLAG_WRITE) {
            if (flags & PRS_PAL_MEM_FLAG_EXECUTE) {
                protect = PAGE_EXECUTE_READWRITE;
            } else {
                protect = PAGE_READWRITE;
            }
        } else {
            if (flags & PRS_PAL_MEM_FLAG_EXECUTE) {
                protect = PAGE_EXECUTE_READ;
            } else {
                protect = PAGE_READONLY;
            }
        }
    } else {
        if (flags & PRS_PAL_MEM_FLAG_EXECUTE) {
            protect = PAGE_EXECUTE;
        }
    }

    if (flags & PRS_PAL_MEM_FLAG_GUARD_ONLY) {
        PRS_ASSERT(protect != PAGE_NOACCESS);
        protect |= PAGE_GUARD;
    }

    return protect;
}

static void* prs_pal_mem_alloc(void* ptr, prs_size_t size, prs_pal_mem_flags_t flags)
{
    DWORD protect = PAGE_NOACCESS;
    DWORD winflags = MEM_RESERVE;

#if defined(PRS_ASSERTIONS)
    const prs_size_t page_size = (flags & PRS_PAL_MEM_FLAG_LARGE_PAGE) ?
        prs_pal_os_get_huge_page_size() :
        prs_pal_os_get_page_size();
    PRS_ASSERT((size % page_size) == 0);
#endif

    if (flags & PRS_PAL_MEM_FLAG_COMMIT) {
        if (ptr) {
            winflags = MEM_COMMIT;
        } else {
            winflags |= MEM_COMMIT;
        }
        if (flags & PRS_PAL_MEM_FLAG_ALL_PROTECTION) {
            protect = prs_pal_mem_protect_flags(flags);
        }
    }

    if (flags & PRS_PAL_MEM_FLAG_LARGE_PAGE) {
        PRS_ASSERT(flags & PRS_PAL_MEM_FLAG_COMMIT);
        winflags |= MEM_LARGE_PAGES;
    }

    void* result = VirtualAlloc(ptr, size, winflags, protect);
    PRS_ERROR_WHEN(!result);
    if (flags & PRS_PAL_MEM_FLAG_LOCK) {
        prs_pal_mem_lock(ptr, size);
    }
    return result;
}

void* prs_pal_mem_map(prs_size_t size, prs_pal_mem_flags_t flags)
{
    PRS_ASSERT(!(flags & PRS_PAL_MEM_FLAG_ALL_PROTECTION) || (flags & PRS_PAL_MEM_FLAG_COMMIT));
    PRS_ASSERT(!(flags & PRS_PAL_MEM_FLAG_LARGE_PAGE) || (flags & PRS_PAL_MEM_FLAG_COMMIT));
    return prs_pal_mem_alloc(0, size, flags);
}

void prs_pal_mem_unmap(void* ptr, prs_size_t size)
{
    const BOOL result = VirtualFree(ptr, 0, MEM_RELEASE);
    PRS_ERROR_WHEN(!result);
}

void prs_pal_mem_commit(void* ptr, prs_size_t size, prs_pal_mem_flags_t flags)
{
    PRS_ASSERT(ptr);
    flags |= PRS_PAL_MEM_FLAG_COMMIT;
    const void* new_ptr = prs_pal_mem_alloc(ptr, size, flags);
    PRS_ASSERT(new_ptr == ptr);
}

void prs_pal_mem_uncommit(void* ptr, prs_size_t size)
{
    PRS_ASSERT(ptr);
    const BOOL result = VirtualFree(ptr, size, MEM_DECOMMIT);
    PRS_ERROR_WHEN(!result);
}

void prs_pal_mem_lock(void* ptr, prs_size_t size)
{
    PRS_ASSERT(ptr);
    const BOOL result = VirtualLock(ptr, size);
    PRS_ERROR_WHEN(!result);
}

void prs_pal_mem_unlock(void* ptr, prs_size_t size)
{
    PRS_ASSERT(ptr);
    const BOOL result = VirtualUnlock(ptr, size);
    PRS_ERROR_WHEN(!result);
}

void prs_pal_mem_protect(void* ptr, prs_size_t size, prs_pal_mem_flags_t flags)
{
    PRS_ASSERT(ptr);
    const DWORD protect = prs_pal_mem_protect_flags(flags);
    DWORD old_protect = 0;
    const BOOL result = VirtualProtect(ptr, size, protect, &old_protect);
    PRS_ERROR_WHEN(!result);
}

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
 *  This file contains the pointer directory definitions.
 *
 *  A pointer directory (PD) is a fixed size table which contains IDs and pointers. The goal of the pointer directory
 *  is to provide simultaneous reference-counted access to pointers from multiple workers.
 *
 *  With the current atomic primitives in most hardware architectures, it is not possible to have a smart pointer that
 *  works across multiple threads.  The issue is that the memory area where the reference count is located might be
 *  freed between the time it is dereferenced and incremented.
 *
 *  By using a unique ID which serves as an index in a static table, it is possible to achieve the smart pointer
 *  functionality. The user uses a \ref prs_pd_id_t ID instead of a smart pointer, dereferences the ID using
 *  \ref prs_pd_lock and releases the reference using \ref prs_pd_unlock.
 */

#include <prs/pal/arch.h>
#include <prs/pal/atomic.h>
#include <prs/pal/bitops.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/rtc.h>
#include <prs/pd.h>

#if PRS_PAL_POINTER_SIZE == 4
#define PRS_PD_REFCNT_SHIFT             20
#elif PRS_PAL_POINTER_SIZE == 8
#define PRS_PD_REFCNT_SHIFT             52
#else
#error Unknown pointer size.
#endif

#define PRS_PD_REFCNT_BITS              12
#define PRS_PD_REFCNT_INC               ((prs_pd_id_t)1 << PRS_PD_REFCNT_SHIFT)
#define PRS_PD_REFCNT_MASK              ((((prs_pd_id_t)1 << PRS_PD_REFCNT_BITS) - 1) << PRS_PD_REFCNT_SHIFT)

#define PRS_PD_ID_MASK                  (PRS_PD_REFCNT_INC - 1)

#define PRS_PD_GET_ID(header)           ((header) & PRS_PD_ID_MASK)
#define PRS_PD_TEST_REFCNT(header)      ((header) & PRS_PD_REFCNT_MASK)
#define PRS_PD_GET_REFCNT(header)       (PRS_PD_TEST_REFCNT(header) >> PRS_PD_REFCNT_SHIFT)

typedef prs_pd_id_t prs_pd_index_t;

struct prs_pd_entry {
    PRS_ATOMIC prs_pd_id_t              header;
    void*                               ptr;
};

struct prs_pd {
    void*                               area;
    struct prs_pd_entry*                entries;
    prs_pd_index_t                      max_entries;
    prs_pd_index_t                      max_entries_mask;
    PRS_ATOMIC prs_pd_index_t           write_index;
};

static struct prs_pd_entry* prs_pd_get_entry(struct prs_pd* pd, prs_pd_id_t id)
{
    PRS_PRECONDITION(pd);
    PRS_PRECONDITION(pd->max_entries_mask > 0);
    PRS_PRECONDITION(id != PRS_PD_ID_INVALID);
    const prs_pd_index_t index = id & pd->max_entries_mask;
    return &pd->entries[index];
}

/**
 * \brief
 *  Returns the size of the pointer directory data structures allocated by \ref prs_pd_create.
 * \param params
 *  Pointer directory parameters.
 */
prs_size_t prs_pd_struct_size(struct prs_pd_create_params* params)
{
    return sizeof(struct prs_pd) + sizeof(struct prs_pd_entry) * params->max_entries;
}

/**
 * \brief
 *  Creates a pointer directory.
 * \param params
 *  Pointer directory parameters.
 */
struct prs_pd* prs_pd_create(struct prs_pd_create_params* params)
{
    PRS_PRECONDITION(params);
    PRS_PRECONDITION(prs_bitops_is_power_of_2(params->max_entries));

    struct prs_pd* pd = params->area;
    if (pd) {
        pd->area = params->area;
        pd->entries = (struct prs_pd_entry*)(pd + 1);
    } else {
        pd = prs_pal_malloc_zero(sizeof(*pd));
        if (!pd) {
            goto cleanup;
        }

        pd->entries = prs_pal_malloc_zero(sizeof(*pd->entries) * params->max_entries);
        if (!pd->entries) {
            goto cleanup;
        }
    }

    pd->max_entries = params->max_entries;
    pd->max_entries_mask = pd->max_entries - 1;
    prs_pal_atomic_store(&pd->write_index, 1);

    return pd;

    cleanup:

    if (pd && !params->area) {
        if (pd->entries) {
            prs_pal_free(pd->entries);
        }

        prs_pal_free(pd);
    }

    return 0;
}

/**
 * \brief
 *  Destroys a pointer directory.
 * \param pd
 *  Pointer directory to destroy.
 */
void prs_pd_destroy(struct prs_pd* pd)
{
    if (!pd->area) {
        prs_pal_free(pd->entries);
        prs_pal_free(pd);
    }
}

/**
 * \brief
 *  Allocates and lock a pointer directory entry.
 * \param pd
 *  Pointer directory to allocate from.
 * \param ptr
 *  Pointer to copy into the allocated entry.
 * \param id
 *  ID of the entry that was allocated.
 * \return
 *  \ref PRS_OUT_OF_MEMORY if the pointer directory was full.
 *  \ref PRS_OK if the entry was successfully allocated.
 */
prs_result_t prs_pd_alloc_and_lock(struct prs_pd* pd, void* ptr, prs_pd_id_t* id)
{
    struct prs_pd_entry* entry;
    prs_pd_index_t new_header;

    const prs_pd_index_t write_index = prs_pal_atomic_load(&pd->write_index);
    const prs_pd_index_t end = write_index + pd->max_entries;

    for (prs_pd_index_t i = write_index; i < end; ++i) {
        entry = prs_pd_get_entry(pd, i);
        if (!entry) {
            continue;
        }

        prs_pd_index_t header = prs_pal_atomic_load(&entry->header);
        if (PRS_PD_TEST_REFCNT(header)) {
            continue;
        }

        /*
         * Very unlikely scenario: the ID is the same. If we accept to allocate a new entry with the same entry that
         * existed before, we are possibly enabling an ABA scenario.
         * Note: this will happen every time for the first object ever allocated, because memory will be zeroed.
         */
        if (PRS_PD_GET_ID(header) == i) {
            continue;
        }

        new_header = PRS_PD_REFCNT_INC | (i & PRS_PD_ID_MASK);
        if (!prs_pal_atomic_compare_exchange_strong(&entry->header, &header, new_header)) {
            continue;
        }

        prs_pal_atomic_store(&pd->write_index, i + 1);
        entry->ptr = ptr;
        *id = i;

        PRS_POSTCONDITION(*id != PRS_PD_ID_INVALID);
        return PRS_OK;
    }

    *id = PRS_PD_ID_INVALID;
    return PRS_OUT_OF_MEMORY;
}

/**
 * \brief
 *  Locks a pointer directory entry and returns its pointer.
 * \param pd
 *  Pointer directory to lock from.
 * \param id
 *  ID of the entry to lock.
 * \return
 *  A pointer to the entry that was locked, or \p null if it was not found.
 */
void* prs_pd_lock(struct prs_pd* pd, prs_pd_id_t id)
{
    PRS_PRECONDITION(pd);
    PRS_RTC_IF (id == PRS_PD_ID_INVALID) {
        return 0;
    }

    struct prs_pd_entry* entry = prs_pd_get_entry(pd, id);
    if (!entry) {
        return 0;
    }

    prs_pd_index_t header = prs_pal_atomic_load(&entry->header);
    prs_pd_index_t new_header;

    do {
        if (PRS_PD_GET_ID(header) != id) {
            return 0;
        }
        if (!PRS_PD_TEST_REFCNT(header)) {
            return 0;
        }
        PRS_ERROR_IF (PRS_PD_TEST_REFCNT(header) == PRS_PD_REFCNT_MASK) {
            return 0;
        }
        new_header = header + PRS_PD_REFCNT_INC;
    } while (!prs_pal_atomic_compare_exchange_weak(&entry->header, &header, new_header));

    return entry->ptr;
}

/**
 * \brief
 *  Unlocks a pointer directory entry.
 * \param pd
 *  Pointer directory to unlock from.
 * \param id
 *  ID of the entry to unlock.
 * \return
 *  \ref PRS_TRUE if the entry was freed after it was unlocked.
 *  \ref PRS_FALSE if the entry was not freed after it was unlocked.
 */
prs_bool_t prs_pd_unlock(struct prs_pd* pd, prs_pd_id_t id)
{
    PRS_PRECONDITION(pd);
    PRS_RTC_IF (id == PRS_PD_ID_INVALID) {
        return PRS_FALSE;
    }

    struct prs_pd_entry* entry = prs_pd_get_entry(pd, id);
    PRS_RTC_IF (!entry) {
        return PRS_FALSE;
    }

    prs_bool_t last = PRS_FALSE;
    prs_pd_index_t header = prs_pal_atomic_load(&entry->header);
    prs_pd_index_t new_header;

    do {
        PRS_RTC_IF (PRS_PD_GET_ID(header) != id) {
            return PRS_FALSE;
        }
        const prs_pd_id_t refcnt = PRS_PD_TEST_REFCNT(header);
        PRS_RTC_IF (!refcnt) {
            return PRS_FALSE;
        }
        new_header = header - PRS_PD_REFCNT_INC;
        last = PRS_BOOL(refcnt == PRS_PD_REFCNT_INC);
    } while (!prs_pal_atomic_compare_exchange_weak(&entry->header, &header, new_header));

    return last;
}



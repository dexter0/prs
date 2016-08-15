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
 *  This file contains the pool definitions.
 *
 *  The pool is a data structure that supports multiple simultaneous threads accessing it at once to allocate, free,
 *  lock or unlock entries. It has a fixed number of entries of a fixed size.
 *
 *  Entries in the pool can be allocated with \ref prs_pool_alloc or \ref prs_pool_alloc_and_lock and freed with
 *  \ref prs_pool_free, \ref prs_pool_unlock, \ref prs_pool_try_unlock_final, \ref prs_pool_unlock_dest or
 *  \ref prs_pool_try_unlock_final_dest.
 *
 *  Entries are reference counted, so that the last worker that unlocks an entry also frees it.
 */

#include <stddef.h>
#include <string.h>

#include <prs/pal/arch.h>
#include <prs/pal/atomic.h>
#include <prs/pal/bitops.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/rtc.h>
#include <prs/pool.h>

#if PRS_PAL_POINTER_SIZE == 4
#define PRS_POOL_REFCNT_SHIFT           20
#elif PRS_PAL_POINTER_SIZE == 8
#define PRS_POOL_REFCNT_SHIFT           52
#else
#error Unknown pointer size.
#endif
#define PRS_POOL_REFCNT_BITS            11
#define PRS_POOL_REFCNT_INC             ((prs_pool_id_t)1 << PRS_POOL_REFCNT_SHIFT)
#define PRS_POOL_REFCNT_MASK            ((((prs_pool_id_t)1 << PRS_POOL_REFCNT_BITS) - 1) << PRS_POOL_REFCNT_SHIFT)

#define PRS_POOL_ALLOC_SHIFT            (PRS_POOL_REFCNT_SHIFT + PRS_POOL_REFCNT_BITS)
#define PRS_POOL_ALLOC_MASK             ((prs_pool_id_t)1 << PRS_POOL_ALLOC_SHIFT)

#define PRS_POOL_ID_MASK                (PRS_POOL_REFCNT_INC - 1)

#define PRS_POOL_GET_ID(header)         ((header) & PRS_POOL_ID_MASK)
#define PRS_POOL_TEST_REFCNT(header)    ((header) & PRS_POOL_REFCNT_MASK)
#define PRS_POOL_GET_REFCNT(header)     (PRS_POOL_TEST_REFCNT(header) >> PRS_POOL_REFCNT_SHIFT)
#define PRS_POOL_TEST_ALLOC(header)     ((header) & PRS_POOL_ALLOC_MASK)

typedef prs_pool_id_t prs_pool_index_t;
typedef prs_pool_id_t prs_pool_header_t;

struct prs_pool_entry {
    PRS_ATOMIC prs_pool_header_t        header;
    prs_uint8_t                         data[1];
};

struct prs_pool {
    void*                               area;
    void*                               entries;
    prs_size_t                          data_size;
    prs_size_t                          entry_size;
    prs_pool_index_t                    max_entries;
    prs_pool_index_t                    max_entries_mask;
    PRS_ATOMIC prs_pool_index_t         write_index;
};

static struct prs_pool_entry* prs_pool_get_entry(struct prs_pool* pool, prs_pool_id_t id)
{
    PRS_PRECONDITION(pool);
    PRS_PRECONDITION(pool->max_entries_mask > 0);
    PRS_PRECONDITION(id != PRS_POOL_ID_INVALID);
    const prs_pool_index_t index = id & pool->max_entries_mask;
    const prs_size_t pos = index * pool->entry_size;
    return (struct prs_pool_entry*)&((prs_uint8_t*)pool->entries)[pos];
}

/**
 * \brief
 *  Returns the size of the pool data structures allocated by \ref prs_pool_create.
 * \param params
 *  Pool parameters.
 */
prs_size_t prs_pool_struct_size(struct prs_pool_create_params* params)
{
    const prs_size_t entry_size = params->data_size + offsetof(struct prs_pool_entry, data);
    return sizeof(struct prs_pool) + entry_size * params->max_entries;
}

/**
 * \brief
 *  Creates a pool.
 * \param params
 *  Pool parameters.
 */
struct prs_pool* prs_pool_create(struct prs_pool_create_params* params)
{
    PRS_PRECONDITION(params);
    PRS_PRECONDITION(prs_bitops_is_power_of_2(params->max_entries));
    PRS_PRECONDITION(!(params->data_size & (PRS_PAL_POINTER_SIZE - 1))); /* Align on PRS_PAL_POINTER_SIZE bytes */

    const prs_size_t entry_size = params->data_size + offsetof(struct prs_pool_entry, data);

    struct prs_pool* pool = params->area;
    if (pool) {
        pool->area = params->area;
        pool->entries = (struct prs_pool_entry*)(pool + 1);
    } else {
        pool = prs_pal_malloc_zero(sizeof(*pool));
        if (!pool) {
            goto cleanup;
        }

        pool->entries = prs_pal_malloc_zero(entry_size * params->max_entries);
        if (!pool->entries) {
            goto cleanup;
        }
    }

    pool->data_size = params->data_size;
    pool->entry_size = entry_size;
    pool->max_entries = params->max_entries;
    pool->max_entries_mask = pool->max_entries - 1;
    prs_pal_atomic_store(&pool->write_index, 1);

    /*
     * Initialize the first entry so that it has an ID different than zero. This is to ensure that we can allocate the
     * first object at the first position.
     */
    struct prs_pool_entry* entry = prs_pool_get_entry(pool, 0);
    prs_pal_atomic_store(&entry->header, PRS_POOL_ALLOC_MASK);

    return pool;

    cleanup:

    if (pool && !params->area) {
        if (pool->entries) {
            prs_pal_free(pool->entries);
        }

        prs_pal_free(pool);
    }

    return 0;
}

/**
 * \brief
 *  Destroys a pool.
 * \param params
 *  Pool to destroy.
 */
void prs_pool_destroy(struct prs_pool* pool)
{
    if (!pool->area) {
        prs_pal_free(pool->entries);
        prs_pal_free(pool);
    }
}

/**
 * \brief
 *  Allocates an entry in the pool.
 * \param pool
 *  Pool to allocate from.
 * \param id
 *  ID of the entry allocated in the pool
 * \return
 *  The pool entry that was allocated, or \p null if no free entries were found.
 * \note
 *  An entry that was allocated with this function can be immediately freed with \ref prs_pool_free. Otherwise, it can
 *  make use of the reference locking mechanism by locking it once with \ref prs_pool_lock_first.
 */
void* prs_pool_alloc(struct prs_pool* pool, prs_pool_id_t* id)
{
    PRS_PRECONDITION(pool);

    struct prs_pool_entry* entry;
    prs_pool_header_t new_header;

    const prs_pool_index_t write_index = prs_pal_atomic_load(&pool->write_index);
    const prs_pool_index_t end = write_index + pool->max_entries;

    for (prs_pool_index_t i = write_index; i < end; ++i) {
        entry = prs_pool_get_entry(pool, i);
        if (!entry) {
            continue;
        }

        prs_pool_header_t header = prs_pal_atomic_load(&entry->header);
        if (PRS_POOL_TEST_ALLOC(header)) {
            continue;
        }

        /*
         * Very unlikely scenario: the ID is the same. If we accept to allocate a new entry with the same entry that
         * existed before, we are possibly enabling an ABA scenario.
         * Note: this could happen every time for the first object ever allocated, because memory will be zeroed.
         */
        if (PRS_POOL_GET_ID(header) == i) {
            continue;
        }

        /* Only allocate the entry here. Since the reference counter will still be zero, it will not be lockable yet. */
        new_header = PRS_POOL_ALLOC_MASK | (i & PRS_POOL_ID_MASK);
        if (!prs_pal_atomic_compare_exchange_strong(&entry->header, &header, new_header)) {
            continue;
        }

        prs_pal_atomic_store(&pool->write_index, i + 1);
        if (id) {
            *id = i;
            PRS_POSTCONDITION(*id != PRS_POOL_ID_INVALID);
        }

        return entry->data;
    }

    if (id) {
        *id = PRS_POOL_ID_INVALID;
    }
    return 0;
}

/**
 * \brief
 *  Frees an entry to the pool.
 * \param pool
 *  Pool to free to.
 * \param id
 *  ID of the entry to free.
 * \note
 *  An entry can only be freed if it was never locked firsthand.
 */
void prs_pool_free(struct prs_pool* pool, prs_pool_id_t id)
{
    PRS_PRECONDITION(pool);
    PRS_PRECONDITION(id != PRS_POOL_ID_INVALID);

    struct prs_pool_entry* entry = prs_pool_get_entry(pool, id);
#if defined(PRS_ASSERTIONS) || defined(PRS_RUN_TIME_CHECKING)
    prs_pool_header_t header = prs_pal_atomic_load(&entry->header);

    PRS_RTC_IF (!PRS_POOL_TEST_ALLOC(header)) {
        return;
    }
    PRS_RTC_IF (PRS_POOL_TEST_REFCNT(header)) {
        return;
    }

    prs_pool_header_t new_header = id;
    const prs_bool_t success = PRS_BOOL(prs_pal_atomic_compare_exchange_strong(&entry->header, &header, new_header));
    PRS_RTC_IF (!success) {
        return;
    }
#else
    prs_pal_atomic_store(&entry->header, id);
#endif
}

/**
 * \brief
 *  Locks a pool entry that was just allocated by \ref prs_pool_alloc.
 * \param pool
 *  Pool in which the entry resides.
 * \param id
 *  ID of the entry to lock.
 */
void prs_pool_lock_first(struct prs_pool* pool, prs_pool_id_t id)
{
    PRS_PRECONDITION(pool);
    PRS_PRECONDITION(id != PRS_POOL_ID_INVALID);

    struct prs_pool_entry* entry = prs_pool_get_entry(pool, id);
#if defined(PRS_ASSERTIONS) || defined(PRS_RUN_TIME_CHECKING)
    prs_pool_header_t header = prs_pal_atomic_load(&entry->header);

    PRS_RTC_IF (PRS_POOL_GET_ID(header) != id) {
        return;
    }
    PRS_RTC_IF (!PRS_POOL_TEST_ALLOC(header)) {
        return;
    }
    PRS_RTC_IF (PRS_POOL_TEST_REFCNT(header)) {
        return;
    }

    prs_pool_header_t new_header = PRS_POOL_REFCNT_INC | header;
    const prs_bool_t success = PRS_BOOL(prs_pal_atomic_compare_exchange_strong(&entry->header, &header, new_header));
    PRS_RTC_IF (!success) {
        return;
    }
#else
    prs_pal_atomic_store(&entry->header, PRS_POOL_ALLOC_MASK | PRS_POOL_REFCNT_INC | id);
#endif
}

/**
 * \brief
 *  Allocates and lock a pool entry.
 * \param pool
 *  Pool to allocate from.
 * \param data
 *  Data to copy into the allocated entry.
 * \param id
 *  ID of the entry that was allocated.
 * \return
 *  A pointer to the entry that was allocated.
 */
void* prs_pool_alloc_and_lock(struct prs_pool* pool, void* data, prs_pool_id_t* id)
{
    PRS_PRECONDITION(pool);
    PRS_PRECONDITION(data);
    PRS_PRECONDITION(id);

    void* result = prs_pool_alloc(pool, id);
    if (!result) {
        PRS_ASSERT(*id == PRS_POOL_ID_INVALID);
        return 0;
    }
    PRS_ASSERT(*id != PRS_POOL_ID_INVALID);
    memcpy(result, data, pool->data_size);
    prs_pool_lock_first(pool, *id);
    return result;
}

/**
 * \brief
 *  Locks a pool entry.
 * \param pool
 *  Pool to lock from.
 * \param id
 *  ID of the entry to lock.
 * \return
 *  A pointer to the entry that was locked, or \p null if it was not found.
 */
void* prs_pool_lock(struct prs_pool* pool, prs_pool_id_t id)
{
    PRS_PRECONDITION(pool);
    PRS_RTC_IF (id == PRS_POOL_ID_INVALID) {
        return 0;
    }

    struct prs_pool_entry* entry = prs_pool_get_entry(pool, id);
    if (!entry) {
        return 0;
    }

    prs_pool_header_t header = prs_pal_atomic_load(&entry->header);
    prs_pool_header_t new_header;

    do {
        if (PRS_POOL_GET_ID(header) != id) {
            return 0;
        }
        if (!PRS_POOL_TEST_ALLOC(header)) {
            return 0;
        }
        if (!PRS_POOL_TEST_REFCNT(header)) {
            return 0;
        }
        PRS_ERROR_IF (PRS_POOL_TEST_REFCNT(header) == PRS_POOL_REFCNT_MASK) {
            return 0;
        }
        new_header = header + PRS_POOL_REFCNT_INC;
    } while (!prs_pal_atomic_compare_exchange_weak(&entry->header, &header, new_header));

    return entry->data;
}

static prs_bool_t prs_pool_unlock_internal(struct prs_pool* pool, prs_pool_id_t id, prs_bool_t try_final, void (*destructor)(void* userdata, void* data), void* userdata)
{
    PRS_PRECONDITION(pool);
    PRS_RTC_IF (id == PRS_POOL_ID_INVALID) {
        return PRS_FALSE;
    }

    struct prs_pool_entry* entry = prs_pool_get_entry(pool, id);
    PRS_RTC_IF (!entry) {
        return PRS_FALSE;
    }

    prs_bool_t last = PRS_FALSE;
    prs_pool_header_t header = prs_pal_atomic_load(&entry->header);
    prs_pool_header_t new_header;

    do {
        PRS_RTC_IF (PRS_POOL_GET_ID(header) != id) {
            return PRS_FALSE;
        }
        PRS_RTC_IF (!PRS_POOL_TEST_ALLOC(header)) {
            return PRS_FALSE;
        }
        const prs_pool_id_t refcnt = PRS_POOL_TEST_REFCNT(header);
        PRS_RTC_IF (!refcnt) {
            return PRS_FALSE;
        }
        last = PRS_BOOL(refcnt == PRS_POOL_REFCNT_INC);
        if (try_final && !last) {
            return PRS_FALSE;
        }
        new_header = header - PRS_POOL_REFCNT_INC;
        if (last && !destructor) {
            new_header &= ~PRS_POOL_ALLOC_MASK;
        }
    } while (!prs_pal_atomic_compare_exchange_weak(&entry->header, &header, new_header));

    if (last && destructor) {
        destructor(userdata, entry->data);
#if defined(PRS_ASSERTIONS) || defined(PRS_RUN_TIME_CHECKING)
        header = new_header;
#endif
        new_header &= PRS_POOL_ID_MASK;
#if defined(PRS_ASSERTIONS) || defined(PRS_RUN_TIME_CHECKING)
        const prs_bool_t result = prs_pal_atomic_compare_exchange_strong(&entry->header, &header, new_header);
        PRS_RTC_IF (!result) {
            return PRS_FALSE;
        }
#else
        prs_pal_atomic_store(&entry->header, new_header);
#endif
    }

    return last;
}

/**
 * \brief
 *  Unlocks a pool entry.
 * \param pool
 *  Pool to unlock from.
 * \param id
 *  ID of the entry to unlock.
 * \return
 *  \ref PRS_TRUE if the entry was freed after it was unlocked.
 *  \ref PRS_FALSE if the entry was not freed after it was unlocked.
 */
prs_bool_t prs_pool_unlock(struct prs_pool* pool, prs_pool_id_t id)
{
    return prs_pool_unlock_internal(pool, id, PRS_FALSE, 0, 0);
}

/**
 * \brief
 *  Unlocks a pool entry only if the reference count is one.
 * \param pool
 *  Pool to unlock from.
 * \param id
 *  ID of the entry to unlock.
 * \return
 *  \ref PRS_TRUE if the entry was freed after it was unlocked.
 *  \ref PRS_FALSE if the entry was not freed after it was unlocked, or if the reference counter was not one.
 */
prs_bool_t prs_pool_try_unlock_final(struct prs_pool* pool, prs_pool_id_t id)
{
    return prs_pool_unlock_internal(pool, id, PRS_TRUE, 0, 0);
}

/**
 * \brief
 *  Unlocks a pool entry. Execute the specified destructor before freeing the entry.
 * \param pool
 *  Pool to unlock from.
 * \param id
 *  ID of the entry to unlock.
 * \param destructor
 *  Function to execute when the entry is freed.
 * \param userdata
 *  Userdata to pass to the destructor.
 * \return
 *  \ref PRS_TRUE if the entry was freed after it was unlocked.
 *  \ref PRS_FALSE if the entry was not freed after it was unlocked.
 */
prs_bool_t prs_pool_unlock_dest(struct prs_pool* pool, prs_pool_id_t id, void (*destructor)(void* userdata, void* data), void* userdata)
{
    return prs_pool_unlock_internal(pool, id, PRS_FALSE, destructor, userdata);
}

/**
 * \brief
 *  Unlocks a pool entry only if the reference count is one. Execute the specified destructor before freeing the entry.
 * \param pool
 *  Pool to unlock from.
 * \param id
 *  ID of the entry to unlock.
 * \param destructor
 *  Function to execute when the entry is freed.
 * \param userdata
 *  Userdata to pass to the destructor.
 * \return
 *  \ref PRS_TRUE if the entry was freed after it was unlocked.
 *  \ref PRS_FALSE if the entry was not freed after it was unlocked, or if the reference counter was not one.
 */
prs_bool_t prs_pool_try_unlock_final_dest(struct prs_pool* pool, prs_pool_id_t id, void (*destructor)(void* userdata, void* data), void* userdata)
{
    return prs_pool_unlock_internal(pool, id, PRS_TRUE, destructor, userdata);
}

static struct prs_pool_entry* prs_pool_data_to_entry(void* data)
{
    return (struct prs_pool_entry*)((prs_uintptr_t)data - offsetof(struct prs_pool_entry, data));
}

/**
 * \brief
 *  Returns the ID of the specified data.
 * \param pool
 *  Pool to get the ID from.
 * \param data
 *  Data to get the ID from.
 */
prs_pool_id_t prs_pool_get_id(struct prs_pool* pool, void* data)
{
    PRS_PRECONDITION(pool);
    PRS_PRECONDITION(data);

    struct prs_pool_entry* entry = prs_pool_data_to_entry(data);
    PRS_ASSERT(entry);

    const prs_intptr_t diff = (prs_intptr_t)entry - (prs_intptr_t)pool->entries;
    const prs_pool_index_t index = diff / pool->entry_size;
    PRS_ASSERT(index * pool->entry_size == diff);

    const prs_pool_index_t header = prs_pal_atomic_load(&entry->header);
    PRS_ASSERT(PRS_POOL_TEST_ALLOC(header));
    return PRS_POOL_GET_ID(header);
}

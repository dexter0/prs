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
 *  This file contains the global object directory definitions.
 *
 *  The global object directory (GOD) is a fixed size table which contains IDs and references to registered objects.
 *  The goal of the GOD is to provide simultaneous access to objects from multiple workers and providing general
 *  functionality such as destructors and unique IDs.
 *
 *  Once an object is created, it can be registered with \ref prs_god_alloc_and_lock to obtain a unique ID. The ID can
 *  then be used with \ref prs_god_lock and \ref prs_god_unlock to acquire and release references to the object.
 *
 *  The caller of \ref prs_god_alloc_and_lock must also provide a set of generic functions. The most important function
 *  is the one to free the object once its reference count reaches zero.
 *
 * \note
 *  Only one GOD can exist at any time.
 */

#include <prs/pal/atomic.h>
#include <prs/pal/bitops.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/god.h>
#include <prs/rtc.h>

#define PRS_GOD_HEADER_BITS             (sizeof(prs_god_entry_header_t) * 8)
#define PRS_GOD_ID_BITS                 32
#define PRS_GOD_INDEX_MASK              (((prs_god_entry_header_t)1 << PRS_GOD_ID_BITS) - 1)
#define PRS_GOD_NONID_BITS              (PRS_GOD_HEADER_BITS - PRS_GOD_ID_BITS)
#define PRS_GOD_HEADER_ID               ((((prs_god_entry_header_t)1 << PRS_GOD_ID_BITS) - 1) << PRS_GOD_NONID_BITS)
#define PRS_GOD_HEADER_RESERVED         0x0000000000004000
#define PRS_GOD_HEADER_USED             0x0000000000002000
#define PRS_GOD_HEADER_DELETE_MARK      0x0000000000001000
#define PRS_GOD_HEADER_REFCNT           0x0000000000000FFF
#define PRS_GOD_TEST_FLAG(header, flag) ((header) & (flag))
#define PRS_GOD_GET_ID(flags)           (((flags) & PRS_GOD_HEADER_ID) >> PRS_GOD_NONID_BITS)
#define PRS_GOD_SET_ID(id)              ((prs_god_entry_header_t)(id) << PRS_GOD_NONID_BITS)

typedef prs_object_id_t prs_god_index_t;

typedef prs_uint64_t prs_god_entry_header_t;

struct prs_god_entry {
    PRS_ATOMIC prs_god_entry_header_t   header;
    void*                               object;
    struct prs_object_ops*              ops;
};

struct prs_god {
    struct prs_god_entry*               entries;
    prs_god_index_t                     max_entries;
    prs_god_index_t                     max_entries_mask;
    PRS_ATOMIC prs_god_index_t          write_index;
};

static struct prs_god* s_god = 0;

static struct prs_god* prs_god_get(void)
{
    PRS_PRECONDITION(s_god);
    return s_god;
}

static struct prs_god_entry* prs_god_get_entry(struct prs_god* god, prs_object_id_t id)
{
    PRS_PRECONDITION(god);
    PRS_PRECONDITION(god->max_entries_mask > 0);
    const prs_god_index_t index = id & god->max_entries_mask;
    return &god->entries[index];
}

/**
 * \brief
 *  Initializes the global object directory.
 * \param params
 *  Parameters for the global object directory.
 */
prs_result_t prs_god_create(struct prs_god_create_params* params)
{
    PRS_PRECONDITION(!s_god);
    PRS_PRECONDITION(params);
    PRS_PRECONDITION(prs_bitops_is_power_of_2(params->max_entries));

    prs_result_t result = PRS_OK;
    struct prs_god* god = prs_pal_malloc_zero(sizeof(*god));
    if (!god) {
        result = PRS_OUT_OF_MEMORY;
        goto cleanup;
    }

    god->entries = prs_pal_malloc_zero(sizeof(*god->entries) * params->max_entries);
    if (!god->entries) {
        result = PRS_OUT_OF_MEMORY;
        goto cleanup;
    }

    god->max_entries = params->max_entries;
    god->max_entries_mask = god->max_entries - 1;
    prs_pal_atomic_store(&god->write_index, 1);

    prs_pal_atomic_store(&god->entries->header, PRS_GOD_HEADER_RESERVED | PRS_GOD_HEADER_USED);

    s_god = god;

    return result;

    cleanup:

    if (god->entries) {
        prs_pal_free(god->entries);
    }
    if (god) {
        prs_pal_free(god);
    }
    return result;
}

/**
 * \brief
 *  Destroys the global object directory.
 */
void prs_god_destroy(void)
{
    struct prs_god* god = prs_god_get();
    prs_pal_free(god->entries);
    prs_pal_free(god);
}

/**
 * \brief
 *  Registers an object into the GOD and sets its reference counter to one.
 * \param object
 *  Object to register. This is the value that will be returned by \ref prs_god_lock and passed in parameters to the
 *  functions provided by \p ops.
 * \param ops
 *  Generic functions that can be called for this object.
 * \return
 *  The unique ID for this object, or \ref PRS_OBJECT_ID_INVALID.
 */
prs_object_id_t prs_god_alloc_and_lock(void* object, struct prs_object_ops* ops)
{
    struct prs_god* god = prs_god_get();

    struct prs_god_entry* entry;
    prs_god_entry_header_t new_header;

    const prs_god_index_t write_index = prs_pal_atomic_load(&god->write_index);
    const prs_god_index_t end = write_index + god->max_entries;

    for (prs_god_index_t i = write_index; i < end; ++i) {
        entry = prs_god_get_entry(god, i);

        prs_god_entry_header_t header = prs_pal_atomic_load(&entry->header);
        if (PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_RESERVED)) {
            continue;
        }

        /*
         * Very unlikely scenario: the ID is the same. If we accept to allocate a new entry with the same entry that
         * existed before, we are possibly enabling an ABA scenario.
         * Note: this will happen every time for the first object ever allocated, because memory will be zeroed.
         */
        if (PRS_GOD_GET_ID(header) == i) {
            continue;
        }

        new_header = PRS_GOD_SET_ID(i) | PRS_GOD_HEADER_RESERVED | 1;
        if (!prs_pal_atomic_compare_exchange_strong(&entry->header, &header, new_header)) {
            continue;
        }

        prs_pal_atomic_store(&god->write_index, i + 1);

        entry->object = object;
        entry->ops = ops;
        new_header |= PRS_GOD_HEADER_USED;
        prs_pal_atomic_store(&entry->header, new_header);

        return i;
    }

    return PRS_OBJECT_ID_INVALID;
}

static void prs_god_free(prs_object_id_t id)
{
    PRS_ERROR_IF (id == PRS_OBJECT_ID_INVALID) {
        return;
    }

    struct prs_god* god = prs_god_get();
    struct prs_god_entry* entry = prs_god_get_entry(god, id);

    prs_god_entry_header_t header = prs_pal_atomic_load(&entry->header);

#if defined(PRS_ASSERTIONS)
    PRS_ASSERT(PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_USED));
    PRS_ASSERT(PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_RESERVED));
    PRS_ASSERT(PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_DELETE_MARK));
    PRS_ASSERT(PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_REFCNT) == 0);
#endif

    if (entry->ops->free) {
        entry->ops->free(entry->object);
    }

    for (;;) {
        const prs_god_entry_header_t new_header = header & PRS_GOD_HEADER_ID;
        const prs_bool_t success = prs_pal_atomic_compare_exchange_weak(&entry->header, &header, new_header);
        if (success) {
            break;
        } else {
            PRS_ERROR_WHEN(!success);
        }
    }
}

/**
 * \brief
 *  Retrieves the object from the GOD.
 * \param id
 *  ID of the object to retrieve.
 * \return
 *  The object value if it was found, or \p null.
 * \note
 *  This function should not be called if the object may be destroyed by the time it is accessed.
 */
void* prs_god_find(prs_object_id_t id)
{
    PRS_RTC_IF (id == PRS_OBJECT_ID_INVALID) {
        return 0;
    }

    struct prs_god* god = prs_god_get();
    struct prs_god_entry* entry = prs_god_get_entry(god, id);

#if defined(PRS_ASSERTIONS)
    const prs_god_entry_header_t header = prs_pal_atomic_load(&entry->header);
    PRS_ASSERT(PRS_GOD_GET_ID(header) == id);
    PRS_ASSERT(PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_USED));
#endif

    return entry->object;
}

static struct prs_god_entry* prs_god_lock_entry(prs_object_id_t id)
{
    PRS_RTC_IF (id == PRS_OBJECT_ID_INVALID) {
        return 0;
    }

    struct prs_god* god = prs_god_get();
    struct prs_god_entry* entry = prs_god_get_entry(god, id);

    prs_god_entry_header_t header = prs_pal_atomic_load(&entry->header);
    prs_god_entry_header_t new_header;

    do {
        if (PRS_GOD_GET_ID(header) != id) {
            return 0;
        }
        if (!PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_USED)) {
            return 0;
        }
        if (PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_DELETE_MARK)) {
            return 0;
        }
        PRS_ERROR_IF (PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_REFCNT) == PRS_GOD_HEADER_REFCNT) {
            return 0;
        }
        new_header = header + 1;
    } while (!prs_pal_atomic_compare_exchange_weak(&entry->header, &header, new_header));

    return entry;
}

/**
 * \brief
 *  Increments the reference counter of the object referenced by the provided \p id, and returns its value.
 * \param id
 *  ID of the object to retrieve.
 * \return
 *  The object value if it was found, or \p null.
 */
void* prs_god_lock(prs_object_id_t id)
{
    struct prs_god_entry* entry = prs_god_lock_entry(id);
    if (entry) {
        return entry->object;
    } else {
        return 0;
    }
}

/**
 * \brief
 *  Decrements the reference counter of the object referenced by the provided \p id. If the counter is zero, the object
 *  is destroyed.
 * \param id
 *  ID of the object to unreference.
 */
void prs_god_unlock(prs_object_id_t id)
{
    PRS_RTC_IF (id == PRS_OBJECT_ID_INVALID) {
        return;
    }

    struct prs_god* god = prs_god_get();
    struct prs_god_entry* entry = prs_god_get_entry(god, id);

    prs_god_entry_header_t header = prs_pal_atomic_load(&entry->header);
    prs_god_entry_header_t new_header;

    do {
        PRS_RTC_IF (PRS_GOD_GET_ID(header) != id) {
            return;
        }
        PRS_RTC_IF (!PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_USED)) {
            return;
        }
        const prs_god_entry_header_t refcnt = PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_REFCNT);
        if (refcnt == 1) {
            new_header = (header | PRS_GOD_HEADER_DELETE_MARK) & ~PRS_GOD_HEADER_REFCNT;
        } else {
            PRS_RTC_IF (refcnt == 0) {
                return;
            }
            new_header = header - 1;
        }
    } while (!prs_pal_atomic_compare_exchange_weak(&entry->header, &header, new_header));

    if (PRS_GOD_TEST_FLAG(new_header, PRS_GOD_HEADER_REFCNT) == 0) {
        prs_god_free(id);
    }
}

/**
 * \brief
 *  If the reference count for the specified object is one, free the object.
 * \param id
 *  ID of the object to try to free.
 * \return
 *  \ref PRS_NOT_FOUND if the object was already freed or not found.
 *  \ref PRS_LOCKED if the reference count of the object was not one.
 *  \ref PRS_OK if the object was freed by this call.
 */
prs_result_t prs_god_try_unlock_final(prs_object_id_t id)
{
    PRS_RTC_IF (id == PRS_OBJECT_ID_INVALID) {
        return PRS_NOT_FOUND;
    }

    struct prs_god* god = prs_god_get();
    struct prs_god_entry* entry = prs_god_get_entry(god, id);

    prs_god_entry_header_t header = prs_pal_atomic_load(&entry->header);
    prs_god_entry_header_t new_header;

    do {
        PRS_RTC_IF (PRS_GOD_GET_ID(header) != id) {
            return PRS_INVALID_STATE;
        }
        PRS_RTC_IF (!PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_USED)) {
            return PRS_INVALID_STATE;
        }
        const prs_god_entry_header_t refcnt = PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_REFCNT);
        if (refcnt == 1) {
            new_header = (header | PRS_GOD_HEADER_DELETE_MARK) & ~PRS_GOD_HEADER_REFCNT;
        } else {
            PRS_RTC_IF (refcnt == 0) {
                return PRS_INVALID_STATE;
            }
            return PRS_LOCKED;
        }
    } while (!prs_pal_atomic_compare_exchange_weak(&entry->header, &header, new_header));

    PRS_ASSERT(PRS_GOD_TEST_FLAG(new_header, PRS_GOD_HEADER_REFCNT) == 0);
    prs_god_free(id);
    return PRS_OK;
}

/**
 * \brief
 *  Calls the destructor for the object.
 * \param id
 *  ID of the object to call the destructor on.
 * \note
 *  This locks the object before calling the destructor, to ensure that the object still exists. This does not directly
 *  free the object.
 */
prs_result_t prs_god_object_destroy(prs_object_id_t id)
{
    struct prs_god_entry* entry = prs_god_lock_entry(id);
    if (entry) {
        struct prs_object_ops* ops = entry->ops;
        if (ops->destroy) {
            entry->ops->destroy(entry->object);
        }
        prs_god_unlock(id);
        return PRS_OK;
    } else {
        return PRS_NOT_FOUND;
    }
}

/**
 * \brief
 *  Print information about all the objects in the GOD.
 * \param userdata
 *  Data that will be passed to \p fct as the first parameter.
 * \param fct
 *  Function to call each time characters must be printed.
 */
void prs_god_print(void* userdata, void (*fct)(void*, const char*, ...))
{
    PRS_PRECONDITION(fct);

    struct prs_god* god = prs_god_get();
    for (prs_god_index_t i = 1; i < god->max_entries; ++i) {
        struct prs_god_entry* entry = prs_god_get_entry(god, i);

        prs_god_entry_header_t header = prs_pal_atomic_load(&entry->header);
        if (!PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_USED)) {
            continue;
        }

        const prs_object_id_t id = PRS_GOD_GET_ID(entry->header);
        fct(userdata, "Entry id=%u, refcnt=%u flags=0x%08X%08X\n",
            id,
            PRS_GOD_TEST_FLAG(header, PRS_GOD_HEADER_REFCNT),
            (unsigned int)(header >> 32), (unsigned int)header);

        struct prs_god_entry* locked_entry = prs_god_lock_entry(id);
        if (locked_entry) {
            struct prs_object_ops* ops = entry->ops;
            if (ops->print) {
                ops->print(entry->object, userdata, fct);
            } else {
                fct(userdata, "-- No print callback\n");
            }
            prs_god_unlock(id);
        } else {
            fct(userdata, "-- Couldn't obtain reference\n");
        }
    }
}

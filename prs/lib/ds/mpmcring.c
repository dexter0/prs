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
 *  This file contains the definitions for a multi-producer multi-consumer ring.
 *
 *  This data structure makes use of a pool to access nodes from multiple workers.
 */

#include <stddef.h>

#include <prs/pal/atomic.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/mpmcring.h>
#include <prs/pool.h>

struct prs_mpmcring_entry {
    PRS_ATOMIC prs_pool_id_t            next;
    PRS_ATOMIC prs_bool_t               consumed;
    prs_uint8_t                         data[1];
};

struct prs_mpmcring {
    struct prs_pool*                    pool;
    prs_size_t                          data_size;

    PRS_ATOMIC prs_pool_id_t            head_id;
    PRS_ATOMIC prs_pool_id_t            tail_id;
};

static struct prs_mpmcring_entry* prs_mpmcring_data_to_entry(void* data)
{
    return (struct prs_mpmcring_entry*)((prs_uintptr_t)data - offsetof(struct prs_mpmcring_entry, data));
}

/**
 * \brief
 *  Create the ring.
 */
struct prs_mpmcring* prs_mpmcring_create(struct prs_mpmcring_create_params* params)
{
    struct prs_mpmcring* mpmcring = prs_pal_malloc_zero(sizeof(*mpmcring));
    if (!mpmcring) {
        return 0;
    }
    
    struct prs_pool_create_params pool_params = {
        .max_entries = params->max_entries,
        .data_size = params->data_size,
        .area = 0
    };
    mpmcring->pool = prs_pool_create(&pool_params);
    if (!mpmcring->pool) {
        goto cleanup;
    }

    prs_pal_atomic_store(&mpmcring->head_id, PRS_POOL_ID_INVALID);
    prs_pal_atomic_store(&mpmcring->tail_id, PRS_POOL_ID_INVALID);

    return mpmcring;

    cleanup:

    prs_pal_free(mpmcring);

    return 0;
}

/**
 * \brief
 *  Destroy the ring.
 */
void prs_mpmcring_destroy(struct prs_mpmcring* mpmcring)
{
    prs_pool_destroy(mpmcring->pool);
    prs_pal_free(mpmcring);
}

/**
 * \brief
 *  Allocate an element from the ring.
 * \param mpmcring
 *  Ring to allocate from.
 * \return
 *  The allocated element, or \p null if the ring was full.
 * \note
 *  After being allocated, the element can be immediately freed using \ref prs_mpmcring_free or pushed into the ring
 *  using \ref prs_mpmcring_push.
 */
void* prs_mpmcring_alloc(struct prs_mpmcring* mpmcring)
{
    prs_pool_id_t id;
    struct prs_mpmcring_entry* entry = prs_pool_alloc(mpmcring->pool, &id);
    if (!entry) {
        return 0;
    }
    PRS_ASSERT(id != PRS_POOL_ID_INVALID);
    prs_pal_atomic_store(&entry->consumed, PRS_FALSE);
    prs_pal_atomic_store(&entry->next, PRS_POOL_ID_INVALID);
    prs_pool_lock_first(mpmcring->pool, id);
    return entry->data;
}

/**
 * \brief
 *  Free an element to the ring.
 * \param mpmcring
 *  Ring to free to.
 * \param data
 *  Element that was obtained from \ref prs_mpmcring_alloc or \ref prs_mpmcring_pop.
 */
void prs_mpmcring_free(struct prs_mpmcring* mpmcring, void* data)
{
    PRS_PRECONDITION(mpmcring);
    PRS_PRECONDITION(data);

    struct prs_mpmcring_entry* entry = prs_mpmcring_data_to_entry(data);
    PRS_ASSERT(entry);

    /*
     * It is not guaranteed that we are the last user of this entry because the push and pop functions could still have
     * a reference to it. Therefore, we cannot verify the return value of this unlock.
     */
    prs_pool_unlock(mpmcring->pool, prs_pool_get_id(mpmcring->pool, entry));
}

/**
 * \brief
 *  Push an element in the ring.
 * \param mpmcring
 *  Ring to push into.
 * \param data
 *  Element to push.
 */
void prs_mpmcring_push(struct prs_mpmcring* mpmcring, void* data)
{
    PRS_PRECONDITION(mpmcring);
    PRS_PRECONDITION(data);

    struct prs_mpmcring_entry* entry = prs_mpmcring_data_to_entry(data);
    PRS_ASSERT(entry);

    const prs_pool_id_t id = prs_pool_get_id(mpmcring->pool, entry);
    PRS_ASSERT(id != PRS_POOL_ID_INVALID);

    prs_pool_id_t tail_id = prs_pal_atomic_load(&mpmcring->tail_id);
    struct prs_mpmcring_entry* tail_entry = 0;
    if (tail_id != PRS_POOL_ID_INVALID) {
        tail_entry = prs_pool_lock(mpmcring->pool, tail_id);
    }
    for (;;) {
        if (tail_entry) {
            prs_pool_id_t next_id = prs_pal_atomic_load(&tail_entry->next);
            struct prs_mpmcring_entry* next_entry = 0;
            if (next_id != PRS_POOL_ID_INVALID) {
                next_entry = prs_pool_lock(mpmcring->pool, next_id);
            }

            if (next_entry) {
                prs_pool_unlock(mpmcring->pool, tail_id);
                /*
                 * Try to move the tail to the next entry, since we know this tail has not been moved yet. If it fails,
                 * the tail was already moved beyond, so we can just continue looking for the tail entry.
                 */
                prs_pal_atomic_compare_exchange_strong(&mpmcring->tail_id, &tail_id, next_id);
                tail_id = next_id;
                tail_entry = next_entry;
                continue;
            }

            const prs_bool_t tail_next_set = prs_pal_atomic_compare_exchange_weak(&tail_entry->next, &next_id, id);
            if (!tail_next_set) {
                /*
                 * The tail's next id has been set by another worker. Try to acquire the tail id again, and hopefully
                 * it will point to the right location.
                 */
                prs_pool_unlock(mpmcring->pool, tail_id);
                tail_id = prs_pal_atomic_load(&mpmcring->tail_id);
                if (tail_id != PRS_POOL_ID_INVALID) {
                    tail_entry = prs_pool_lock(mpmcring->pool, tail_id);
                }
                continue;
            }

            prs_pool_unlock(mpmcring->pool, tail_id);
        }

        /*
         * Try to move the tail to the just added node since we successfully linked it to the previous tail.
         * If it fails, the tail was already moved beyond.
         */
        prs_pool_id_t prev_tail_id = tail_id;
        const prs_bool_t tail_set = prs_pal_atomic_compare_exchange_strong(&mpmcring->tail_id, &prev_tail_id, id);
        if (!tail_entry) {
            if (tail_set) {
                /* Also set the head if it wasn't set */
                prs_pal_atomic_store(&mpmcring->head_id, id);
            } else {
                /* Couldn't change the tail, which means there is a new one */
                tail_id = prs_pal_atomic_load(&mpmcring->tail_id);
                if (tail_id != PRS_POOL_ID_INVALID) {
                    tail_entry = prs_pool_lock(mpmcring->pool, tail_id);
                }
                continue;
            }
        }
        break;
    }
}

/**
 * \brief
 *  Pop an element from the ring.
 * \param mpmcring
 *  Ring to pop from.
 * \return
 *  The element that was removed from the ring, or \p null if the ring was empty.
 */
void* prs_mpmcring_pop(struct prs_mpmcring* mpmcring)
{
    prs_pool_id_t head_id = prs_pal_atomic_load(&mpmcring->head_id);
    struct prs_mpmcring_entry* head_entry = 0;
    if (head_id != PRS_POOL_ID_INVALID) {
        head_entry = prs_pool_lock(mpmcring->pool, head_id);
    }

    for (;;) {
        if (head_entry) {
            const prs_bool_t already_consumed = prs_pal_atomic_exchange(&head_entry->consumed, PRS_TRUE);

            const prs_pool_id_t next_id = prs_pal_atomic_load(&head_entry->next);
            if (next_id != PRS_POOL_ID_INVALID) {
                /* Move the head pointer to the next entry */
                prs_pool_id_t prev_head_id = head_id;
                const prs_bool_t success = PRS_BOOL(prs_pal_atomic_compare_exchange_weak(&mpmcring->head_id, &prev_head_id, next_id));
                if (success) {
                    /* We do not need the extra reference anymore */
                    prs_pool_unlock(mpmcring->pool, head_id);
                }

                if (already_consumed) {
                    prs_pool_unlock(mpmcring->pool, head_id);
                    head_id = prev_head_id;
                    head_entry = prs_pool_lock(mpmcring->pool, prev_head_id);
                    continue;
                }
            }

            if (already_consumed) {
                prs_pool_unlock(mpmcring->pool, head_id);
                return 0;
            } else {
                break;
            }
        }

        prs_pool_id_t prev_head_id = head_id;
        head_id = prs_pal_atomic_load(&mpmcring->head_id);
        if (head_id != prev_head_id) {
            head_entry = prs_pool_lock(mpmcring->pool, head_id);
            continue;
        } else {
            return 0;
        }
    }

    return head_entry->data;
}

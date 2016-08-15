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
 *  This file contains the definitions for a multi-producer multi-consumer queue.
 *
 *  This data structure makes use of a pointer directory to access nodes from multiple workers.
 */

#include <prs/pal/arch.h>
#include <prs/pal/atomic.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/mpmcq.h>
#include <prs/pd.h>

struct prs_mpmcq_node {
    PRS_ATOMIC prs_pd_id_t              next;
    PRS_ATOMIC prs_bool_t               consumed;
    prs_pd_id_t                         id;
    void*                               data;
};

struct prs_mpmcq_locked_node {
    struct prs_mpmcq_node*              node;
    prs_pd_id_t                         id;
};

struct prs_mpmcq {
    struct prs_pd*                      pd;
    prs_bool_t                          owns_pd;

    PRS_ATOMIC prs_pd_id_t              head;
    PRS_ATOMIC prs_pd_id_t              tail;
};

static struct prs_mpmcq_locked_node prs_mpmcq_alloc_node(struct prs_mpmcq* mpmcq, void* data)
{
    struct prs_mpmcq_locked_node locked_node;
    locked_node.node = prs_pal_malloc_zero(sizeof(*locked_node.node));
    if (!locked_node.node) {
        goto end;
    }

    const prs_result_t result = prs_pd_alloc_and_lock(mpmcq->pd, locked_node.node, &locked_node.id);
    if (result != PRS_OK) {
        prs_pal_free(locked_node.node);
        locked_node.node = 0;
        goto end;
    }

    locked_node.node->data = data;
    prs_pal_atomic_store(&locked_node.node->consumed, PRS_FALSE);
    prs_pal_atomic_store(&locked_node.node->next, PRS_PD_ID_INVALID);

    end:

    return locked_node;
}

static struct prs_mpmcq_locked_node prs_mpmcq_lock(struct prs_mpmcq* mpmcq, PRS_ATOMIC prs_pd_id_t* ptr)
{
    const prs_pd_id_t id = prs_pal_atomic_load(ptr);
    struct prs_mpmcq_locked_node locked_node = {
        .node = (id == PRS_PD_ID_INVALID ? 0 : prs_pd_lock(mpmcq->pd, id)),
        .id = id
    };
    return locked_node;
}

static void prs_mpmcq_unlock(struct prs_mpmcq* mpmcq, struct prs_mpmcq_locked_node* locked_node)
{
    const prs_bool_t must_free = prs_pd_unlock(mpmcq->pd, locked_node->id);
    if (must_free) {
        prs_pal_free(locked_node->node);
    }
}

/**
 * \brief
 *  Returns the size of the \ref prs_mpscq structure.
 */
prs_size_t prs_mpmcq_struct_size(void)
{
    return sizeof(struct prs_mpmcq);
}

/**
 * \brief
 *  Create the queue.
 */
struct prs_mpmcq* prs_mpmcq_create(struct prs_mpmcq_create_params* params)
{
    struct prs_mpmcq* mpmcq = params->area;
    if (!mpmcq) {
        mpmcq = prs_pal_malloc_zero(sizeof(*mpmcq));
        if (!mpmcq) {
            return 0;
        }
    }

    mpmcq->pd = params->pd;
    if (!mpmcq->pd) {
        struct prs_pd_create_params pd_params = {
            .max_entries = 4096
        };
        struct prs_pd_create_params* pd_params_ptr = params->pd_params;
        if (!params->pd_params) {
            pd_params_ptr = &pd_params;
        }
        mpmcq->pd = prs_pd_create(pd_params_ptr);
        if (!mpmcq->pd) {
            goto cleanup;
        }
        mpmcq->owns_pd = PRS_TRUE;
    }

    prs_pal_atomic_store(&mpmcq->head, PRS_PD_ID_INVALID);
    prs_pal_atomic_store(&mpmcq->tail, PRS_PD_ID_INVALID);

    return mpmcq;

    cleanup:

    if (mpmcq) {
        prs_pal_free(mpmcq);
    }

    return 0;
}

/**
 * \brief
 *  Destroy the queue.
 */
void prs_mpmcq_destroy(struct prs_mpmcq* mpmcq)
{
    /* Note: we have to assume here that the user doesn't care about the pointers stored in the queue */
    void* dummy;
    prs_result_t result = prs_mpmcq_pop(mpmcq, &dummy);
    while (result == PRS_OK) {
        result = prs_mpmcq_pop(mpmcq, &dummy);
    }

    if (mpmcq->owns_pd) {
        prs_pal_free(mpmcq->pd);
    }
    prs_pal_free(mpmcq);
}

/**
 * \brief
 *  Push an element in the queue.
 * \param mpmcq
 *  Queue to push into.
 * \param data
 *  Element to push.
 */
prs_result_t prs_mpmcq_push(struct prs_mpmcq* mpmcq, void* data)
{
    PRS_PRECONDITION(mpmcq);

    struct prs_mpmcq_locked_node new_node = prs_mpmcq_alloc_node(mpmcq, data);
    if (!new_node.node) {
        return PRS_OUT_OF_MEMORY;
    }

    struct prs_mpmcq_locked_node tail = prs_mpmcq_lock(mpmcq, &mpmcq->tail);
    for (;;) {
        prs_pd_id_t tail_id = tail.id; /* the prs_pal_atomic_compare_exchange method will fill this when it fails */
        if (tail.node) {
            struct prs_mpmcq_locked_node next = prs_mpmcq_lock(mpmcq, &tail.node->next);
            if (next.node) {
                /*
                 * Try to move the tail to the next node, since we know this tail has not been moved yet. If it fails,
                 * the tail was already moved beyond, so we can just continue looking for the last node.
                 */
                prs_pal_atomic_compare_exchange_strong(&mpmcq->tail, &tail_id, next.id);
                prs_mpmcq_unlock(mpmcq, &tail);
                tail = next;
                continue;
            }

            const prs_bool_t tail_next_set = PRS_BOOL(prs_pal_atomic_compare_exchange_strong(&tail.node->next, &next.id, new_node.id));
            if (!tail_next_set) {
                prs_mpmcq_unlock(mpmcq, &tail);
                tail = prs_mpmcq_lock(mpmcq, &mpmcq->tail);
                continue;
            }

            prs_mpmcq_unlock(mpmcq, &tail);
        }

        /*
         * Try to move the tail to the just added node since we successfully linked it to the previous tail.
         * If it fails, the tail was already moved beyond.
         */
        const prs_bool_t tail_set = PRS_BOOL(prs_pal_atomic_compare_exchange_strong(&mpmcq->tail, &tail_id, new_node.id));
        if (!tail.node) {
            if (tail_set) {
                /* Also set the head if it wasn't set */
                prs_pal_atomic_store(&mpmcq->head, new_node.id);
            } else {
                /* Couldn't change the tail, which means there is a new one */
                tail = prs_mpmcq_lock(mpmcq, &mpmcq->tail);
                continue;
            }
        }
        break;
    }

    return PRS_OK;
}

/**
 * \brief
 *  Pop an element from the queue.
 * \param mpmcq
 *  Queue to pop from.
 * \param data_ptr
 *  Pointer that receives the data that was removed from the queue.
 * \return
 *  \ref PRS_EMPTY when the queue is empty.
 *  \ref PRS_OK when an element was successfully removed.
 */
prs_result_t prs_mpmcq_pop(struct prs_mpmcq* mpmcq, void* data_ptr)
{
    PRS_PRECONDITION(mpmcq);
    PRS_PRECONDITION(data_ptr);

    struct prs_mpmcq_locked_node test_node = prs_mpmcq_lock(mpmcq, &mpmcq->head);
    for (;;) {
        if (test_node.node) {
            const prs_bool_t already_consumed = prs_pal_atomic_exchange(&test_node.node->consumed, PRS_TRUE);
            if (!already_consumed) {
                void** p = data_ptr;
                *p = test_node.node->data;
            }

            struct prs_mpmcq_locked_node next = prs_mpmcq_lock(mpmcq, &test_node.node->next);
            if (next.node) {
                /* Move the head pointer to the next entry */
                prs_pd_id_t tmp_id = test_node.id;
                const prs_bool_t success = PRS_BOOL(prs_pal_atomic_compare_exchange_strong(&mpmcq->head, &tmp_id, next.id));
                if (success) {
                    /* If the pointer was successfully moved, we do not need this node anymore, so we can free it */
                    prs_mpmcq_unlock(mpmcq, &test_node);
                }

                if (already_consumed) {
                    prs_mpmcq_unlock(mpmcq, &test_node);
                    test_node = next;
                    continue;
                }

                prs_mpmcq_unlock(mpmcq, &next);
            }

            prs_mpmcq_unlock(mpmcq, &test_node);

            if (already_consumed) {
                return PRS_EMPTY;
            } else {
                break;
            }
        }

        prs_pd_id_t prev_head_id = test_node.id;
        test_node = prs_mpmcq_lock(mpmcq, &mpmcq->head);
        if (test_node.id != prev_head_id) {
            continue;
        } else {
            PRS_ASSERT(!test_node.node);
            return PRS_UNKNOWN;
        }
    }

    return PRS_OK;
}

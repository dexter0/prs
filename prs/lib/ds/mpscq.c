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
 *  This file contains the definitions for a multi-producer single-consumer queue.
 */

#include <prs/pal/atomic.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/mpscq.h>
#include <prs/rtc.h>

/*
 * Define the following preprocessor symbol to enable loop checking
 * Note: it may be slow in some cases where nodes are inserted while reading
 */
//#define PRS_MPSCQ_LOOP_CHECK

struct prs_mpscq_node {
    struct prs_mpscq_node*              next;
    struct prs_mpscq_node*              prev;
    void*                               data;
};

struct prs_mpscq {
    struct prs_mpscq_node* PRS_ATOMIC   head;
    struct prs_mpscq_node*              reverse_tail;
    struct prs_mpscq_node*              reverse_head;
};

/**
 * \brief
 *  Create the queue.
 */
struct prs_mpscq* prs_mpscq_create(void)
{
    struct prs_mpscq* mpscq = prs_pal_malloc_zero(sizeof(*mpscq));
    if (!mpscq) {
        return 0;
    }

    return mpscq;
}

/**
 * \brief
 *  Destroy the queue.
 */
void prs_mpscq_destroy(struct prs_mpscq* mpscq)
{
    struct prs_mpscq_node* node = prs_mpscq_begin(mpscq);
    while (node) {
        prs_mpscq_remove(mpscq, node);
        node = prs_mpscq_begin(mpscq);
    }
    prs_pal_free(mpscq);
}

static void prs_mpscq_build_reverse(struct prs_mpscq* mpscq, struct prs_mpscq_node* head)
{
    if (head != mpscq->reverse_tail) {
        struct prs_mpscq_node* node = head;
        struct prs_mpscq_node* prev;
        do {
            prev = node;
            node = node->next;
            if (node) {
                node->prev = prev;
            }
        } while (node != mpscq->reverse_tail);

        if (!mpscq->reverse_head) {
            mpscq->reverse_head = prev;
        }

        mpscq->reverse_tail = head;
    }
}

#if defined(PRS_MPSCQ_LOOP_CHECK)
static prs_bool_t prs_mpscq_check(struct prs_mpscq* mpscq)
{
    struct prs_mpscq_node* node = prs_pal_atomic_load(&mpscq->head);
    struct prs_mpscq_node* base = node;
    while (node) {
        node = node->next;
        PRS_ERROR_IF (node == base) {
            return PRS_FALSE;
        }
    }

    PRS_ERROR_IF (mpscq->reverse_head && mpscq->reverse_head->next) {
        return PRS_FALSE;
    }

    PRS_ERROR_IF (mpscq->reverse_tail && mpscq->reverse_tail->prev) {
        return PRS_FALSE;
    }

    return PRS_TRUE;
}
#endif /* defined(PRS_MPSCQ_LOOP_CHECK) */

/**
 * \brief
 *  Push an element in the queue.
 * \param mpscq
 *  Queue to push into.
 * \param data
 *  Element to push.
 */
prs_result_t prs_mpscq_push(struct prs_mpscq* mpscq, void* data)
{
    PRS_PRECONDITION(mpscq);

    struct prs_mpscq_node* node = prs_pal_malloc(sizeof(*node));
    if (!node) {
        return PRS_OUT_OF_MEMORY;
    }

    node->prev = 0;
    node->data = data;

    node->next = prs_pal_atomic_load(&mpscq->head);
    while (!prs_pal_atomic_compare_exchange_weak(&mpscq->head, &node->next, node))
    {
    }

    return PRS_OK;
}

/**
 * \brief
 *  Removes the specified element from the queue.
 * \param mpscq
 *  Queue to remove from.
 * \param node
 *  Node to remove.
 */
void prs_mpscq_remove(struct prs_mpscq* mpscq, struct prs_mpscq_node* node)
{
    PRS_PRECONDITION(mpscq);
    PRS_PRECONDITION(node);

    struct prs_mpscq_node* head = prs_pal_atomic_load(&mpscq->head);
    do {
        if (node != head) {
            break;
        }
    } while (!prs_pal_atomic_compare_exchange_weak(&mpscq->head, &head, head->next));

    if (node != head) {
        /*
         * Nodes might have been added between this call to remove() and the last call to begin(). Also, nothing
         * guarantees that a call to begin will be made anyway. prev pointers are required to efficiently remove
         * elements.
         */
        prs_mpscq_build_reverse(mpscq, head);
    }

    if (node->next) {
        node->next->prev = node->prev;
    }

    if (node == mpscq->reverse_head) {
        mpscq->reverse_head = node->prev;
    }

    if (node->prev) {
        node->prev->next = node->next;
    }

    if (node == mpscq->reverse_tail) {
        mpscq->reverse_tail = node->next;
    }

    prs_pal_free(node);

#if defined(PRS_MPSCQ_LOOP_CHECK)
    prs_mpscq_check(mpscq);
#endif /* PRS_MPSCQ_LOOP_CHECK */
}

/**
 * \brief
 *  Returns the first, oldest node of the queue.
 * \param mpscq
 *  Queue to get the first node from.
 * \return
 *  The first node of the queue, or \p null if the queue is empty.
 */
struct prs_mpscq_node* prs_mpscq_begin(struct prs_mpscq* mpscq)
{
    PRS_PRECONDITION(mpscq);

    struct prs_mpscq_node* head = prs_pal_atomic_load(&mpscq->head);
    prs_mpscq_build_reverse(mpscq, head);
    return mpscq->reverse_head;
}

/**
 * \brief
 *  Returns the last, youngest node of the queue.
 * \param mpscq
 *  Queue to get the last node from.
 * \return
 *  The last node of the queue, or \p null if the queue is empty.
 */
struct prs_mpscq_node* prs_mpscq_rbegin(struct prs_mpscq* mpscq)
{
    return prs_pal_atomic_load(&mpscq->head);
}

/**
 * \brief
 *  Returns the end of the queue.
 * \param mpscq
 *  Queue to get the end from.
 * \return
 *  \p null
 */
struct prs_mpscq_node* prs_mpscq_end(struct prs_mpscq* mpscq)
{
    return 0;
}

/**
 * \brief
 *  Returns the next node in the queue.
 * \param mpscq
 *  Queue to get the next node from.
 * \param node
 *  Node to get the next from.
 * \return
 *  The node following the specified one, or \p null if the specified node was the last.
 */
struct prs_mpscq_node* prs_mpscq_next(struct prs_mpscq* mpscq, struct prs_mpscq_node* node)
{
    return node->prev;
}

/**
 * \brief
 *  Returns the previous node in the queue.
 * \param mpscq
 *  Queue to get the previous node from.
 * \param node
 *  Node to get the previous from.
 * \return
 *  The node following the specified one, or \p null if the specified node was the first.
 */
struct prs_mpscq_node* prs_mpscq_prev(struct prs_mpscq* mpscq, struct prs_mpscq_node* node)
{
    return node->next;
}

/**
 * \brief
 *  Returns the element corresponding with the specified \p node.
 * \param mpscq
 *  Queue to get the element from.
 * \param node
 *  Node to get the element from.
 */
void* prs_mpscq_get_data(struct prs_mpscq* mpscq, struct prs_mpscq_node* node)
{
    return node->data;
}

/**
 * \brief
 *  Returns if the specified node was inserted in the queue.
 */
prs_bool_t prs_mpscq_is_inserted(struct prs_mpscq* mpscq, struct prs_mpscq_node* node)
{
    prs_mpscq_foreach(mpscq, search) {
        if (search == node) {
            return PRS_TRUE;
        }
    }
    return PRS_FALSE;
}

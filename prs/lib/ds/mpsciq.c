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
 *  This file contains the definitions for an intrusive multi-producer single-consumer queue.
 */

#include <prs/pal/atomic.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/mpsciq.h>
#include <prs/rtc.h>

/*
 * Define the following preprocessor symbol to enable loop checking
 * Note: it may be slow in some cases where nodes are inserted while reading
 */
//#define PRS_MPSCIQ_LOOP_CHECK

/*
 * Define the following preprocessor symbol to enable integrity check
 */
//#define PRS_MPSCIQ_INTEGRITY_CHECK

struct prs_mpsciq {
    struct prs_mpsciq_node* PRS_ATOMIC  head;
    struct prs_mpsciq_node*             reverse_tail;
    struct prs_mpsciq_node*             reverse_head;

    prs_size_t                          node_offset;
};

/**
 * \brief
 *  Create the queue.
 */
struct prs_mpsciq* prs_mpsciq_create(struct prs_mpsciq_create_params* params)
{
    struct prs_mpsciq* mpsciq = prs_pal_malloc_zero(sizeof(*mpsciq));
    if (!mpsciq) {
        return 0;
    }

    mpsciq->node_offset = params->node_offset;

    return mpsciq;
}

/**
 * \brief
 *  Destroy the queue.
 */
void prs_mpsciq_destroy(struct prs_mpsciq* mpsciq)
{
    /* Note: we cannot free the entries in the list, we can only assume that they have been taken care of by the user */
    prs_pal_free(mpsciq);
}

#if defined(PRS_MPSCIQ_INTEGRITY_CHECK)
static prs_bool_t prs_mpsciq_check(struct prs_mpsciq* mpsciq)
{
    struct prs_mpsciq_node* node = prs_pal_atomic_load(&mpsciq->head);
    struct prs_mpsciq_node* prev = 0;
    prs_bool_t reverse_tail_found = PRS_FALSE;
    while (node) {
        if (reverse_tail_found) {
            PRS_ASSERT(node->prev == prev);
        } else {
            if (node == mpsciq->reverse_tail) {
                reverse_tail_found = PRS_TRUE;
            }
        }
        prev = node;
        node = node->next;
    }

    PRS_ASSERT(!!mpsciq->reverse_tail == !!mpsciq->reverse_head);
    PRS_ASSERT(!mpsciq->reverse_head || mpsciq->reverse_head == prev);
    PRS_ASSERT(prs_pal_atomic_load(&mpsciq->head) || !mpsciq->reverse_tail);
    PRS_ASSERT(prs_pal_atomic_load(&mpsciq->head) || !mpsciq->reverse_head);

    return PRS_TRUE;
}
#endif /* defined(PRS_MPSCIQ_INTEGRITY_CHECK) */

#if defined(PRS_MPSCIQ_LOOP_CHECK)
static prs_bool_t prs_mpsciq_loop_check(struct prs_mpsciq* mpsciq)
{
    struct prs_mpsciq_node* node = prs_pal_atomic_load(&mpsciq->head);
    struct prs_mpsciq_node* base = node;
    while (node) {
        node = node->next;
        PRS_ERROR_IF (node == base) {
            return PRS_FALSE;
        }
    }

    PRS_ERROR_IF (mpsciq->reverse_head && prs_pal_atomic_load(&mpsciq->reverse_head->next)) {
        return PRS_FALSE;
    }

    PRS_ERROR_IF (mpsciq->reverse_tail && mpsciq->reverse_tail->prev) {
        return PRS_FALSE;
    }

    return PRS_TRUE;
}
#endif /* defined(PRS_MPSCIQ_LOOP_CHECK) */

static void prs_mpsciq_build_reverse(struct prs_mpsciq* mpsciq, struct prs_mpsciq_node* head)
{
#if defined(PRS_MPSCIQ_INTEGRITY_CHECK)
    prs_mpsciq_check(mpsciq);
#endif /* PRS_MPSCIQ_INTEGRITY_CHECK */

    if (head != mpsciq->reverse_tail) {
        struct prs_mpsciq_node* node = head;
        struct prs_mpsciq_node* prev;
        do {
            prev = node;
            node = node->next;
            if (node) {
                node->prev = prev;
            }
        } while (node != mpsciq->reverse_tail);

        if (!mpsciq->reverse_head) {
            mpsciq->reverse_head = prev;
        }

        mpsciq->reverse_tail = head;
    }
}

/**
 * \brief
 *  Push an element in the queue.
 * \param mpsciq
 *  Queue to push into.
 * \param node
 *  Node to push.
 */
void prs_mpsciq_push(struct prs_mpsciq* mpsciq, struct prs_mpsciq_node* node)
{
    PRS_PRECONDITION(mpsciq);
    PRS_PRECONDITION(node);

    PRS_RTC_IF (node->next) {
        return;
    }
    PRS_RTC_IF (node->prev) {
        return;
    }

#if defined(DEBUG)
    PRS_RTC_IF (node->mpsciq) {
        return;
    }
    node->mpsciq = mpsciq;
#endif /* defined(DEBUG) */

    node->next = prs_pal_atomic_load(&mpsciq->head);
    while (!prs_pal_atomic_compare_exchange_weak(&mpsciq->head, &node->next, node)) {
    }
}

/**
 * \brief
 *  Removes the specified element from the queue.
 * \param mpsciq
 *  Queue to remove from.
 * \param node
 *  Node to remove.
 */
void prs_mpsciq_remove(struct prs_mpsciq* mpsciq, struct prs_mpsciq_node* node)
{
    PRS_PRECONDITION(mpsciq);
    PRS_PRECONDITION(node);

#if defined(DEBUG)
    PRS_RTC_IF (node->mpsciq != mpsciq) {
        return;
    }
    node->mpsciq = 0;
#endif /* defined(DEBUG) */

#if defined(PRS_MPSCIQ_INTEGRITY_CHECK)
    prs_mpsciq_check(mpsciq);
#endif /* PRS_MPSCIQ_INTEGRITY_CHECK */

    /* If the node is the head, try to move the head further. If it fails, the head was already moved. */
    struct prs_mpsciq_node* head = prs_pal_atomic_load(&mpsciq->head);
    if (node == head) {
        prs_pal_atomic_compare_exchange_strong(&mpsciq->head, &head, head->next);
    }

    if (node != head) {
        /*
         * Nodes might have been added between this call to remove() and the last call to begin(). Also, nothing
         * guarantees that a call to begin will be made anyway. prev pointers are required to efficiently remove
         * elements.
         */
        prs_mpsciq_build_reverse(mpsciq, head);
    }

    if (node->next) {
        node->next->prev = node->prev;
    }

    if (node == mpsciq->reverse_head) {
        mpsciq->reverse_head = node->prev;
    }

    if (node->prev) {
        prs_pal_atomic_store(&node->prev->next, node->next);
    }

    if (node == mpsciq->reverse_tail) {
        mpsciq->reverse_tail = node->next;
    }

    node->next = 0;
    node->prev = 0;

#if defined(PRS_MPSCIQ_LOOP_CHECK)
    prs_mpsciq_loop_check(mpsciq);
#endif /* PRS_MPSCIQ_LOOP_CHECK */

#if defined(PRS_MPSCIQ_INTEGRITY_CHECK)
    prs_mpsciq_check(mpsciq);
#endif /* PRS_MPSCIQ_INTEGRITY_CHECK */
}

/**
 * \brief
 *  Returns the first, oldest node of the queue.
 * \param mpsciq
 *  Queue to get the first node from.
 * \return
 *  The first node of the queue, or \p null if the queue is empty.
 */
struct prs_mpsciq_node* prs_mpsciq_begin(struct prs_mpsciq* mpsciq)
{
    PRS_PRECONDITION(mpsciq);

    struct prs_mpsciq_node* head = prs_pal_atomic_load(&mpsciq->head);
    prs_mpsciq_build_reverse(mpsciq, head);
    return mpsciq->reverse_head;
}

/**
 * \brief
 *  Returns the last, youngest node of the queue.
 * \param mpsciq
 *  Queue to get the last node from.
 * \return
 *  The last node of the queue, or \p null if the queue is empty.
 */
struct prs_mpsciq_node* prs_mpsciq_rbegin(struct prs_mpsciq* mpsciq)
{
    return prs_pal_atomic_load(&mpsciq->head);
}

/**
 * \brief
 *  Returns the end of the queue.
 * \param mpsciq
 *  Queue to get the end from.
 * \return
 *  \p null
 */
struct prs_mpsciq_node* prs_mpsciq_end(struct prs_mpsciq* mpsciq)
{
    return 0;
}

/**
 * \brief
 *  Returns the next node in the queue.
 * \param mpsciq
 *  Queue to get the next node from.
 * \param node
 *  Node to get the next from.
 * \return
 *  The node following the specified one, or \p null if the specified node was the last.
 */
struct prs_mpsciq_node* prs_mpsciq_next(struct prs_mpsciq* mpsciq, struct prs_mpsciq_node* node)
{
    return node->prev;
}

/**
 * \brief
 *  Returns the previous node in the queue.
 * \param mpsciq
 *  Queue to get the previous node from.
 * \param node
 *  Node to get the previous from.
 * \return
 *  The node following the specified one, or \p null if the specified node was the first.
 */
struct prs_mpsciq_node* prs_mpsciq_prev(struct prs_mpsciq* mpsciq, struct prs_mpsciq_node* node)
{
    return node->next;
}

/**
 * \brief
 *  Returns the element corresponding with the specified \p node.
 * \param mpsciq
 *  Queue to get the element from.
 * \param node
 *  Node to get the element from.
 */
void* prs_mpsciq_get_data(struct prs_mpsciq* mpsciq, struct prs_mpsciq_node* node)
{
    return (char*)node - mpsciq->node_offset;
}

/**
 * \brief
 *  Returns if the specified node was inserted in the queue.
 */
prs_bool_t prs_mpsciq_is_inserted(struct prs_mpsciq* mpsciq, struct prs_mpsciq_node* node)
{
    prs_mpsciq_foreach(mpsciq, search) {
        if (search == node) {
            return PRS_TRUE;
        }
    }
    return PRS_FALSE;
}

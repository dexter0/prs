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
 *  This file contains the definitions for an doubly linked list.
 */

#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/dllist.h>
#include <prs/rtc.h>

struct prs_dllist_node {
    struct prs_dllist_node*             next;
    struct prs_dllist_node*             prev;
    void*                               data;
};

struct prs_dllist {
    struct prs_dllist_node*             head;
    struct prs_dllist_node*             tail;
    prs_size_t                          size;
};

/**
 * \brief
 *  Create the list.
 */
struct prs_dllist* prs_dllist_create(void)
{
    struct prs_dllist* dllist = prs_pal_malloc_zero(sizeof(*dllist));
    if (!dllist) {
        return 0;
    }

    return dllist;
}

/**
 * \brief
 *  Destroy the list.
 */
void prs_dllist_destroy(struct prs_dllist* dllist)
{
    struct prs_dllist_node* node;
    while ((node = prs_dllist_begin(dllist)) != 0) {
        prs_dllist_remove(dllist, node);
    }
    prs_pal_free(dllist);
}

/**
 * \brief
 *  Insert an element in the list, before another.
 * \param dllist
 *  List to insert into.
 * \param before
 *  Node before which the inserted element should be added. If \p null, the element is added at the beginning of the
 *  list.
 * \param data
 *  Element to insert.
 */
prs_result_t prs_dllist_insert_before(struct prs_dllist* dllist, struct prs_dllist_node* before, void* data)
{
    PRS_PRECONDITION(dllist);

    struct prs_dllist_node* node = prs_pal_malloc(sizeof(*node));
    if (!node) {
        return PRS_OUT_OF_MEMORY;
    }

    node->data = data;

    struct prs_dllist_node* next = before;
    struct prs_dllist_node* prev = (before ? before->prev : dllist->tail);

    node->next = next;
    node->prev = prev;

    if (next) {
        next->prev = node;
    }
    if (prev) {
        prev->next = node;
    }

    if (next == dllist->head) {
        dllist->head = node;
    }
    if (prev == dllist->tail) {
        dllist->tail = node;
    }

    ++dllist->size;

    return PRS_OK;
}

/**
 * \brief
 *  Removes the specified element from the list.
 * \param dllist
 *  List to remove from.
 * \param node
 *  Node to remove.
 */
void prs_dllist_remove(struct prs_dllist* dllist, struct prs_dllist_node* node)
{
    PRS_PRECONDITION(dllist);
    PRS_PRECONDITION(node);

    struct prs_dllist_node* next = node->next;
    struct prs_dllist_node* prev = node->prev;

    if (next) {
        next->prev = prev;
    }
    if (prev) {
        prev->next = next;
    }

    if (node == dllist->head) {
        dllist->head = next;
    }
    if (node == dllist->tail) {
        dllist->tail = prev;
    }

    --dllist->size;

    prs_pal_free(node);
}

/**
 * \brief
 *  Returns the first node of the list.
 * \param dllist
 *  List to get the first node from.
 * \return
 *  The first node of the list, or \p null if the list is empty.
 */
struct prs_dllist_node* prs_dllist_begin(struct prs_dllist* dllist)
{
    PRS_PRECONDITION(dllist);
    return dllist->head;
}

/**
 * \brief
 *  Returns the end of the list.
 * \param dllist
 *  List to get the end from.
 * \return
 *  \p null
 */
struct prs_dllist_node* prs_dllist_end(struct prs_dllist* dllist)
{
    return 0;
}

/**
 * \brief
 *  Returns the next node in the list.
 * \param dllist
 *  List to get the next node from.
 * \param node
 *  Node to get the next from.
 * \return
 *  The node following the specified one, or \p null if the specified node was the last.
 */
struct prs_dllist_node* prs_dllist_next(struct prs_dllist* dllist, struct prs_dllist_node* node)
{
    return node->next;
}

/**
 * \brief
 *  Returns the element specified in \ref prs_dllist_insert_before corresponding with the specified \p node.
 * \param dllist
 *  List to get the element from.
 * \param node
 *  Node to get the element from.
 */
void* prs_dllist_get_data(struct prs_dllist* dllist, struct prs_dllist_node* node)
{
    return node->data;
}

/**
 * \brief
 *  Returns if the list is empty.
 */
prs_bool_t prs_dllist_empty(struct prs_dllist* dllist)
{
    return dllist->size == 0 ? PRS_TRUE : PRS_FALSE;
}

/**
 * \brief
 *  Returns the size of the list.
 */
prs_size_t prs_dllist_size(struct prs_dllist* dllist)
{
    return dllist->size;
}

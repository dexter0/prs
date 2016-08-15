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
 *  This file contains the definitions for an intrusive doubly linked list.
 */

#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/idllist.h>
#include <prs/rtc.h>

struct prs_idllist {
    struct prs_idllist_node*            head;
    struct prs_idllist_node*            tail;

    prs_size_t                          node_offset;

    prs_size_t                          size;
};

/**
 * \brief
 *  Create the list.
 */
struct prs_idllist* prs_idllist_create(struct prs_idllist_create_params* params)
{
    struct prs_idllist* idllist = prs_pal_malloc_zero(sizeof(*idllist));
    if (!idllist) {
        return 0;
    }

    idllist->node_offset = params->node_offset;

    return idllist;
}

/**
 * \brief
 *  Destroy the list.
 */
void prs_idllist_destroy(struct prs_idllist* idllist)
{
    struct prs_idllist_node* node;
    while ((node = prs_idllist_begin(idllist)) != 0) {
        prs_idllist_remove(idllist, node);
    }
    prs_pal_free(idllist);
}

/**
 * \brief
 *  Insert an element in the list, before another.
 * \param idllist
 *  List to insert into.
 * \param before
 *  Node before which the inserted element should be added. If \p null, the element is added at the beginning of the
 *  list.
 * \param node
 *  Node to insert.
 */
void prs_idllist_insert_before(struct prs_idllist* idllist, struct prs_idllist_node* before, struct prs_idllist_node* node)
{
    PRS_PRECONDITION(idllist);
    PRS_PRECONDITION(node);

    PRS_RTC_IF (node->next) {
        return;
    }
    PRS_RTC_IF (node->prev) {
        return;
    }
#if defined(DEBUG)
    PRS_RTC_IF (before && before->list != idllist) {
        return;
    }
    PRS_RTC_IF (node->list) {
        return;
    }
#endif /* DEBUG */

    struct prs_idllist_node* next = before;
    struct prs_idllist_node* prev = (before ? before->prev : idllist->tail);

    node->next = next;
    node->prev = prev;
#if defined(DEBUG)
    node->list = idllist;
#endif /* DEBUG */

    if (next) {
        next->prev = node;
    }
    if (prev) {
        prev->next = node;
    }

    if (next == idllist->head) {
        idllist->head = node;
    }
    if (prev == idllist->tail) {
        idllist->tail = node;
    }

    ++idllist->size;

    PRS_POSTCONDITION(idllist->head && idllist->tail);
}

/**
 * \brief
 *  Removes the specified element from the list.
 * \param idllist
 *  List to remove from.
 * \param node
 *  Node to remove.
 */
void prs_idllist_remove(struct prs_idllist* idllist, struct prs_idllist_node* node)
{
    PRS_PRECONDITION(idllist);
    PRS_PRECONDITION(node);

    struct prs_idllist_node* next = node->next;
    struct prs_idllist_node* prev = node->prev;

#if defined(DEBUG)
    PRS_RTC_IF (!node->list) {
        return;
    }
    PRS_RTC_IF (node->list != idllist) {
        return;
    }
#endif /* DEBUG */

    if (next) {
        next->prev = prev;
    }
    if (prev) {
        prev->next = next;
    }

    if (node == idllist->head) {
        idllist->head = next;
    }
    if (node == idllist->tail) {
        idllist->tail = prev;
    }

    node->next = 0;
    node->prev = 0;
#if defined(DEBUG)
    node->list = 0;
#endif /* DEBUG */

    --idllist->size;

    PRS_POSTCONDITION(idllist->size != 0 || (!idllist->head && !idllist->tail));
    PRS_POSTCONDITION(idllist->size == 0 || (idllist->head && idllist->tail));
}

/**
 * \brief
 *  Returns the first node of the list.
 * \param idllist
 *  List to get the first node from.
 * \return
 *  The first node of the list, or \p null if the list is empty.
 */
struct prs_idllist_node* prs_idllist_begin(struct prs_idllist* idllist)
{
    PRS_PRECONDITION(idllist);
    return idllist->head;
}

/**
 * \brief
 *  Returns the end of the list.
 * \param idllist
 *  List to get the end from.
 * \return
 *  \p null
 */
struct prs_idllist_node* prs_idllist_end(struct prs_idllist* idllist)
{
    return 0;
}

/**
 * \brief
 *  Returns the next node in the list.
 * \param idllist
 *  List to get the next node from.
 * \param node
 *  Node to get the next from.
 * \return
 *  The node following the specified one, or \p null if the specified node was the last.
 */
struct prs_idllist_node* prs_idllist_next(struct prs_idllist* idllist, struct prs_idllist_node* node)
{
    return node->next;
}

/**
 * \brief
 *  Returns the element specified in \ref prs_idllist_insert_before corresponding with the specified \p node.
 * \param idllist
 *  List to get the element from.
 * \param node
 *  Node to get the element from.
 */
void* prs_idllist_get_data(struct prs_idllist* idllist, struct prs_idllist_node* node)
{
    return (char*)node - idllist->node_offset;
}

/**
 * \brief
 *  Returns if the list is empty.
 */
prs_bool_t prs_idllist_empty(struct prs_idllist* idllist)
{
    return idllist->size == 0 ? PRS_TRUE : PRS_FALSE;
}

/**
 * \brief
 *  Returns the size of the list.
 */
prs_size_t prs_idllist_size(struct prs_idllist* idllist)
{
    return idllist->size;
}

/**
 * \brief
 *  Returns if the specified node was inserted in the list.
 */
prs_bool_t prs_idllist_is_inserted(struct prs_idllist* idllist, struct prs_idllist_node* node)
{
    prs_idllist_foreach(idllist, search) {
        if (search == node) {
            return PRS_TRUE;
        }
    }
    return PRS_FALSE;
}

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
 *  This file contains the declarations for an intrusive doubly linked list.
 */

#ifndef _PRS_IDLLIST_H
#define _PRS_IDLLIST_H

#include <prs/result.h>
#include <prs/types.h>

struct prs_idllist;

struct prs_idllist_node {
    struct prs_idllist_node*            next;
    struct prs_idllist_node*            prev;

#if defined(DEBUG)
    struct prs_idllist*                 list;
#endif /* DEBUG */
};

struct prs_idllist_create_params {
    prs_size_t                          node_offset;
};

struct prs_idllist* prs_idllist_create(struct prs_idllist_create_params* params);
void prs_idllist_destroy(struct prs_idllist* idllist);

void prs_idllist_insert_before(struct prs_idllist* idllist, struct prs_idllist_node* before, struct prs_idllist_node* node);
void prs_idllist_remove(struct prs_idllist* idllist, struct prs_idllist_node* node);

struct prs_idllist_node* prs_idllist_begin(struct prs_idllist* idllist);
struct prs_idllist_node* prs_idllist_end(struct prs_idllist* idllist);
struct prs_idllist_node* prs_idllist_next(struct prs_idllist* idllist, struct prs_idllist_node* node);
void* prs_idllist_get_data(struct prs_idllist* idllist, struct prs_idllist_node* node);
prs_bool_t prs_idllist_is_inserted(struct prs_idllist* idllist, struct prs_idllist_node* node);

prs_bool_t prs_idllist_empty(struct prs_idllist* idllist);
prs_size_t prs_idllist_size(struct prs_idllist* idllist);

#define prs_idllist_foreach(idllist, node) \
    for(struct prs_idllist_node* node = prs_idllist_begin(idllist); \
        node != prs_idllist_end(idllist); \
        node = prs_idllist_next(idllist, node))

#endif /* _PRS_IDLLIST_H */

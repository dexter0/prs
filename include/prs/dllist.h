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
 *  This file contains the declarations for a doubly linked list.
 */

#ifndef _PRS_DLLIST_H
#define _PRS_DLLIST_H

#include <prs/types.h>
#include <prs/result.h>

struct prs_dllist;
struct prs_dllist_node;

struct prs_dllist* prs_dllist_create(void);
void prs_dllist_destroy(struct prs_dllist* dllist);

prs_result_t prs_dllist_insert_before(struct prs_dllist* dllist, struct prs_dllist_node* before, void* data);
void prs_dllist_remove(struct prs_dllist* dllist, struct prs_dllist_node* node);

struct prs_dllist_node* prs_dllist_begin(struct prs_dllist* dllist);
struct prs_dllist_node* prs_dllist_end(struct prs_dllist* dllist);
struct prs_dllist_node* prs_dllist_next(struct prs_dllist* dllist, struct prs_dllist_node* node);
void* prs_dllist_get_data(struct prs_dllist* dllist, struct prs_dllist_node* node);

prs_bool_t prs_dllist_empty(struct prs_dllist* dllist);
prs_size_t prs_dllist_size(struct prs_dllist* dllist);

#define prs_dllist_foreach(dllist, node) \
    for(struct prs_dllist_node* node = prs_dllist_begin(dllist); \
        node != prs_dllist_end(dllist); \
        node = prs_dllist_next(dllist, node))

#endif /* _PRS_DLLIST_H */

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
 *  This file contains the declarations for a multi-producer single-consumer intrusive queue.
 */

#ifndef _PRS_MPSCIQ_H
#define _PRS_MPSCIQ_H

#include <prs/types.h>
#include <prs/result.h>

struct prs_mpsciq;

struct prs_mpsciq_node {
#if defined(DEBUG)
    struct prs_mpsciq*                  mpsciq;
#endif /* defined(DEBUG) */
    struct prs_mpsciq_node*             next;
    struct prs_mpsciq_node*             prev;
};

struct prs_mpsciq_create_params {
    prs_size_t                          node_offset;
};

struct prs_mpsciq* prs_mpsciq_create(struct prs_mpsciq_create_params* params);
void prs_mpsciq_destroy(struct prs_mpsciq* mpsciq);

void prs_mpsciq_push(struct prs_mpsciq* mpsciq, struct prs_mpsciq_node* node);
void prs_mpsciq_remove(struct prs_mpsciq* mpsciq, struct prs_mpsciq_node* node);

struct prs_mpsciq_node* prs_mpsciq_begin(struct prs_mpsciq* mpsciq);
struct prs_mpsciq_node* prs_mpsciq_rbegin(struct prs_mpsciq* mpsciq);
struct prs_mpsciq_node* prs_mpsciq_end(struct prs_mpsciq* mpsciq);
struct prs_mpsciq_node* prs_mpsciq_next(struct prs_mpsciq* mpsciq, struct prs_mpsciq_node* node);
struct prs_mpsciq_node* prs_mpsciq_prev(struct prs_mpsciq* mpscq, struct prs_mpsciq_node* node);
void* prs_mpsciq_get_data(struct prs_mpsciq* mpsciq, struct prs_mpsciq_node* node);
prs_bool_t prs_mpsciq_is_inserted(struct prs_mpsciq* mpsciq, struct prs_mpsciq_node* node);

#define prs_mpsciq_foreach(mpsciq, node) \
    for(struct prs_mpsciq_node* node = prs_mpsciq_begin(mpsciq); \
        node != prs_mpsciq_end(mpsciq); \
        node = prs_mpsciq_next(mpsciq, node))

#define prs_mpsciq_rforeach(mpsciq, node) \
    for(struct prs_mpsciq_node* node = prs_mpsciq_rbegin(mpsciq); \
        node != prs_mpsciq_end(mpsciq); \
        node = prs_mpsciq_prev(mpsciq, node))

#endif /* _PRS_MPSCIQ_H */

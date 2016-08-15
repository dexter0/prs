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
 *  This file contains the declarations for a multi-producer single-consumer queue.
 */

#ifndef _PRS_MPSCQ_H
#define _PRS_MPSCQ_H

#include <prs/types.h>
#include <prs/result.h>

struct prs_mpscq;
struct prs_mpscq_node;

struct prs_mpscq* prs_mpscq_create(void);
void prs_mpscq_destroy(struct prs_mpscq* mpscq);

prs_result_t prs_mpscq_push(struct prs_mpscq* mpscq, void* data);
void prs_mpscq_remove(struct prs_mpscq* mpscq, struct prs_mpscq_node* node);

struct prs_mpscq_node* prs_mpscq_begin(struct prs_mpscq* mpscq);
struct prs_mpscq_node* prs_mpscq_rbegin(struct prs_mpscq* mpscq);
struct prs_mpscq_node* prs_mpscq_end(struct prs_mpscq* mpscq);
struct prs_mpscq_node* prs_mpscq_next(struct prs_mpscq* mpscq, struct prs_mpscq_node* node);
struct prs_mpscq_node* prs_mpscq_prev(struct prs_mpscq* mpscq, struct prs_mpscq_node* node);
void* prs_mpscq_get_data(struct prs_mpscq* mpscq, struct prs_mpscq_node* node);

#define prs_mpscq_foreach(mpscq, node) \
    for(struct prs_mpscq_node* node = prs_mpscq_begin(mpscq); \
        node != prs_mpscq_end(mpscq); \
        node = prs_mpscq_next(mpscq, node))

#define prs_mpscq_rforeach(mpscq, node) \
    for(struct prs_mpscq_node* node = prs_mpscq_rbegin(mpscq); \
        node != prs_mpscq_end(mpscq); \
        node = prs_mpscq_prev(mpscq, node))

#endif /* _PRS_MPSCQ_H */

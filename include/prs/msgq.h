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
 *  This file contains the message queue declarations.
 */

#ifndef _PRS_MSGQ_H
#define _PRS_MSGQ_H

#include <prs/object.h>
#include <prs/result.h>
#include <prs/ticks.h>
#include <prs/types.h>

struct prs_msgq_filter;
struct prs_msgq;
struct prs_msg;

/**
 * \brief
 *  Message queue creation parameters.
 */
struct prs_msgq_create_params {
    /**
     * \brief
     *  Pointer directory to use for assigning filters to the message queue. When \p null, the global pointer directory
     *  (GPD) is used.
     */
    struct prs_pd*                      pd;
};

/**
 * \brief
 *  Defines a message queue filter function. The filter function returns \p true when the specified message matches
 *  the filter.
 */
typedef prs_bool_t (*prs_msgq_filter_function_t)(void* userdata, struct prs_msg* msg);

struct prs_msgq* prs_msgq_create(struct prs_msgq_create_params* params);
void prs_msgq_destroy(struct prs_msgq* msgq);

void prs_msgq_send(struct prs_msgq* msgq, struct prs_msg* msg);

struct prs_msg* prs_msgq_recv(struct prs_msgq* msgq);
struct prs_msg* prs_msgq_recv_timeout(struct prs_msgq* msgq, prs_ticks_t timeout);
struct prs_msg* prs_msgq_recv_filter(struct prs_msgq* msgq, void* userdata, prs_size_t userdata_size,
    prs_msgq_filter_function_t function);
struct prs_msg* prs_msgq_recv_filter_timeout(struct prs_msgq* msgq, void* userdata, prs_size_t userdata_size,
    prs_msgq_filter_function_t function, prs_ticks_t timeout);

#endif /* _PRS_MSGQ_H */

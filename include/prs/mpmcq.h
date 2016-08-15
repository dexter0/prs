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
 *  This file contains the declarations for a multi-producer multi-consumer queue.
 */

#ifndef _PRS_MPMCQ_H
#define _PRS_MPMCQ_H

#include <prs/pd.h>
#include <prs/types.h>
#include <prs/result.h>

struct prs_mpmcq;
struct prs_mpmcq_node;

struct prs_mpmcq_create_params {
    void*                               area;
    struct prs_pd*                      pd;
    struct prs_pd_create_params*        pd_params;
};

prs_size_t prs_mpmcq_struct_size(void);

struct prs_mpmcq* prs_mpmcq_create(struct prs_mpmcq_create_params* params);
void prs_mpmcq_destroy(struct prs_mpmcq* mpmcq);

prs_result_t prs_mpmcq_push(struct prs_mpmcq* mpmcq, void* data);
prs_result_t prs_mpmcq_pop(struct prs_mpmcq* mpmcq, void* data_ptr);

#endif /* _PRS_MPMCQ_H */

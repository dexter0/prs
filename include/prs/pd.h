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
 *  This file contains the declarations for a pointer directory.
 */

#ifndef _PRS_PD_H
#define _PRS_PD_H

#include <prs/types.h>
#include <prs/result.h>

#define PRS_PD_ID_INVALID               ((prs_pd_id_t)-1)

typedef prs_uint_t prs_pd_id_t;

struct prs_pd_create_params {
    prs_size_t                          max_entries;
    void*                               area;
};

prs_size_t prs_pd_struct_size(struct prs_pd_create_params* params);

struct prs_pd* prs_pd_create(struct prs_pd_create_params* params);
void prs_pd_destroy(struct prs_pd* pd);

prs_result_t prs_pd_alloc_and_lock(struct prs_pd* pd, void* ptr, prs_pd_id_t* id);

void* prs_pd_lock(struct prs_pd* pd, prs_pd_id_t id);
prs_bool_t prs_pd_unlock(struct prs_pd* pd, prs_pd_id_t id);

#endif /* _PRS_PD_H */

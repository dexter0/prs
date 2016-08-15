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
 *  This file contains the declarations for a multi-producer multi-consumer ring.
 */

#ifndef _PRS_MPMCRING_H
#define _PRS_MPMCRING_H

#include <prs/types.h>
#include <prs/result.h>

struct prs_mpmcring;

struct prs_mpmcring_create_params {
    prs_size_t                          data_size;
    prs_size_t                          max_entries;
};

struct prs_mpmcring* prs_mpmcring_create(struct prs_mpmcring_create_params* params);
void prs_mpmcring_destroy(struct prs_mpmcring* mpmcring);

void* prs_mpmcring_alloc(struct prs_mpmcring* mpmcring);
void prs_mpmcring_free(struct prs_mpmcring* mpmcring, void* data);

void prs_mpmcring_push(struct prs_mpmcring* mpmcring, void* data);
void* prs_mpmcring_pop(struct prs_mpmcring* mpmcring);

#endif /* _PRS_MPMCRING_H */

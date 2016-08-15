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
 *  This file contains the pool declarations.
 */

#ifndef _PRS_POOL_H
#define _PRS_POOL_H

#include <prs/result.h>
#include <prs/types.h>

/**
 * \brief
 *  Invalid pool element ID.
 */
#define PRS_POOL_ID_INVALID             ((prs_pool_id_t)-1)

/** \brief Pool element ID type. */
typedef prs_uint_t prs_pool_id_t;

/**
 * \brief
 *  Pool creation parameters.
 */
struct prs_pool_create_params {
    /** \brief Maximum number of elements in the pool. */
    prs_size_t                          max_entries;
    /** \brief Size of an element. */
    prs_size_t                          data_size;
    /**
     * \brief
     *  Pre-allocated memory area that the pool can use. It must be as large as \ref prs_pool_struct_size returns.
     */
    void*                               area;
};

prs_size_t prs_pool_struct_size(struct prs_pool_create_params* params);

struct prs_pool* prs_pool_create(struct prs_pool_create_params* params);
void prs_pool_destroy(struct prs_pool* pool);

void* prs_pool_alloc(struct prs_pool* pool, prs_pool_id_t* id);
void prs_pool_free(struct prs_pool* pool, prs_pool_id_t id);

void prs_pool_lock_first(struct prs_pool* pool, prs_pool_id_t id);

void* prs_pool_alloc_and_lock(struct prs_pool* pool, void* data, prs_pool_id_t* id);
void* prs_pool_lock(struct prs_pool* pool, prs_pool_id_t id);

prs_bool_t prs_pool_unlock(struct prs_pool* pool, prs_pool_id_t id);
prs_bool_t prs_pool_try_unlock_final(struct prs_pool* pool, prs_pool_id_t id);
prs_bool_t prs_pool_unlock_dest(struct prs_pool* pool, prs_pool_id_t id, void (*destructor)(void* userdata, void* data), void* userdata);
prs_bool_t prs_pool_try_unlock_final_dest(struct prs_pool* pool, prs_pool_id_t id, void (*destructor)(void* userdata, void* data), void* userdata);

prs_pool_id_t prs_pool_get_id(struct prs_pool* pool, void* data);

#endif /* _PRS_POOL_H */

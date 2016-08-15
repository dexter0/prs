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
 *  This file contains the global object directory declarations.
 */

#ifndef _PRS_GOD_H
#define _PRS_GOD_H

#include <prs/object.h>
#include <prs/result.h>
#include <prs/types.h>

/**
 * \brief
 *  Global object directory creation parameters.
 */
struct prs_god_create_params {
    /** \brief Maximum simultaneous number of entries in the global object directory. */
    prs_size_t                          max_entries;
};

prs_result_t prs_god_create(struct prs_god_create_params* params);
void prs_god_destroy(void);

prs_object_id_t prs_god_alloc_and_lock(void* object, struct prs_object_ops* ops);

void* prs_god_find(prs_object_id_t id);

void* prs_god_lock(prs_object_id_t id);
void prs_god_unlock(prs_object_id_t id);
prs_result_t prs_god_try_unlock_final(prs_object_id_t id);

prs_result_t prs_god_object_destroy(prs_object_id_t id);

void prs_god_print(void* userdata, void (*fct)(void*, const char*, ...));

#endif /* _PRS_GOD_H */

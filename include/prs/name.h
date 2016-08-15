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
 *  This file contains the name resolver declarations.
 */

#ifndef _PRS_NAME_H
#define _PRS_NAME_H

#include <prs/object.h>
#include <prs/result.h>
#include <prs/types.h>

struct prs_name;

/**
 * \brief
 *  Name resolver creation parameters.
 */
struct prs_name_create_params {
    /** \brief Maximum number of entries in the name resolver's hash table. */
    prs_size_t                          max_entries;
    /** \brief Offset of the name of the object relative to its object data structure. */
    prs_size_t                          string_offset;
};

struct prs_name* prs_name_create(struct prs_name_create_params* params);
void prs_name_destroy(struct prs_name* name);

prs_result_t prs_name_alloc(struct prs_name* name, prs_object_id_t id);
prs_result_t prs_name_free(struct prs_name* name, prs_object_id_t id);
prs_object_id_t prs_name_find(struct prs_name* name, const char* key);
prs_object_id_t prs_name_find_and_lock(struct prs_name* name, const char* key);

#endif /* _PRS_NAME_H */

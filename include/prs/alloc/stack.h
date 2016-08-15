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
 *  This file contains the stack allocator declarations.
 */

#ifndef _PRS_ALLOC_STACK_H
#define _PRS_ALLOC_STACK_H

#include <prs/types.h>

void* prs_stack_create(prs_size_t size, prs_size_t* available_size);
void prs_stack_destroy(void* stack);

prs_bool_t prs_stack_grow(void* stack, prs_size_t old_size, void* failed_ptr, prs_size_t* new_size);
prs_bool_t prs_stack_address_in_range(void* stack, void* address);

#endif /* _PRS_ALLOC_STACK_H */

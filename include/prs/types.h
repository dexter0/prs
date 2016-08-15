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
 *  This file contains the basic types declarations.
 */

#ifndef _PRS_TYPES_H
#define _PRS_TYPES_H

#include <prs/pal/types.h>
#include <prs/config.h>

/** \brief Converts an expression to a boolean expression. */
#define PRS_BOOL(expr)                  ((expr) ? PRS_TRUE : PRS_FALSE)

/** \brief CPU core mask type. */
typedef PRS_CPU_TYPE prs_core_mask_t;

/** \brief PRS object ID type. */
typedef prs_uint32_t prs_object_id_t;

#endif /* _PRS_TYPES_H */

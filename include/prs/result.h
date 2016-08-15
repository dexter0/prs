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
 *  This file contains the common PRS result values.
 */

#ifndef _PRS_RESULT_H
#define _PRS_RESULT_H

#include <prs/types.h>

/**
 * \brief
 *  Defines common result codes used in PRS.
 */
typedef enum prs_result {
    PRS_OK = 0,
    PRS_UNKNOWN,
    PRS_NOT_IMPLEMENTED,
    PRS_OUT_OF_MEMORY,
    PRS_PLATFORM_ERROR,
    PRS_INVALID_STATE,
    PRS_NOT_FOUND,
    PRS_ALREADY_EXISTS,
    PRS_EMPTY,
    PRS_LOCKED,
    PRS_TIMEOUT
} prs_result_t;

#endif /* _PRS_RESULT_H */

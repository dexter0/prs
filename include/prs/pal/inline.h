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
 *  This file contains the PAL inline preprocessor definitions.
 */
 
#ifndef _PRS_PAL_INLINE_H
#define _PRS_PAL_INLINE_H

#include <prs/pal/compiler.h>

#if PRS_PAL_COMPILER == PRS_PAL_COMPILER_GCC
    #define PRS_INLINE                  __inline
#elif PRS_PAL_COMPILER == PRS_PAL_COMPILER_MSVC
    #define PRS_INLINE                  __inline
#endif

/**
 * \def PRS_INLINE
 * \brief
 *  Defines the inline compiler directive.
 */
#if !defined(PRS_INLINE)
    #error PRS_INLINE is not defined
    #define PRS_INLINE /* doxygen */
#endif

#endif /* _PRS_PAL_INLINE_H */
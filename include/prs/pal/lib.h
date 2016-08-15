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
 *  This file contains the PAL dynamic library preprocessor definitions.
 */

#ifndef _PRS_PAL_LIB
#define _PRS_PAL_LIB

#include <prs/pal/compiler.h>
#include <prs/pal/os.h>

#if PRS_PAL_COMPILER == PRS_PAL_COMPILER_GCC
    #if PRS_PAL_OS == PRS_PAL_OS_WINDOWS
        #define PRS_PAL_LIB_IMPORT      __attribute__((dllimport))
        #define PRS_PAL_LIB_EXPORT      __attribute__((dllexport))
    #elif PRS_PAL_OS == PRS_PAL_OS_LINUX
        /* Builder is responsible for setting -fvisibility=hidden in exported code */
        #define PRS_PAL_LIB_IMPORT
        #define PRS_PAL_LIB_EXPORT      __attribute__((visibility("default")))
    #endif
#elif PRS_PAL_COMPILER == PRS_PAL_COMPILER_MSVC
    #define PRS_PAL_LIB_IMPORT          __declspec(dllimport)
    #define PRS_PAL_LIB_EXPORT          __declspec(dllexport)
#endif

/**
 * \def PRS_PAL_LIB_IMPORT
 * \brief
 *  Defines the symbol import compiler directive.
 */
#if !defined(PRS_PAL_LIB_IMPORT)
    #error PRS_PAL_LIB_IMPORT is not defined
    #define PRS_PAL_LIB_IMPORT /* doxygen */
#endif

/**
 * \def PRS_PAL_LIB_EXPORT
 * \brief
 *  Defines the symbol export compiler directive.
 */
#if !defined(PRS_PAL_LIB_EXPORT)
    #error PRS_PAL_LIB_EXPORT is not defined
    #define PRS_PAL_LIB_EXPORT /* doxygen */
#endif

#endif /* _PRS_PAL_LIB */
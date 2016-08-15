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
 *  This file contains the PAL compiler preprocessor definitions.
 */

#ifndef _PRS_PAL_COMPILER_H
#define _PRS_PAL_COMPILER_H

/** \brief GCC compiler. */
#define PRS_PAL_COMPILER_GCC            0
/** \brief MSVC compiler. */
#define PRS_PAL_COMPILER_MSVC           1

#if defined(__GNUC__)
    #define PRS_PAL_COMPILER            PRS_PAL_COMPILER_GCC
    #define PRS_PAL_COMPILER_NAME       "gcc"
    #define __PRS_PAL_COMPILER_STR(x)   #x
    #define _PRS_PAL_COMPILER_STR(x)    __PRS_PAL_COMPILER_STR(x)
    #define PRS_PAL_COMPILER_VERSION    _PRS_PAL_COMPILER_STR(__GNUC__) \
        "."_PRS_PAL_COMPILER_STR(__GNUC_MINOR__) \
        "."_PRS_PAL_COMPILER_STR(__GNUC_PATCHLEVEL__)
#elif defined(_MSC_VER)
    #define PRS_PAL_COMPILER            PRS_PAL_COMPILER_MSVC
    #define PRS_PAL_COMPILER_NAME       "msvc"
    #define __PRS_PAL_COMPILER_STR(x)   #x
    #define _PRS_PAL_COMPILER_STR(x)    __PRS_PAL_COMPILER_STR(x)
    #define PRS_PAL_COMPILER_VERSION    _PRS_PAL_COMPILER_STR(_MSC_FULL_VER)
#endif

/**
 * \def PRS_PAL_COMPILER
 * \brief
 *  Defines the current compiler.
 *
 *  It is one of the following:
 *    - \ref PRS_PAL_COMPILER_GCC
 *    - \ref PRS_PAL_COMPILER_MSVC
 */
#if !defined(PRS_PAL_COMPILER)
    #error PRS_PAL_COMPILER is not defined
    #define PRS_PAL_COMPILER /* doxygen */
#endif

/**
 * \def PRS_PAL_COMPILER_NAME
 * \brief
 *  Defines the current compiler name in a string literal format.
 */
#if !defined(PRS_PAL_COMPILER_NAME)
    #error PRS_PAL_COMPILER_NAME is not defined
    #define PRS_PAL_COMPILER_NAME /* doxygen */
#endif

/**
 * \def PRS_PAL_COMPILER_VERSION
 * \brief
 *  Defines the current compiler version in a string literal format.
 */
#if !defined(PRS_PAL_COMPILER_VERSION)
    #error PRS_PAL_COMPILER_VERSION is not defined
    #define PRS_PAL_COMPILER_VERSION /* doxygen */
#endif

#endif /* _PRS_PAL_COMPILER_H */

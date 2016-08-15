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
 *  This file contains the PAL architecture preprocessor definitions.
 */

#ifndef _PRS_PAL_ARCH_H
#define _PRS_PAL_ARCH_H

#include <prs/pal/compiler.h>

/** \brief x86 architecture */
#define PRS_PAL_ARCH_X86                0
/** \brief AMD64 architecture */
#define PRS_PAL_ARCH_AMD64              1
/** \brief ARM architecture */
#define PRS_PAL_ARCH_ARM                2

#if PRS_PAL_COMPILER == PRS_PAL_COMPILER_GCC
    #define PRS_PAL_POINTER_SIZE        __SIZEOF_POINTER__
    #if defined(__i386__) || defined(__i486__) || defined(__i586__) || defined(__i686__)
        #define PRS_PAL_REGISTER_SIZE   4
        #define PRS_PAL_ARCH            PRS_PAL_ARCH_X86
    #elif defined(__amd64__)
        #define PRS_PAL_REGISTER_SIZE   8
        #define PRS_PAL_ARCH            PRS_PAL_ARCH_AMD64
    #elif defined(__arm__)
        #define PRS_PAL_REGISTER_SIZE   4
        #define PRS_PAL_ARCH            PRS_PAL_ARCH_ARM
    #endif
#elif PRS_PAL_COMPILER == PRS_PAL_COMPILER_MSVC
    #if defined(_M_IX86)
        #define PRS_PAL_POINTER_SIZE    4
        #define PRS_PAL_REGISTER_SIZE   4
        #define PRS_PAL_ARCH            PRS_PAL_ARCH_X86
    #elif defined(_M_AMD64)
        #define PRS_PAL_POINTER_SIZE    8
        #define PRS_PAL_REGISTER_SIZE   8
        #define PRS_PAL_ARCH            PRS_PAL_ARCH_AMD64
    #endif
#endif

#if PRS_PAL_ARCH == PRS_PAL_ARCH_X86
    #define PRS_PAL_ARCH_NAME           "x86"
#elif PRS_PAL_ARCH == PRS_PAL_ARCH_AMD64
    #define PRS_PAL_ARCH_NAME           "amd64"
#elif PRS_PAL_ARCH == PRS_PAL_ARCH_ARM
    #define PRS_PAL_ARCH_NAME           "arm"
#endif

/**
 * \def PRS_PAL_POINTER_SIZE
 * \brief
 *  Defines the size of a pointer in the current architecture.
 */
#if !defined(PRS_PAL_POINTER_SIZE)
    #error PRS_PAL_POINTER_SIZE is not defined
    #define PRS_PAL_POINTER_SIZE /* doxygen */
#endif

/**
 * \def PRS_PAL_REGISTER_SIZE
 * \brief
 *  Defines the size of a register in the current architecture.
 */
#if !defined(PRS_PAL_REGISTER_SIZE)
    #error PRS_PAL_REGISTER_SIZE is not defined
    #define PRS_PAL_REGISTER_SIZE /* doxygen */
#endif

/**
 * \def PRS_PAL_ARCH
 * \brief
 *  Defines the current architecture.
 *
 *  It is one of the following:
 *    - \ref PRS_PAL_ARCH_X86
 *    - \ref PRS_PAL_ARCH_AMD64
 *    - \ref PRS_PAL_ARCH_ARM
 */
#if !defined(PRS_PAL_ARCH)
    #error PRS_PAL_ARCH is not defined
    #define PRS_PAL_ARCH /* doxygen */
#endif

/**
 * \def PRS_PAL_ARCH_NAME
 * \brief
 *  Defines the current architecture name in a string literal format.
 */
#if !defined(PRS_PAL_ARCH_NAME)
    #error PRS_PAL_ARCH_NAME is not defined
    #define PRS_PAL_ARCH_NAME /* doxygen */
#endif

#endif /* _PRS_PAL_ARCH_H */

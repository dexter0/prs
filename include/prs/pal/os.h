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
 *  This file contains the PAL operating system preprocessor definitions and function declarations.
 */

#ifndef _PRS_PAL_OS_H
#define _PRS_PAL_OS_H

#include <prs/pal/compiler.h>
#include <prs/types.h>

/** \brief Linux operating system. */
#define PRS_PAL_OS_LINUX                0
/** \brief Windows operating system. */
#define PRS_PAL_OS_WINDOWS              1

#if PRS_PAL_COMPILER == PRS_PAL_COMPILER_GCC
    #if defined(__linux__)
        #define PRS_PAL_OS              PRS_PAL_OS_LINUX
    #elif defined(_WIN32)
        #define PRS_PAL_OS              PRS_PAL_OS_WINDOWS
    #endif
#elif PRS_PAL_COMPILER == PRS_PAL_COMPILER_MSVC
    #if defined(_WIN32)
        #define PRS_PAL_OS              PRS_PAL_OS_WINDOWS
    #endif
#endif

#if PRS_PAL_OS == PRS_PAL_OS_LINUX
    #define PRS_PAL_OS_NAME             "linux"
#elif PRS_PAL_OS == PRS_PAL_OS_WINDOWS
    #define PRS_PAL_OS_NAME             "windows"
#endif

/**
 * \def PRS_PAL_OS
 * \brief
 *  Defines the current operating system.
 *
 *  It is one of the following:
 *    - \ref PRS_PAL_OS_LINUX
 *    - \ref PRS_PAL_OS_WINDOWS
 */
#if !defined(PRS_PAL_OS)
    #error PRS_PAL_OS is not defined
    #define PRS_PAL_OS /* doxygen */
#endif

/**
 * \def PRS_PAL_OS_NAME
 * \brief
 *  Defines the current operating system name in a string literal format.
 */
#if !defined(PRS_PAL_OS_NAME)
    #error PRS_PAL_OS_NAME is not defined
    #define PRS_PAL_OS_NAME /* doxygen */
#endif

/**
 * \brief
 *  Initializes OS facilities used by PRS.
 */
void prs_pal_os_init(void);

/**
 * \brief
 *  Uninitializes OS facilities used by PRS.
 */
void prs_pal_os_uninit(void);

/**
 * \brief
 *  Returns the number of CPU cores in the system.
 */
prs_size_t prs_pal_os_get_core_count(void);

/**
 * \brief
 *  Returns the page size of the system.
 */
prs_size_t prs_pal_os_get_page_size(void);

/**
 * \brief
 *  Returns the huge page size of the system.
 */
prs_size_t prs_pal_os_get_huge_page_size(void);

/**
 * \brief
 *  Returns the version of the operating system.
 */
const char* prs_pal_os_get_version(void);

/**
 * \brief
 *  Returns the system's name.
 */
const char* prs_pal_os_get_computer(void);

#endif /* _PRS_PAL_OS_H */

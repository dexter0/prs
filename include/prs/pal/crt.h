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
 *  This file contains the PAL CRT definitions.
 */

#ifndef _PRS_PAL_CRT_H
#define _PRS_PAL_CRT_H

#include <prs/pal/compiler.h>
#include <prs/pal/inline.h>
#include <prs/pal/os.h>

/**
 * \fn _prs_pal_crt_ctors
 * \brief
 *  This function is called when a dynamically loaded PRS executable starts. It must initialize all the C constructs
 *  necessary for the executable's function.
 */
 
/**
 * \fn _prs_pal_crt_dtors
 * \brief
 *  This function is called when a dynamically loaded PRS executable is destroyed. It must uninitialize all the C
 *  constructs that were initialized in \ref _prs_pal_crt_ctors.
 */

#if PRS_PAL_COMPILER == PRS_PAL_COMPILER_GCC

typedef void (*_prs_pal_crt_func_ptr_t)(void);

#if PRS_PAL_OS == PRS_PAL_OS_WINDOWS

/* MinGW still defines __CTOR_LIST__ and __DTOR_LIST__ in the default linker script */
extern _prs_pal_crt_func_ptr_t __CTOR_LIST__[];
extern _prs_pal_crt_func_ptr_t __DTOR_LIST__[];

static PRS_INLINE void _prs_pal_crt_ctors(void)
{
    unsigned long nptrs = (unsigned long)(uintptr_t)__CTOR_LIST__[0];
    unsigned long i;

    if (nptrs == (unsigned long)-1) {
        for (nptrs = 0; __CTOR_LIST__[nptrs + 1]; nptrs++) {
        }
    }

    for (i = nptrs; i >= 1; i--) {
        __CTOR_LIST__[i]();
    }
}

static void _prs_pal_crt_dtors(void)
{
    static _prs_pal_crt_func_ptr_t *p = __DTOR_LIST__ + 1;

    while (*p) {
        (*p)();
        ++p;
    }
}

#else /* PRS_PAL_OS == PRS_PAL_OS_WINDOWS */

/* On Linux, the .init_array section is used with the following symbols */
extern _prs_pal_crt_func_ptr_t __preinit_array_start[];
extern _prs_pal_crt_func_ptr_t __preinit_array_end[];
extern _prs_pal_crt_func_ptr_t __init_array_start[];
extern _prs_pal_crt_func_ptr_t __init_array_end[];
extern _prs_pal_crt_func_ptr_t __fini_array_start[];
extern _prs_pal_crt_func_ptr_t __fini_array_end[];

static PRS_INLINE void _prs_pal_crt_ctors(void)
{
    const unsigned long preinit_array_count = __preinit_array_end - __preinit_array_start;
    const unsigned long init_array_count = __init_array_end - __init_array_start;

    for (unsigned long i = 0; i < preinit_array_count; ++i) {
        __preinit_array_start[i]();
    }

    for (unsigned long i = 0; i < init_array_count; ++i) {
        __init_array_start[i]();
    }
}

static void _prs_pal_crt_dtors(void)
{
    const unsigned long fini_array_count = __fini_array_end - __fini_array_start;

    unsigned long i = fini_array_count;
    while (i-- > 0) {
        __fini_array_start[i]();
    }
}

#endif /* PRS_PAL_OS != PRS_PAL_OS_WINDOWS */

#endif /* PRS_PAL_COMPILER == PRS_PAL_COMPILER_GCC */

#endif /* _PRS_PAL_CRT_H */

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
 *  This file contains the PAL cycle counter definitions.
 */

#ifndef _PRS_PAL_CYCLES_H
#define _PRS_PAL_CYCLES_H

#include <prs/pal/arch.h>
#include <prs/pal/compiler.h>
#include <prs/pal/inline.h>
#include <prs/pal/types.h>

/**
 * \typedef prs_cycles_t
 * \brief
 *  Cycle counter type.
 */

/**
 * \fn prs_cycles_now
 * \brief
 *  This function returns the value of a cycle counter. Ideally, the cycle counter should ignore hardware sleep states
 *  and be consistent across CPU cores on the same system.
 */

#if PRS_PAL_ARCH == PRS_PAL_ARCH_X86 || PRS_PAL_ARCH == PRS_PAL_ARCH_AMD64
typedef prs_uint64_t prs_cycles_t;

#if PRS_PAL_COMPILER == PRS_PAL_COMPILER_GCC

static PRS_INLINE prs_cycles_t prs_cycles_now(void)
{
    prs_uint32_t hi;
    prs_uint32_t lo;

    /*
     * For this to work properly, the CPU must have the Invariant TSC feature so that the cycle count increments
     * symmetrically on multiple cores.
     * On Linux, the feature can be observed through the "nonstop_tsc" flag in /proc/cpuinfo.
     */
    __asm__ __volatile__ (
        "rdtsc"
        : "=a"(lo),
          "=d"(hi)
    );

    return (prs_uint64_t)lo | ((prs_uint64_t)hi << 32);
}

#endif /* PRS_PAL_COMPILER == PRS_PAL_COMPILER_GCC */

#endif /* PRS_PAL_ARCH == PRS_PAL_ARCH_X86 || PRS_PAL_ARCH == PRS_PAL_ARCH_AMD64 */

#endif /* _PRS_PAL_CYCLES_H */

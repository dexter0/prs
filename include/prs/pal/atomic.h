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
 *  This file contains the PAL atomic declarations.
 */

#ifndef _PRS_PAL_ATOMIC_H
#define _PRS_PAL_ATOMIC_H

#include <prs/pal/compiler.h>

#if PRS_PAL_COMPILER == PRS_PAL_COMPILER_GCC

#if (__STDC_VERSION__ >= 201112L) && !defined (__STDC_NO_ATOMICS__)

#include <stdatomic.h>

#define PRS_ATOMIC                      _Atomic

#define prs_pal_atomic_load(p)          atomic_load(p)
#define prs_pal_atomic_store(p, v)      atomic_store(p, v)
#define prs_pal_atomic_exchange(p, v)   atomic_exchange(p, v)
#define prs_pal_atomic_compare_exchange_strong(p, e, v) \
                                        atomic_compare_exchange_strong(p, e, v)
#define prs_pal_atomic_compare_exchange_weak(p, e, v) \
                                        atomic_compare_exchange_weak(p, e, v)
#define prs_pal_atomic_fetch_add(p, v)  atomic_fetch_add(p, v)
#define prs_pal_atomic_fetch_sub(p, v)  atomic_fetch_sub(p, v)
#define prs_pal_atomic_fetch_or(p, v)   atomic_fetch_or(p, v)
#define prs_pal_atomic_fetch_xor(p, v)  atomic_fetch_xor(p, v)
#define prs_pal_atomic_fetch_and(p, v)  atomic_fetch_and(p, v)

#else /* (__STDC_VERSION__ >= 201112L) && !defined (__STDC_NO_ATOMICS__) */

/* Atomic support for old GCC versions */

#define _PRS_PAL_COMPILER_FENCE()       __asm__ __volatile__("":::"memory")

#define PRS_ATOMIC

#define prs_pal_atomic_load(p)          ({ \
        _PRS_PAL_COMPILER_FENCE(); /* Note: should probably be __sync_synchronize() on ARM */ \
        __typeof__(*p) tmp = *(p); \
        _PRS_PAL_COMPILER_FENCE(); \
        tmp; \
    })

#define prs_pal_atomic_store(p, v)      do { _PRS_PAL_COMPILER_FENCE(); *(p) = (v); __sync_synchronize(); } while (0)

#define prs_pal_atomic_exchange(p, v)   ({ \
        _PRS_PAL_COMPILER_FENCE(); \
        __typeof__(*p) r = __sync_lock_test_and_set(p, v); \
        _PRS_PAL_COMPILER_FENCE(); \
        r; \
    })

#define prs_pal_atomic_compare_exchange_strong(p, pe, v) ({ \
        _PRS_PAL_COMPILER_FENCE(); \
        __typeof__(pe) lpe = (pe); /* Copy pe in case it is an expression */ \
        __typeof__(*(p)) tmp = *lpe; \
        __typeof__(*(p)) prev = __sync_val_compare_and_swap(p, tmp, v); \
        _PRS_PAL_COMPILER_FENCE(); \
        if (tmp != prev) { \
            *lpe = prev; \
        } \
        tmp == prev; \
    })

#define prs_pal_atomic_compare_exchange_weak(p, pe, v) \
                                        prs_pal_atomic_compare_exchange_strong(p, pe, v)

#define _PRS_PAL_ATOMIC_FETCH(op, p, v) ({ \
        _PRS_PAL_COMPILER_FENCE(); \
        __typeof__(*p) r = op(p, v); \
        _PRS_PAL_COMPILER_FENCE(); \
        r; \
    })

#define prs_pal_atomic_fetch_add(p, v)  _PRS_PAL_ATOMIC_FETCH(__sync_fetch_and_add, p, v)
#define prs_pal_atomic_fetch_sub(p, v)  _PRS_PAL_ATOMIC_FETCH(__sync_fetch_and_sub, p, v)
#define prs_pal_atomic_fetch_or(p, v)   _PRS_PAL_ATOMIC_FETCH(__sync_fetch_and_or, p, v)
#define prs_pal_atomic_fetch_xor(p, v)  _PRS_PAL_ATOMIC_FETCH(__sync_fetch_and_xor, p, v)
#define prs_pal_atomic_fetch_and(p, v)  _PRS_PAL_ATOMIC_FETCH(__sync_fetch_and_and, p, v)

#endif /* (__STDC_VERSION__ >= 201112L) && !defined (__STDC_NO_ATOMICS__) */

#else
#error Unknown compiler.
#endif

/**
 * \brief
 *  Defines an atomic type specifier.
 */
#if !defined(PRS_ATOMIC)
    #error PRS_ATOMIC is not defined
    #define PRS_ATOMIC /* doxygen */
#endif

#endif /* _PRS_PAL_ATOMIC_H */

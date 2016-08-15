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
 *  This file contains the PAL bit operation definitions.
 */

#ifndef _PRS_PAL_BITOPS_H
#define _PRS_PAL_BITOPS_H

#include <prs/pal/arch.h>
#include <prs/pal/inline.h>
#include <prs/assert.h>
#include <prs/types.h>

#define _PRS_BITOPS_HSB(v, t) \
    do { \
        for (prs_int_t pos = sizeof(t) * 8 - 1; pos >= 0; --pos) { \
            if (v & ((t)1 << pos)) { \
                return pos; \
            } \
        } \
        return -1; \
    } while (0)

#define _PRS_BITOPS_LSB(v, t) \
    do { \
        const prs_int_t end = sizeof(t) * 8 - 1; \
        for (prs_int_t pos = 0; pos < end; ++pos) { \
            if (v & ((t)1 << pos)) { \
                return pos; \
            } \
        } \
        return -1; \
    } while (0)

/**
 * \brief
 *  Returns the highest (most significant) set bit position.
 */
static PRS_INLINE prs_int_t prs_bitops_hsb_uint64(prs_uint64_t v)
{
#if PRS_PAL_COMPILER == PRS_PAL_COMPILER_GCC
    if (v == 0) {
        return -1;
    } else {
        const int builtin_result = __builtin_clzll(v);
        PRS_ASSERT(builtin_result >= 0 && builtin_result < 64);
        return 64 - builtin_result - 1;
    }
#else
    _PRS_BITOPS_HSB(v, prs_uint64_t);
#endif
}

/**
 * \brief
 *  Returns the highest (most significant) set bit position.
 */
static PRS_INLINE prs_int_t prs_bitops_hsb_uint32(prs_uint32_t v)
{
#if PRS_PAL_COMPILER == PRS_PAL_COMPILER_GCC
    if (v == 0) {
        return -1;
    } else {
        const int builtin_result = __builtin_clz(v);
        PRS_ASSERT(builtin_result >= 0 && builtin_result < 32);
        return 32 - builtin_result - 1;
    }
#else
    _PRS_BITOPS_HSB(v, prs_uint32_t);
#endif
}

/**
 * \brief
 *  Returns the highest (most significant) set bit position.
 */
static PRS_INLINE prs_int_t prs_bitops_hsb_uint(prs_uint_t v)
{
#if PRS_PAL_POINTER_SIZE == 4
    return prs_bitops_hsb_uint32(v);
#elif PRS_PAL_POINTER_SIZE == 8
    return prs_bitops_hsb_uint64(v);
#endif
}

/**
 * \brief
 *  Returns the lowest (least significant) set bit position.
 */
static PRS_INLINE prs_int_t prs_bitops_lsb_uint64(prs_uint64_t v)
{
#if PRS_PAL_COMPILER == PRS_PAL_COMPILER_GCC
    if (v == 0) {
        return -1;
    } else {
        const int builtin_result = __builtin_ctzll(v);
        PRS_ASSERT(builtin_result >= 0 && builtin_result < 64);
        return builtin_result;
    }
#else
    _PRS_BITOPS_LSB(v, prs_uint64_t);
#endif
}

/**
 * \brief
 *  Returns the lowest (least significant) set bit position.
 */
static PRS_INLINE prs_int_t prs_bitops_lsb_uint32(prs_uint32_t v)
{
#if PRS_PAL_COMPILER == PRS_PAL_COMPILER_GCC
    if (v == 0) {
        return -1;
    } else {
        const int builtin_result = __builtin_ctz(v);
        PRS_ASSERT(builtin_result >= 0 && builtin_result < 32);
        return builtin_result;
    }
#else
    _PRS_BITOPS_LSB(v, prs_uint32_t);
#endif
}

/**
 * \brief
 *  Returns the lowest (least significant) set bit position.
 */
static PRS_INLINE prs_int_t prs_bitops_lsb_uint(prs_uint_t v)
{
#if PRS_PAL_POINTER_SIZE == 4
    return prs_bitops_lsb_uint32(v);
#elif PRS_PAL_POINTER_SIZE == 8
    return prs_bitops_lsb_uint64(v);
#endif
}

/**
 * \brief
 *  Returns the next power of two.
 */
static PRS_INLINE prs_uint_t prs_bitops_next_power_of_2(prs_uint_t v)
{
    --v;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
#if PRS_PAL_POINTER_SIZE == 8
    v |= v >> 32;
#endif /* PRS_PAL_POINTER_SIZE == 8 */
    return v + 1;
}

/**
 * \brief
 *  Returns the first multiple of \p align that is greater than \p size.
 */
static PRS_INLINE prs_uint_t prs_bitops_align_size(prs_uint_t size, prs_uint_t align)
{
    const prs_size_t mask = (align - 1);
    PRS_ASSERT(!(align & mask));
    return (size + mask) & ~mask;
}

/**
 * \brief
 *  Returns if the specified \p value is a power of two.
 */
static PRS_INLINE prs_bool_t prs_bitops_is_power_of_2(prs_uint_t value)
{
    return PRS_BOOL(value && !(value & (value - 1)));
}

#endif /* _PRS_PAL_BITOPS_H */

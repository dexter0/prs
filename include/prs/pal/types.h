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
 *  This file contains the PAL types preprocessor definitions and typedef definitions.
 */

#ifndef _PRS_PAL_TYPES_H
#define _PRS_PAL_TYPES_H

#include <prs/pal/arch.h>
#include <prs/pal/compiler.h>

#if PRS_PAL_COMPILER == PRS_PAL_COMPILER_GCC || \
    (PRS_PAL_COMPILER == PRS_PAL_COMPILER_MSVC && _MSC_VER_FULL >= 180021005)
    #include <stdbool.h>
    #define PRS_PAL_TYPE_BOOL           bool
    #define PRS_PAL_BOOL_TRUE           true
    #define PRS_PAL_BOOL_FALSE          false
    #include <stdint.h>
    #define PRS_PAL_TYPE_INT8           int8_t
    #define PRS_PAL_TYPE_UINT8          uint8_t
    #define PRS_PAL_TYPE_INT16          int16_t
    #define PRS_PAL_TYPE_UINT16         uint16_t
    #define PRS_PAL_TYPE_INT32          int32_t
    #define PRS_PAL_TYPE_UINT32         uint32_t
    #define PRS_PAL_TYPE_INT64          int64_t
    #define PRS_PAL_TYPE_UINT64         uint64_t
    #define PRS_PAL_TYPE_INTPTR         intptr_t
    #define PRS_PAL_TYPE_UINTPTR        uintptr_t
#elif PRS_PAL_COMPILER == PRS_PAL_COMPILER_MSVC
    #include <windef.h>
    #define PRS_PAL_TYPE_BOOL           BOOL
    #define PRS_PAL_BOOL_TRUE           TRUE
    #define PRS_PAL_BOOL_FALSE          FALSE
    #include <basetsd.h>
    #define PRS_PAL_TYPE_INT8           INT8
    #define PRS_PAL_TYPE_UINT8          UINT8
    #define PRS_PAL_TYPE_INT16          INT16
    #define PRS_PAL_TYPE_UINT16         UINT16
    #define PRS_PAL_TYPE_INT32          INT32
    #define PRS_PAL_TYPE_UINT32         UINT32
    #define PRS_PAL_TYPE_INT64          INT64
    #define PRS_PAL_TYPE_UINT64         UINT64
    #define PRS_PAL_TYPE_INTPTR         INT_PTR
    #define PRS_PAL_TYPE_UINTPTR        UINT_PTR
#endif

#if PRS_PAL_REGISTER_SIZE == 4
    #define PRS_PAL_TYPE_INT            PRS_PAL_TYPE_INT32
    #define PRS_PAL_TYPE_UINT           PRS_PAL_TYPE_UINT32
#elif PRS_PAL_REGISTER_SIZE == 8
    #define PRS_PAL_TYPE_INT            PRS_PAL_TYPE_INT64
    #define PRS_PAL_TYPE_UINT           PRS_PAL_TYPE_UINT64
#endif

#if !defined(PRS_PAL_TYPE_BOOL) || \
    !defined(PRS_PAL_BOOL_TRUE) || \
    !defined(PRS_PAL_BOOL_FALSE) || \
    !defined(PRS_PAL_TYPE_INT8) || \
    !defined(PRS_PAL_TYPE_UINT8) || \
    !defined(PRS_PAL_TYPE_INT16) || \
    !defined(PRS_PAL_TYPE_UINT16) || \
    !defined(PRS_PAL_TYPE_INT32) || \
    !defined(PRS_PAL_TYPE_UINT32) || \
    !defined(PRS_PAL_TYPE_INT64) || \
    !defined(PRS_PAL_TYPE_UINT64) || \
    !defined(PRS_PAL_TYPE_INTPTR) || \
    !defined(PRS_PAL_TYPE_UINTPTR) || \
    !defined(PRS_PAL_TYPE_INT) || \
    !defined(PRS_PAL_TYPE_UINT)
    #error PRS_PAL_TYPEs are not all defined
#endif

/** \brief Boolean type. */
typedef PRS_PAL_TYPE_BOOL prs_bool_t;

/** \brief True value. */
#define PRS_TRUE                        (PRS_PAL_BOOL_TRUE)
/** \brief False value. */
#define PRS_FALSE                       (PRS_PAL_BOOL_FALSE)

/** \brief 8-bit signed integer. */
typedef PRS_PAL_TYPE_INT8 prs_int8_t;
/** \brief 8-bit unsigned integer. */
typedef PRS_PAL_TYPE_UINT8 prs_uint8_t;
/** \brief 16-bit signed integer. */
typedef PRS_PAL_TYPE_INT16 prs_int16_t;
/** \brief 16-bit unsigned integer. */
typedef PRS_PAL_TYPE_UINT16 prs_uint16_t;
/** \brief 32-bit signed integer. */
typedef PRS_PAL_TYPE_INT32 prs_int32_t;
/** \brief 32-bit unsigned integer. */
typedef PRS_PAL_TYPE_UINT32 prs_uint32_t;
/** \brief 64-bit signed integer. */
typedef PRS_PAL_TYPE_INT64 prs_int64_t;
/** \brief 64-bit unsigned integer. */
typedef PRS_PAL_TYPE_UINT64 prs_uint64_t;

/** \brief Signed integer that has the same size as a pointer. */
typedef PRS_PAL_TYPE_INTPTR prs_intptr_t;
/** \brief Unsigned integer that has the same size as a pointer. */
typedef PRS_PAL_TYPE_UINTPTR prs_uintptr_t;

/** \brief Signed integer that has the same size as a register. */
typedef PRS_PAL_TYPE_INT prs_int_t;
/** \brief Unigned integer that has the same size as a register. */
typedef PRS_PAL_TYPE_UINT prs_uint_t;
/** \brief Size type. */
typedef PRS_PAL_TYPE_UINT prs_size_t;

#endif /* _PRS_PAL_TYPES_H */

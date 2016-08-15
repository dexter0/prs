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
 *  This file contains string manipulation definitions.
 *
 *  Definitions in this file offer common string manipulation functions which avoid several pitfalls and simplify
 *  code related to using the str* and *printf family of functions.
 */

#include <stdio.h>

#include <prs/assert.h>
#include <prs/rtc.h>
#include <prs/str.h>

/**
 * \brief
 *  Copy \p src into \p dst, up to \p max characters. The last character is guaranteed to be a terminating \p null.
 * \param dst
 *  Destination string.
 * \param src
 *  Source string.
 * \param max
 *  Size of the \p dst string buffer.
 * \return
 *  Position of the last character (non-\p null) in \p dst.
 */
int prs_str_copy(char* dst, const char* src, int max)
{
    PRS_PRECONDITION(dst);
    PRS_PRECONDITION(src);
    PRS_PRECONDITION(max > 0);

    return prs_str_append(dst, src, 0, max);
}

/**
 * \brief
 *  Append \p src into \p dst+offset, up to \p max-offset characters. The last character is guaranteed to be a terminating \p null.
 * \param dst
 *  Destination string.
 * \param src
 *  Source string.
 * \param offset
 *  Offset in \p dst to start writing from.
 * \param max
 *  Size of the \p dst string buffer.
 * \return
 *  Position of the last character (non-\p null) in \p dst.
 */
int prs_str_append(char* dst, const char* src, int offset, int max)
{
    PRS_PRECONDITION(dst);
    PRS_PRECONDITION(src);
    PRS_PRECONDITION(offset >= 0);
    PRS_PRECONDITION(max > 0);

    const char* end = dst + max;
    char* d = dst + offset;
    const char* s = src;
    int pos = offset;

    while (d < end && *s) {
        *d++ = *s++;
        ++pos;
    }

    *d = '\0';

    return pos;
}

/**
 * \brief
 *  Print a formatted string in \p dst. The last character is guaranteed to be a terminating \p null.
 * \param dst
 *  Destination string.
 * \param max
 *  Size of the \p dst string buffer.
 * \param fmt
 *  Format string.
 * \param ...
 *  Variable arguments.
 * \return
 *  Position of the last character (non-\p null) in \p dst.
 */
int prs_str_printf(char* dst, int max, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    const int r = prs_str_append_vprintf(dst, 0, max, fmt, va);
    va_end(va);
    return r;
}

/**
 * \brief
 *  Append a formatted string in \p dst. The last character is guaranteed to be a terminating \p null.
 * \param dst
 *  Destination string.
 * \param offset
 *  Offset in \p dst to start writing from.
 * \param max
 *  Size of the \p dst string buffer.
 * \param fmt
 *  Format string.
 * \param ...
 *  Variable arguments.
 * \return
 *  Position of the last character (non-\p null) in \p dst.
 */
int prs_str_append_printf(char* dst, int offset, int max, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    const int r = prs_str_append_vprintf(dst, offset, max, fmt, va);
    va_end(va);
    return r;
}

/**
 * \brief
 *  Print a formatted string in \p dst. The last character is guaranteed to be a terminating \p null.
 * \param dst
 *  Destination string.
 * \param max
 *  Size of the \p dst string buffer.
 * \param fmt
 *  Format string.
 * \param va
 *  Variable arguments.
 * \return
 *  Position of the last character (non-\p null) in \p dst.
 */
int prs_str_vprintf(char* dst, int max, const char* fmt, va_list va)
{
    return prs_str_append_vprintf(dst, 0, max, fmt, va);
}

/**
 * \brief
 *  Append a formatted string in \p dst. The last character is guaranteed to be a terminating \p null.
 * \param dst
 *  Destination string.
 * \param offset
 *  Offset in \p dst to start writing from.
 * \param max
 *  Size of the \p dst string buffer.
 * \param fmt
 *  Format string.
 * \param va
 *  Variable arguments.
 * \return
 *  Position of the last character (non-\p null) in \p dst.
 */
int prs_str_append_vprintf(char* dst, int offset, int max, const char* fmt, va_list va)
{
    va_list local_va;
    va_copy(local_va, va);
    const size_t n = max - offset;
    const int r = vsnprintf(dst + offset, n, fmt, local_va);
    PRS_RTC_IF (r > n - 1) {
        return 0;
    }
    return offset + r;
}

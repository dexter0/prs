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
 *  This file contains string manipulation declarations.
 */

#ifndef _PRS_STR_H
#define _PRS_STR_H

#include <stdarg.h>

int prs_str_copy(char* dst, const char* src, int max);
int prs_str_append(char* dst, const char* src, int offset, int max);
int prs_str_printf(char* dst, int max, const char* fmt, ...);
int prs_str_append_printf(char* dst, int offset, int max, const char* fmt, ...);
int prs_str_vprintf(char* dst, int max, const char* fmt, va_list va);
int prs_str_append_vprintf(char* dst, int offset, int max, const char* fmt, va_list va);

#endif /* _PRS_STR_H */

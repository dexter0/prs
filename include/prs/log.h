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
 *  This file contains the log module declarations.
 */

#ifndef _PRS_LOG_H
#define _PRS_LOG_H

#include <stdarg.h>

#include <prs/config.h>
#include <prs/types.h>

/**
 * \def PRS_FTRACE
 * \brief
 *  Prints a trace in the log with the function name as a prefix. Does not print the trace when
 *  \ref PRS_FUNCTION_TRACES is not defined.
 */
#if defined(PRS_FUNCTION_TRACES)
#define PRS_FTRACE(...)                 prs_log_ftrace(__FILE__, __LINE__, __func__, __VA_ARGS__)
#else
#define PRS_FTRACE(...)
#endif

void prs_log_init(void);
void prs_log_uninit(void);

void prs_log_vprint(const char* file, int line, const char* function, const char* fmt, va_list va);

void prs_log_print(const char* fmt, ...);

void prs_log_ftrace(const char* file, int line, const char* function, const char* fmt, ...);

prs_int_t prs_log_flush(void);

#endif /* _PRS_LOG_H */

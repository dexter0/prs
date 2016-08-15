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
 *  This file contains the run-time check declarations.
 */

#ifndef _PRS_RTC_H
#define _PRS_RTC_H

#include <prs/types.h>
#include <prs/config.h>
#include <prs/assert.h>

/**
 * \def PRS_RTC_IF
 * \brief
 *  Does a run-time check on the specified condition.
 *
 *  When \ref PRS_RUN_TIME_CHECKING is defined, the condition is verified. In addition, when \ref PRS_ASSERTIONS is
 *  defined, the condition will be asserted first.
 *
 *  The \ref PRS_RTC_IF macro should be used as a normal \p if C statement as it does not stop PRS nor the currently
 *  running task. It is up to the user of the macro to define behavior when the check fails.
 * \param cond
 *  Condition to check.
 * \note
 *  It is advised not to use following \p else statements as it could cause unintended side effects if the
 *  \ref PRS_RTC_IF implementation changes in the future.
 */
#if defined(PRS_RUN_TIME_CHECKING)
#if defined(PRS_ASSERTIONS)
#define PRS_RTC_IF(cond)                if ((cond) && (prs_assert("!("#cond")", __FILE__, __LINE__), \
                                                     prs_rtc(#cond, __FILE__, __LINE__), 1))
#else
#define PRS_RTC_IF(cond)                if ((cond) && (prs_rtc(#cond, __FILE__, __LINE__), 1))
#endif
#else
#if defined(PRS_ASSERTIONS)
#define PRS_RTC_IF(cond)                if ((cond) && (prs_assert("!("#cond")", __FILE__, __LINE__), 0))
#else
#define PRS_RTC_IF(cond)                if (0)
#endif
#endif

void prs_rtc(const char* expr, const char* file, int line);

#endif /* _PRS_RTC_H */

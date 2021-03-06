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
 *  This file contains the run-time check definitions.
 */

#include <prs/log.h>
#include <prs/rtc.h>

/**
 * \brief
 *  Prints a run-time check failure to the log.
 * \param expr
 *  Expression that is printed into the log.
 * \param file
 *  File where the failure occurred.
 * \param line
 *  Line in \p file where the failure occurred.
 * \note
 *  This should never be called directly by code. Use \ref PRS_RTC_IF instead.
 */
void prs_rtc(const char* expr, const char* file, int line)
{
    prs_log_print("RTC: \"%s\" %s:%d\n", expr, file, line);
}

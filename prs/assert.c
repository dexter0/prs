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
 *  This file contains the assertion definitions.
 */

#include <prs/pal/assert.h>
#include <prs/config.h>
#include <prs/init.h>
#include <prs/log.h>
#include <prs/task.h>

#include "task.h"

/**
 * \brief
 *  Raises an assertion failure. In addition to printing an error message to the log, this will terminate PRS.
 * \param expr
 *  Expression that is printed into the log.
 * \param file
 *  File where the assertion occurred.
 * \param line
 *  Line in \p file where the assertion occurred.
 * \note
 *  This should never be called directly by code. Use \ref PRS_ASSERT, \ref PRS_PRECONDITION or \ref PRS_POSTCONDITION to
 *  raise an assertion.
 */
void prs_assert(const char* expr, const char* file, int line)
{
#if defined(PRS_CLEAN)
    prs_log_print("Assertion failed: %s (%s:%d)", expr, file, line);
    struct prs_task* task = prs_task_current();
    if (task) {
        prs_log_print("  in task %s (%u)", task->name, task->id);
    }
    prs_exit_from_excp(-1);
#endif /* PRS_CLEAN */
    prs_pal_assert(expr, file, line);
}

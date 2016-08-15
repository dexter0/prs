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
 *  This file contains the error module definitions.
 */

#include <prs/error.h>
#include <prs/excp.h>
#include <prs/init.h>
#include <prs/task.h>
#include <prs/worker.h>

static enum prs_excp_result prs_error_type_to_excp_behavior(enum prs_error_type error)
{
    switch (error) {
        case PRS_ERROR_TYPE_CONTINUE:
            return PRS_EXCP_RESULT_CONTINUE;
        case PRS_ERROR_TYPE_KILL_TASK:
            return PRS_EXCP_RESULT_KILL_TASK;
        case PRS_ERROR_TYPE_FATAL:
            return PRS_EXCP_RESULT_EXIT;
        default:
            return PRS_EXCP_RESULT_EXIT;
    }
}

/**
 * \brief
 *  Reports an error and executes the action provided by the \p error parameter.
 * \param error
 *  Behavior of the error.
 * \param expr
 *  Expression that is printed into the log.
 * \param file
 *  File where the error occurred.
 * \param line
 *  Line in \p file where the error occurred.
 * \note
 *  This should never be called directly by code. Use \ref PRS_ERROR, \ref PRS_ERROR_IF, \ref PRS_ERROR_WHEN,
 *  \ref PRS_KILL_TASK, \ref PRS_KILL_TASK_WHEN, \ref PRS_FATAL or \ref PRS_FATAL_WHEN to raise an error.
 */
void prs_error(enum prs_error_type error, const char* expr, const char* file, int line)
{
    struct prs_worker* worker = prs_worker_current();
    struct prs_excp_raise_info info = {
        .expr = expr,
        .file = file,
        .line = line,
        .behavior = prs_error_type_to_excp_behavior(error)
    };
    prs_excp_raise(PRS_EXCP_TYPE_PRS, &info, worker, 0);
}

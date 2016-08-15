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
 *  This file contains the declarations for the error module.
 */

#ifndef _PRS_ERROR_H
#define _PRS_ERROR_H

/**
 * \brief
 *  From the current location, reports an error in the log.
 * \param desc
 *  Description of the error.
 */
#define PRS_ERROR(desc)                 prs_error(PRS_ERROR_TYPE_CONTINUE, desc, __FILE__, __LINE__)

/**
 * \brief
 *  From the current location, reports an error in the log if \p cond is \p true. Must be used like an \p if statement.
 * \param cond
 *  Condition that is verified.
 */
#define PRS_ERROR_IF(cond)              if ((cond) && (prs_error(PRS_ERROR_TYPE_CONTINUE, #cond, __FILE__, __LINE__), 1))

/**
 * \brief
 *  From the current location, reports an error in the log when \p cond is \p true.
 * \param cond
 *  Condition that is verified.
 */
#define PRS_ERROR_WHEN(cond)            do { if (cond) { prs_error(PRS_ERROR_TYPE_CONTINUE, #cond, __FILE__, __LINE__); } } while (0);

/**
 * \brief
 *  From the current location, reports and error and kills the current task.
 * \param desc
 *  Description of the error.
 */
#define PRS_KILL_TASK(desc)             prs_error(PRS_ERROR_TYPE_KILL_TASK, desc, __FILE__, __LINE__)

/**
 * \brief
 *  From the current location, reports and error and kills the current task when \p cond is \p true.
 * \param cond
 *  Condition that is verified.
 */
#define PRS_KILL_TASK_WHEN(cond)        do { if (cond) { prs_error(PRS_ERROR_TYPE_KILL_TASK, #cond, __FILE__, __LINE__); } } while (0);

/**
 * \brief
 *  From the current location, reports and error and exits PRS.
 * \param desc
 *  Description of the error.
 */
#define PRS_FATAL(desc)                 prs_error(PRS_ERROR_TYPE_FATAL, desc, __FILE__, __LINE__)

/**
 * \brief
 *  From the current location, reports and error and exits PRS when \p cond is \p true.
 * \param cond
 *  Condition that is verified.
 */
#define PRS_FATAL_WHEN(cond)            do { if (cond) { prs_error(PRS_ERROR_TYPE_FATAL, #cond, __FILE__, __LINE__); } } while (0);

enum prs_error_type {
    PRS_ERROR_TYPE_CONTINUE,
    PRS_ERROR_TYPE_KILL_TASK,
    PRS_ERROR_TYPE_FATAL
};

void prs_error(enum prs_error_type error, const char* expr, const char* file, int line);

#endif /* _PRS_ERROR_H */

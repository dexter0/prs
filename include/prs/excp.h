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
 *  This file contains the exception module declarations.
 */

#ifndef _PRS_EXCP_H
#define _PRS_EXCP_H

#include <prs/result.h>

struct prs_pal_context;
struct prs_task;
struct prs_worker;

/** \brief The possible exception types. */
enum prs_excp_type {
    PRS_EXCP_TYPE_UNKNOWN,
    PRS_EXCP_TYPE_ASSERT,
    PRS_EXCP_TYPE_USER,
    PRS_EXCP_TYPE_PRS,
    PRS_EXCP_TYPE_OS,
    PRS_EXCP_TYPE_STACK_OVERFLOW,
    PRS_EXCP_TYPE_SEGMENTATION_FAULT,
    PRS_EXCP_TYPE_ILLEGAL_INSTRUCTION,
    PRS_EXCP_TYPE_INTEGER,
    PRS_EXCP_TYPE_FLOATING_POINT,
    PRS_EXCP_TYPE_BUS,
    PRS_EXCP_TYPE_USER_INTERRUPT
};

/** \brief The exception handler result codes. */
enum prs_excp_result {
    /** \brief The exception is fatal and PRS should immediately exit. */
    PRS_EXCP_RESULT_EXIT,
    /** \brief The exception is fatal for the currently running task and it should be destroyed. */
    PRS_EXCP_RESULT_KILL_TASK,
    /** \brief The exception is unimportant or corrected; the currently running task can continue its execution. */
    PRS_EXCP_RESULT_CONTINUE,
    /** \brief The exception was not handled; forward the exception to the next exception handler. */
    PRS_EXCP_RESULT_FORWARD
};

/**
 * \brief
 *  This structure adds information to a manually launched exception through \ref prs_excp_raise.
 *
 *  The \p extra parameter of \ref prs_excp_raise should refer to this structure when the \p excp parameter is
 *  \ref PRS_EXCP_TYPE_ASSERT, \ref PRS_EXCP_TYPE_USER, \ref PRS_EXCP_TYPE_PRS or \ref PRS_EXCP_TYPE_OS.
 */
struct prs_excp_raise_info {
    /** \brief Expression that is printed into the log. */
    const char*                         expr;
    /** \brief File where the error occurred. */
    const char*                         file;
    /** \brief Line in \p file where the error occurred. */
    int                                 line;
    /** \brief The desired action to be taken by the exception handler. */
    enum prs_excp_result                behavior;
};

/**
 * \brief
 *  Type representing an exception handler.
 */
typedef enum prs_excp_result (*prs_excp_handler_t)(enum prs_excp_type excp, void* extra, struct prs_worker* worker,
    struct prs_task* task, struct prs_pal_context* context);

void prs_excp_raise(enum prs_excp_type excp, void* extra, struct prs_worker* worker, struct prs_pal_context* context);

prs_result_t prs_excp_register_handler(prs_excp_handler_t handler);

void prs_excp_init(void);

#endif /* _PRS_EXCP_H */

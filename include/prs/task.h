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
 *  This file contains task declarations.
 */

#ifndef _PRS_TASK_H
#define _PRS_TASK_H

#include <prs/config.h>
#include <prs/msgq.h>
#include <prs/result.h>
#include <prs/sched.h>
#include <prs/types.h>

/** \brief Task priority type. */
typedef PRS_TASK_PRIO_TYPE prs_task_prio_t;

struct prs_task;

/**
 * \brief
 *  Task creation parameters.
 */
struct prs_task_create_params {
    /** \brief Name of the task which can then be found through \ref prs_task_find. */
    char                                name[PRS_MAX_TASK_NAME];
    /** \brief Userdata that will be passed as a parameter to the entry point. */
    void*                               userdata;
    /** \brief Initial stack size of the stack. The stack may be extended by an exception handler. */
    prs_size_t                          stack_size;
    /** \brief Priority of the task. May be unused if the scheduler of the task does not support it. */
    prs_task_prio_t                     prio;
    /** \brief Entry point of the task. */
    void                                (*entry)(void* userdata);
};

struct prs_task* prs_task_create(struct prs_task_create_params* params);
void prs_task_destroy(struct prs_task* task);
prs_result_t prs_task_start(struct prs_task* task);
prs_result_t prs_task_stop(struct prs_task* task);

prs_result_t prs_task_set_userdata(struct prs_task* task, void* userdata);
prs_result_t prs_task_get_userdata(struct prs_task* task, void** userdata);

prs_task_prio_t prs_task_get_prio(struct prs_task* task);
void prs_task_set_prio(struct prs_task* task, prs_task_prio_t prio);

prs_task_id_t prs_task_get_id(struct prs_task* task);
struct prs_task* prs_task_current(void);

prs_object_id_t prs_task_find(const char* name);
struct prs_task* prs_task_find_and_lock(const char* name);

#endif /* _PRS_TASK_H */

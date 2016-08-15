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
 *  This file contains the process module declarations.
 */

#ifndef _PRS_PROC_H
#define _PRS_PROC_H

#include <prs/config.h>
#include <prs/object.h>
#include <prs/result.h>
#include <prs/task.h>
#include <prs/types.h>

struct prs_proc;

/**
 * \brief
 *  Main entry point arguments.
 */
struct prs_proc_main_params {
    /** \brief Number of arguments. */
    int                                 argc;
    /** \brief Array of pointers to the arguments. */
    char**                              argv;
};

/**
 * \brief
 *  Process execution parameters.
 */
struct prs_proc_exec_params {
    /** \brief Path of the PRS executable to load. */
    char                                filename[PRS_MAX_PATH];
    /** \brief Full command line (including the executable path). */
    char                                cmdline[PRS_MAX_CMDLINE];
    /** \brief Main task creation parameters. */
    struct prs_task_create_params       main_task_params;
    /** \brief Scheduler object ID on which to add the main task. */
    prs_sched_id_t                      sched_id;
};

void prs_proc_init(void);
void prs_proc_uninit(void);

struct prs_proc* prs_proc_exec(struct prs_proc_exec_params* params);
void prs_proc_destroy(struct prs_proc* proc);
void prs_proc_register_object(struct prs_proc* proc, prs_object_id_t object_id);
prs_result_t prs_proc_unregister_object(struct prs_proc* proc, prs_object_id_t object_id);

void prs_proc_atexit(struct prs_proc* proc, void (*function)(void));

void prs_proc_gc(void);

int prs_proc_get_argc(void);
const char* prs_proc_get_argv(int arg);

prs_bool_t prs_proc_is_user_text(void* instr_ptr);

#endif /* _PRS_PROC_H */

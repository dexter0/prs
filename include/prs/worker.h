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
 *  This file contains the worker declarations.
 */

#ifndef _PRS_WORKER_H
#define _PRS_WORKER_H

#include <prs/pal/thread.h>
#include <prs/sched.h>
#include <prs/task.h>

struct prs_worker;

/**
 * \brief
 *  Worker implementation operations. These operations must be implemented by the user of the worker.
 */
struct prs_worker_ops {
    /**
     * \brief
     *  This function is called by the worker to get the next task to execute.
     *
     *  This function has the same characteristics as \ref prs_sched_ops::get_next.
     * \see
     *  \ref prs_sched_ops::get_next
     */
    prs_bool_t                          (*get_next)(void* userdata, struct prs_task* current_task,
                                            struct prs_task** next_task);
};

/**
 * \brief
 *  Worker creation parameters.
 */
struct prs_worker_create_params {
    /** \brief PAL thread to use for this worker. */
    struct prs_pal_thread*              pal_thread;
    /** \brief Userdata passed to the functions of the worker implementation (\ref prs_worker_ops). */
    void*                               userdata;
    /** \brief Worker implementation operations. */
    struct prs_worker_ops               ops;
};

prs_result_t prs_worker_create(struct prs_worker_create_params* params, prs_worker_id_t* id);
prs_result_t prs_worker_destroy(struct prs_worker* worker);
prs_result_t prs_worker_start(struct prs_worker* worker);
prs_result_t prs_worker_stop(struct prs_worker* worker);
prs_result_t prs_worker_join(struct prs_worker* worker);
prs_result_t prs_worker_interrupt(struct prs_worker* worker);
prs_result_t prs_worker_signal(struct prs_worker* worker);

prs_bool_t prs_worker_int_disable(struct prs_worker* worker);
void prs_worker_int_enable(struct prs_worker* worker);
prs_bool_t prs_worker_int_enabled(struct prs_worker* worker);

void prs_worker_schedule(struct prs_worker* worker);

struct prs_worker* prs_worker_current(void);
struct prs_task* prs_worker_get_current_task(struct prs_worker* worker);
prs_task_id_t prs_worker_get_current_task_id(struct prs_worker* worker);
void* prs_worker_get_userdata(struct prs_worker* worker);

void prs_worker_restore_context(struct prs_worker* worker, struct prs_pal_context* context);

#endif /* _PRS_WORKER_H */

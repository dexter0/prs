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
 *  This file contains scheduler declarations.
 */

#ifndef _PRS_SCHED_H
#define _PRS_SCHED_H

#include <prs/pal/thread.h>
#include <prs/object.h>
#include <prs/task.h>
#include <prs/ticks.h>
#include <prs/types.h>

struct prs_sched;
struct prs_task;

/**
 * \brief
 *  Scheduler implementation data.
 *
 *  This structure is passed as an argument to each scheduler implementation invocation.
 */
struct prs_sched_data {
    /** \brief Private data that must be assigned by \ref prs_sched_ops::init. */
    void*                               userdata;
    /**
     * \brief
     *  List of \ref prs_sched_worker structures assigned to this scheduler.
     * \note
     *  Only one worker per scheduler is currently supported.
     */
    struct prs_dllist*                  workers;
};

/**
 * \brief
 *  This structure is instanced for each worker in the scheduler.
 */
struct prs_sched_worker {
    /** \brief Reference to the scheduler module instance. */
    struct prs_sched*                   sched;
    /** \brief Reference to the scheduler implementation data. */
    struct prs_sched_data*              sched_data;
    /** \brief Reference to the worker assigned to this scheduler. */
    struct prs_worker*                  worker;
};

/**
 * \brief
 *  Scheduler implementation operations.
 *
 *  This structure references the operations that a scheduler implementation must provide.
 */
struct prs_sched_ops {
    /**
     * \brief
     *  Initializes the scheduler implementation.
     * \param sched_data
     *  Scheduler implementation data. The \p workers list is empty at this point, and the \p userdata field should be
     *  filled with private scheduler implementation data.
     * \param userdata
     *  This is a copy of the userdata pointer specified in \ref prs_sched_create_params::userdata.
     */
    prs_result_t                        (*init)(struct prs_sched_data* sched_data, void* userdata);
    
    /**
     * \brief
     *  Unitializes the scheduler implementation.
     */
    prs_result_t                        (*uninit)(struct prs_sched_data* sched_data);

    /**
     * \brief
     *  Adds a task to the scheduler.
     * \param sched_data
     *  Scheduler implementation data.
     * \param task
     *  Task to add to the scheduler. The scheduler implementation is responsible for maintaining the list of tasks.
     *  The scheduler implementation should acquire a lock on the task so that it is not destroyed while still in its
     *  internal lists.
     */
    prs_result_t                        (*add)(struct prs_sched_data* sched_data, struct prs_task* task);
    
    /**
     * \brief
     *  Removes a task from the scheduler.
     * \param sched_data
     *  Scheduler implementation data.
     * \param task
     *  Task to remove from the scheduler. The task does not need to stop executing immediately, but it should
     *  eventually removed at the next scheduler \p get_next call.
     */
    prs_result_t                        (*remove)(struct prs_sched_data* sched_data, struct prs_task* task);

    /**
     * \brief
     *  Gets the next task to run.
     *
     *  This function implements the scheduler algorithm. Using its internal lists, it returns in the \p task
     *  parameter the next task to run. The \p current_task is the previous task that was running. The next task to run
     *  can be the same as the one already running.
     *
     *  The function must return \ref PRS_TRUE when the scheduler algorithm has taken a final decision regarding the
     *  next task to execute. When \p task is \p null, the scheduler indicates that it has no more tasks to run.
     *
     *  When the function returns \ref PRS_FALSE, it indicates that a final decision could not have taken place because
     *  a task was removed from the scheduler and the current stack must be swapped. Typically, in this case, \p task
     *  will be \p null and the stack should be swapped to the operating system's stack for this thread.
     *
     *  The scheduler implementation is free to change the \p current_task's state from \ref PRS_TASK_STATE_RUNNING to
     *  \ref PRS_TASK_STATE_READY if it must be interrupted.
     *
     *  When \p task is non-\p null, its state must be \ref PRS_TASK_STATE_RUNNING.
     *
     *  Finally, this function can handle tasks that must be removed from the scheduler.
     * \param sched_worker
     *  Worker instance for this scheduler. Scheduler implementation data is reachable through the \p sched_data field
     *  of this structure.
     * \param current_task
     *  Task currently running.
     * \param task
     *  Next task to run.
     * \return
     *  \ref PRS_TRUE if a final scheduling decision was made and the \p task field contains the next task to run.
     *  \ref PRS_FALSE if the function must be called again, running under a new stack (possibly suggested by the
     *  \p task parameter).
     */
    prs_bool_t                          (*get_next)(struct prs_sched_worker* sched_worker,
                                            struct prs_task* current_task, struct prs_task** task);

    /**
     * \brief
     *  This function is called by the scheduler module to indicate that the specified task has gone from the
     *  \ref PRS_TASK_STATE_BLOCKED state to the \ref PRS_TASK_STATE_READY state.
     *
     *  Depending on the scheduler implementation, the function may choose to interrupt the scheduler's worker in order
     *  to force a \p get_next call to change the currently running task.
     * \param sched_data
     *  Scheduler implementation data.
     * \param task
     *  Task that had its state changed.
     */
    prs_result_t                        (*ready)(struct prs_sched_data* sched_data, struct prs_task* task);
};

/**
 * \brief
 *  Scheduler module instance creation parameters.
 */
struct prs_sched_create_params {
    /** \brief Scheduler name. */
    char                                name[PRS_MAX_SCHED_NAME];
    /** \brief Userdata that will be passed on to the scheduler implementation. */
    void*                               userdata;
    /** \brief Scheduler implementation operations. */
    struct prs_sched_ops                ops;
};

prs_result_t prs_sched_create(struct prs_sched_create_params* params, prs_sched_id_t* id);
prs_result_t prs_sched_stop(prs_sched_id_t id);
prs_result_t prs_sched_destroy(prs_sched_id_t id);

prs_result_t prs_sched_start(prs_sched_id_t id);

prs_result_t prs_sched_add_thread(prs_sched_id_t id, struct prs_pal_thread* pal_thread);
prs_result_t prs_sched_remove_thread(prs_sched_id_t id, struct prs_pal_thread* pal_thread);

prs_result_t prs_sched_add_task(prs_sched_id_t id, prs_task_id_t task_id);
prs_result_t prs_sched_remove_task(prs_sched_id_t id, prs_task_id_t task_id);

void prs_sched_schedule(void);
prs_result_t prs_sched_ready(struct prs_task* task);
void prs_sched_yield(void);

void prs_sched_sleep(prs_ticks_t ticks);

void prs_sched_block(void);

prs_object_id_t prs_sched_find(const char* name);
struct prs_sched* prs_sched_find_and_lock(const char* name);

#endif /* _PRS_SCHED_H */

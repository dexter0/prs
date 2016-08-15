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
 *  This file contains the private declarations for a task control block (\ref prs_task).
 */

#ifndef _PRSP_TASK_H
#define _PRSP_TASK_H

#include <prs/pal/atomic.h>
#include <prs/config.h>
#include <prs/event.h>
#include <prs/msgq.h>
#include <prs/sched.h>
#include <prs/types.h>

/**
 * \brief
 *  The enumeration of all the possible task states.
 */
enum prs_task_state {
    /** \brief The task is not started and is waiting to be added to a scheduler. */
    PRS_TASK_STATE_STOPPED = 0,
    /** \brief The task is ready to be executed. */
    PRS_TASK_STATE_READY,
    /** \brief The task is currently executing. */
    PRS_TASK_STATE_RUNNING,
    /** \brief The task is blocked, waiting for an event to occur. */
    PRS_TASK_STATE_BLOCKED,
    /** \brief The task partly destroyed. */
    PRS_TASK_STATE_ZOMBIE
};

/**
 * \brief
 *  The task token contains its current state and a version number which is used to ensure that it can be unblocked
 *  atomically.
 */
typedef prs_uint_t prs_task_token_t;

struct prs_task {
    prs_task_id_t                       id;

    char                                name[PRS_MAX_TASK_NAME];
    void*                               userdata;

    void*                               stack;
    prs_size_t                          stack_size;

    prs_task_prio_t                     prio;

    prs_proc_id_t                       proc_id;
    prs_sched_id_t                      sched_id;

    void*                               sched_userdata;

    PRS_ATOMIC prs_task_token_t         state;

    struct prs_pal_context*             context;

    void                                (*entry)(void* userdata);

    struct prs_msgq*                    msgq;
};

enum prs_task_state prs_task_get_state(struct prs_task* task);
void prs_task_change_state(struct prs_task* task, enum prs_task_state expected_state, enum prs_task_state new_state);

prs_task_token_t prs_task_block(struct prs_task* task);
prs_bool_t prs_task_unblock(struct prs_task* task, prs_task_token_t token, prs_uint8_t cause);

prs_uint8_t prs_task_get_last_unblock_cause(struct prs_task* task);

prs_result_t prs_task_set_proc(struct prs_task* task, prs_proc_id_t proc_id);

#endif /* _PRSP_TASK_H */

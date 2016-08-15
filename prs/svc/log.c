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
 *  This file contains the definitions for the log service.
 *
 *  The log service is a single task that periodically flushes the log. The period at which the log is flushed is
 *  determined by the rate of log entries that are generated.
 */

#include <prs/assert.h>
#include <prs/error.h>
#include <prs/log.h>
#include <prs/sched.h>
#include <prs/task.h>
#include <prs/types.h>
#include <prs/worker.h>
#include <pr.h>

#include "../task.h"

static void prs_svc_log_task(void* userdata)
{
    struct prs_worker* worker = prs_worker_current();
    PRS_ASSERT(worker);

    const prs_ticks_t max_ticks_to_wait = PRS_TICKS_FROM_MS(1000);
    const prs_ticks_t min_ticks_to_wait = PRS_TICKS_FROM_MS(100);

    prs_ticks_t ticks_to_wait = min_ticks_to_wait;
    for (;;) {
        prs_worker_int_disable(worker);
        prs_sched_sleep(ticks_to_wait);
        prs_worker_int_enable(worker);

        prs_worker_int_disable(worker);
        const prs_int_t entries_written = prs_log_flush();
        prs_worker_int_enable(worker);
        const prs_int_t entries_per_second = (1000 * entries_written) / PRS_TICKS_TO_MS(ticks_to_wait);

        ticks_to_wait = PRS_TICKS_FROM_MS(1000 / (entries_per_second ? entries_per_second : 1));
        if (ticks_to_wait > max_ticks_to_wait) {
            ticks_to_wait = max_ticks_to_wait;
        }
        if (ticks_to_wait < min_ticks_to_wait) {
            ticks_to_wait = min_ticks_to_wait;
        }
    }
}

/**
 * \brief
 *  Initializes the log service task on the specified scheduler, at the specified priority.
 * \param sched_id
 *  Scheduler object ID to add the task to.
 * \param prio
 *  Priority of the log service.
 */
void prs_svc_log_init(prs_sched_id_t sched_id, prs_task_prio_t prio)
{
    struct prs_task_create_params params = {
        .name = "prs_svc_log",
        .userdata = 0,
        .stack_size = 8192,
        .prio = prio,
        .entry = prs_svc_log_task
    };
    struct prs_task* task = prs_task_create(&params);
    PRS_FATAL_WHEN(!task);

    const prs_result_t result = prs_sched_add_task(sched_id, task->id);
    PRS_FATAL_WHEN(result != PRS_OK);
}

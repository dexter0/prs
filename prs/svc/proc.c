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
 *  This file contains the definitions for the process service.
 *
 *  The process service calls the process module to dynamically load PRS executables. It also periodically runs process
 *  garbage collection.
 */

#include <prs/pal/proc.h>
#include <prs/svc/proc.msg>
#include <prs/clock.h>
#include <prs/error.h>
#include <prs/god.h>
#include <prs/log.h>
#include <prs/proc.h>
#include <prs/str.h>
#include <prs/task.h>
#include <prs/types.h>
#include <prs/worker.h>
#include <pr.h>

#include "../proc.h"

#define PRS_PROC_LOG_PREFIX             "PROC: "

union pr_msg {
    pr_msg_id_t                         id;
    struct prs_proc_msg_exec_request    exec_request;
    struct prs_proc_msg_exec_response   exec_response;
    struct prs_proc_msg_kill_request    kill_request;
    struct prs_proc_msg_kill_response   kill_response;
    struct prs_proc_msg_unreg_task_request
                                        unreg_task_request;
    struct prs_proc_msg_unreg_task_response
                                        unreg_task_response;
};

static void prs_svc_proc_task(void* userdata)
{
    struct prs_worker* worker = prs_worker_current();
    prs_ticks_t last_gc_ticks = prs_clock_get();

    for (;;) {
        const prs_ticks_t now = prs_clock_get();
        if (now - last_gc_ticks >= PRS_TICKS_FROM_SECS(1)) {
            prs_proc_gc();
            last_gc_ticks = now;
        }

        union pr_msg* msg = pr_msg_recv_timeout(PRS_TICKS_FROM_SECS(1) - (now - last_gc_ticks));
        if (!msg) {
            continue;
        }

        prs_worker_int_disable(worker);
        switch (msg->id) {
            case PRS_PROC_MSG_ID_EXEC_REQUEST: {
                struct prs_proc_msg_exec_request* req = &msg->exec_request;

                struct prs_proc_exec_params params = {
                    .main_task_params = req->main_task_params,
                    .sched_id = req->sched_id
                };
                prs_str_copy(params.filename, req->filename, PRS_MAX_PATH);
                prs_str_copy(params.cmdline, req->cmdline, PRS_MAX_CMDLINE);

                struct prs_proc* proc = prs_proc_exec(&params);
                union pr_msg* rsp =
                    pr_msg_alloc(PRS_PROC_MSG_ID_EXEC_RESPONSE, sizeof(struct prs_proc_msg_exec_response));
                if (proc) {
                    prs_log_print(PRS_PROC_LOG_PREFIX "successfully loaded '%s' as process %u",
                        params.filename, proc->id);
                    rsp->exec_response.proc_id = proc->id;
                } else {
                    prs_log_print(PRS_PROC_LOG_PREFIX "error loading process '%s'", params.filename);
                    rsp->exec_response.proc_id = (proc ? proc->id : PRS_OBJECT_ID_INVALID);
                }
                pr_msg_send(pr_msg_get_sender(msg), rsp);
                break;
            }
            case PRS_PROC_MSG_ID_KILL_REQUEST: {
                struct prs_proc_msg_kill_request* req = &msg->kill_request;
                struct prs_proc* proc = prs_god_lock(req->proc_id);
                if (proc) {
                    prs_proc_destroy(proc);
                    prs_god_unlock(req->proc_id);
                }
                if (req->respond) {
                    union pr_msg* rsp =
                        pr_msg_alloc(PRS_PROC_MSG_ID_KILL_RESPONSE, sizeof(struct prs_proc_msg_exec_response));
                    rsp->kill_response.proc_id = req->proc_id;
                    pr_msg_send(pr_msg_get_sender(msg), rsp);
                }
                break;
            }
            case PRS_PROC_MSG_ID_UNREG_OBJECT_REQUEST: {
                struct prs_proc_msg_unreg_task_request* req = &msg->unreg_task_request;
                struct prs_proc* proc = prs_god_lock(req->proc_id);
                if (proc) {
                    prs_proc_unregister_object(proc, req->object_id);
                    prs_god_unlock(req->proc_id);
                }
                if (req->respond) {
                    union pr_msg* rsp =
                        pr_msg_alloc(PRS_PROC_MSG_ID_UNREG_OBJECT_RESPONSE, sizeof(struct prs_proc_msg_exec_response));
                    rsp->unreg_task_response.proc_id = req->proc_id;
                    rsp->unreg_task_response.object_id = req->object_id;
                    pr_msg_send(pr_msg_get_sender(msg), rsp);
                }
                break;
            }
            default:
                PRS_ERROR("Unknown message ID");
                break;
        }
        prs_worker_int_enable(worker);

        pr_msg_free(msg);
    }
}

/**
 * \brief
 *  Initializes the process service task on the specified scheduler, at the specified priority.
 * \param sched_id
 *  Scheduler object ID to add the task to.
 * \param prio
 *  Priority of the process service.
 */
void prs_svc_proc_init(prs_sched_id_t sched_id, prs_task_prio_t prio)
{
    struct prs_task_create_params params = {
        .name = "prs_svc_proc",
        .userdata = 0,
        .stack_size = 16384,
        .prio = prio,
        .entry = prs_svc_proc_task
    };
    struct prs_task* task = prs_task_create(&params);
    PRS_FATAL_WHEN(!task);

    const prs_result_t result = prs_sched_add_task(sched_id, prs_task_get_id(task));
    PRS_FATAL_WHEN(result != PRS_OK);
}

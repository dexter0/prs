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
 *  This file contains the message definitions for the process service.
 */

#ifndef _PRS_SVC_PROC_MSG
#define _PRS_SVC_PROC_MSG

#include <string.h>

#include <prs/pal/inline.h>
#include <prs/assert.h>
#include <prs/config.h>
#include <prs/task.h>
#include <pr_msg.h>
#include <pr.h>

#define PRS_PROC_SVC_NAME               "prs_svc_proc"

#define PRS_PROC_MSG_ID_EXEC_REQUEST    PR_ID_CREATE(PR_ID_CLASS_PR, PR_ID_SVC_PROC, 0)
#define PRS_PROC_MSG_ID_EXEC_RESPONSE   PR_ID_CREATE(PR_ID_CLASS_PR, PR_ID_SVC_PROC, 1)
#define PRS_PROC_MSG_ID_KILL_REQUEST    PR_ID_CREATE(PR_ID_CLASS_PR, PR_ID_SVC_PROC, 2)
#define PRS_PROC_MSG_ID_KILL_RESPONSE   PR_ID_CREATE(PR_ID_CLASS_PR, PR_ID_SVC_PROC, 3)
#define PRS_PROC_MSG_ID_UNREG_OBJECT_REQUEST \
                                        PR_ID_CREATE(PR_ID_CLASS_PR, PR_ID_SVC_PROC, 4)
#define PRS_PROC_MSG_ID_UNREG_OBJECT_RESPONSE \
                                        PR_ID_CREATE(PR_ID_CLASS_PR, PR_ID_SVC_PROC, 5)

struct prs_proc_msg_exec_request {
    pr_msg_id_t                         id;
    char                                filename[PRS_MAX_PATH];
    char                                cmdline[PRS_MAX_CMDLINE];
    struct prs_task_create_params       main_task_params;
    pr_sched_id_t                       sched_id;
};

struct prs_proc_msg_exec_response {
    pr_msg_id_t                         id;
    pr_proc_id_t                        proc_id;
};

struct prs_proc_msg_kill_request {
    pr_msg_id_t                         id;
    pr_proc_id_t                        proc_id;
    prs_bool_t                          respond;
};

struct prs_proc_msg_kill_response {
    pr_msg_id_t                         id;
    pr_proc_id_t                        proc_id;
};

struct prs_proc_msg_unreg_task_request {
    pr_msg_id_t                         id;
    pr_proc_id_t                        proc_id;
    prs_object_id_t                     object_id;
    prs_bool_t                          respond;
};

struct prs_proc_msg_unreg_task_response {
    pr_msg_id_t                         id;
    pr_proc_id_t                        proc_id;
    prs_object_id_t                     object_id;
};

/**
 * \brief
 *  Sends a request to the process service to execute the specified process and waits for the response.
 * \param proc_task_id
 *  Task object ID of the process service.
 * \param filename
 *  Path of the dynamically loaded PRS executable.
 * \param cmdline
 *  Command line parameters of the executable.
 * \param task_params
 *  Task parameters for the initial task that runs the executable.
 * \param sched_id
 *  Scheduler object ID that will run the initial task.
 * \return
 *  The process object ID of the new process, or PRS_OBJECT_ID_INVALID if the process couldn't be loaded.
 */
static PRS_INLINE pr_proc_id_t prs_proc_send_exec(pr_task_id_t proc_task_id, const char* filename, const char* cmdline, struct prs_task_create_params* task_params, pr_sched_id_t sched_id)
{
    struct prs_proc_msg_exec_request* req =
        (struct prs_proc_msg_exec_request*)pr_msg_alloc(PRS_PROC_MSG_ID_EXEC_REQUEST, sizeof(*req));
    strcpy(req->filename, filename);
    strcpy(req->cmdline, cmdline);
    memcpy(&req->main_task_params, task_params, sizeof(req->main_task_params));
    req->sched_id = sched_id;
    pr_msg_send(proc_task_id, (union pr_msg*)req);
    pr_msg_id_t filter[] = {1, PRS_PROC_MSG_ID_EXEC_RESPONSE};
    struct prs_proc_msg_exec_response* rsp = (struct prs_proc_msg_exec_response*)pr_msg_recv_filter(filter);
    const pr_proc_id_t proc_id = rsp->proc_id;
    pr_msg_free((union pr_msg*)rsp);
    return proc_id;
}

/**
 * \brief
 *  Sends a request to the process service to destroy the specified process and optionally waits for the response.
 * \param proc_task_id
 *  Task object ID of the process service.
 * \param proc_id
 *  Process object ID of the process to destroy.
 * \param block
 *  If this call must wait for the process service response.
 * \note
 *  This function should not be called directly by user code.
 */
static PRS_INLINE void prs_proc_send_kill(pr_task_id_t proc_task_id, pr_proc_id_t proc_id, prs_bool_t block)
{
    struct prs_proc_msg_kill_request* req =
        (struct prs_proc_msg_kill_request*)pr_msg_alloc(PRS_PROC_MSG_ID_KILL_REQUEST, sizeof(*req));
    req->proc_id = proc_id;
    req->respond = block;
    pr_msg_send(proc_task_id, (union pr_msg*)req);
    if (block) {
        pr_msg_id_t filter[] = {1, PRS_PROC_MSG_ID_KILL_RESPONSE};
        struct prs_proc_msg_kill_response* rsp = (struct prs_proc_msg_kill_response*)pr_msg_recv_filter(filter);
        PRS_ASSERT(rsp->proc_id == proc_id);
        pr_msg_free((union pr_msg*)rsp);
    }
}

/**
 * \brief
 *  Sends a request to the process service to unregister an object from the specified process and optionally waits for
 *  the response.
 * \param proc_task_id
 *  Task object ID of the process service.
 * \param proc_id
 *  Process object ID of the process to unregister from.
 * \param object_id
 *  Object ID to unregister.
 * \param block
 *  If this call must wait for the process service response.
 * \note
 *  While registering objects to the process is a thread-safe operation, removing objects is not, hence this call. This
 *  function should not be called directly by user code.
 */
static PRS_INLINE void prs_proc_send_unreg_object(pr_task_id_t proc_task_id, pr_proc_id_t proc_id, prs_object_id_t object_id, prs_bool_t block)
{
    struct prs_proc_msg_unreg_task_request* req =
        (struct prs_proc_msg_unreg_task_request*)pr_msg_alloc(PRS_PROC_MSG_ID_UNREG_OBJECT_REQUEST, sizeof(*req));
    req->proc_id = proc_id;
    req->object_id = object_id;
    req->respond = block;
    pr_msg_send(proc_task_id, (union pr_msg*)req);
    if (block) {
        pr_msg_id_t filter[] = {1, PRS_PROC_MSG_ID_UNREG_OBJECT_RESPONSE};
        struct prs_proc_msg_unreg_task_response* rsp = (struct prs_proc_msg_unreg_task_response*)pr_msg_recv_filter(filter);
        PRS_ASSERT(rsp->proc_id == proc_id);
        PRS_ASSERT(rsp->object_id == object_id);
        pr_msg_free((union pr_msg*)rsp);
    }
}

#endif /* _PRS_SVC_PROC_MSG */

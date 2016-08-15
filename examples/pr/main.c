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

#include <stdio.h>

#include <prs/svc/proc.msg>
#include <pr.h>

#define PARENT_TASK_NAME                "init2"

union pr_msg {
    pr_msg_id_t                         id;
};

void task_entry(void* userdata)
{
    const pr_task_id_t parent_id = pr_task_find(PARENT_TASK_NAME);
    PR_FATAL_WHEN(!parent_id);
    
    union pr_msg* msg = pr_msg_alloc(0, sizeof(*msg));
    pr_log("process %u sending message to parent %u", pr_task_get_current(), parent_id);
    pr_msg_send(parent_id, msg);
}

static int main_recursive(int argc, char* argv[])
{
    struct pr_task_create_params params = {
        .userdata = 0,
        .stack_size = 16384,
        .prio = 10,
        .entry = task_entry,
        .sched_id = pr_sched_get_current()
    };
    strcpy(params.name, argv[1]);
    const pr_task_id_t task_id = pr_task_create(&params);
    PR_FATAL_WHEN(!task_id);

    pr_log("%s: created process %u", argv[1], task_id);
    
    /* Prevent this process from exiting immediately */
    pr_sleep_ms(10000);

    return 0;
}

static void spawn_proc(const char* path, const char* taskname)
{
    const pr_task_id_t proc_task_id = pr_task_find(PRS_PROC_SVC_NAME);
    PR_FATAL_WHEN(!proc_task_id);
    
    struct prs_task_create_params child_task_params = {
        .userdata = 0,
        .stack_size = 16384,
        .prio = 20
    };
    
    const pr_int_flag_t flag = pr_int_disable();
    strcpy(child_task_params.name, taskname);
    char cmdline[PRS_MAX_PATH];
    sprintf(cmdline, "%s %s", path, taskname);
    if (flag) {
        pr_int_enable();
    }
    
    const pr_proc_id_t proc_id = prs_proc_send_exec(proc_task_id, path, cmdline, &child_task_params, pr_sched_get_current());
    PR_FATAL_WHEN(!proc_id);
}

int pr_main(int argc, char* argv[])
{
    if (argc > 1) {
        return main_recursive(argc, argv);
    }
    
    for (int i = 0; i < 4; ++i) {
        char taskname[128];
        const pr_int_flag_t flag = pr_int_disable();
        sprintf(taskname, "child%d", i);
        if (flag) {
            pr_int_enable();
        }
        spawn_proc(argv[0], taskname);
    }
    
    for (;;) {
        union pr_msg* msg = pr_msg_recv_timeout(100);
        if (!msg) {
            break;
        }
        pr_log("main: received message from task %u", pr_msg_get_sender(msg));
        pr_msg_free(msg);
    }
    
    pr_log("main: exiting in 1 second");
    
    pr_sleep_ms(1000);
    
    pr_log("main: exiting now");
    
    pr_system_exit(0);
    
    return 0;
}

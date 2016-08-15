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
 *  This file contains the PR API definitions.
 */

#include <stddef.h>
#include <string.h>

#include <prs/pal/malloc.h>
#include <prs/svc/proc.msg>
#include <prs/assert.h>
#include <prs/clock.h>
#include <prs/config.h>
#include <prs/error.h>
#include <prs/god.h>
#include <prs/init.h>
#include <prs/log.h>
#include <prs/msg.h>
#include <prs/proc.h>
#include <prs/rtc.h>
#include <prs/sched.h>
#include <prs/sem.h>
#include <prs/str.h>
#include <prs/systeminfo.h>
#include <prs/worker.h>
#include <pr.h>

#include "task.h"

#define PR_INT_DISABLE()                \
    struct prs_worker* _pr_worker_current = prs_worker_current(); \
    const prs_bool_t _pr_int_disabled = (_pr_worker_current ? prs_worker_int_disable(_pr_worker_current) : PRS_FALSE);
#define PR_INT_ENABLE()                 do { if (_pr_int_disabled) { prs_worker_int_enable(_pr_worker_current); } } while (0);

union pr_msg {
    pr_msg_id_t                         id;
};

static struct prs_worker* pr_get_current_worker(void)
{
    struct prs_worker* worker = prs_worker_current();
    PRS_FATAL_WHEN(!worker);
    return worker;
}

static struct prs_task* pr_get_current_task(void)
{
    struct prs_worker* worker = prs_worker_current();
    PRS_FATAL_WHEN(!worker);
    struct prs_task* task = prs_worker_get_current_task(worker);
    PRS_FATAL_WHEN(!task);
    return task;
}

PR_EXPORT pr_int_flag_t pr_int_disable(void)
{
    return (pr_int_flag_t)prs_worker_int_disable(pr_get_current_worker());
}

PR_EXPORT void pr_int_enable(void)
{
    prs_worker_int_enable(pr_get_current_worker());
}

PR_EXPORT pr_ticks_t pr_ticks_get(void)
{
    return prs_clock_get();
}

PR_EXPORT pr_ticks_t pr_ticks_per_second(void)
{
    return PRS_HZ;
}

PR_EXPORT void* pr_malloc(prs_size_t size)
{
    PR_INT_DISABLE();
    void* result = prs_pal_malloc(size);
    PR_INT_ENABLE();
    return result;
}

PR_EXPORT void pr_free(void* ptr)
{
    PR_INT_DISABLE();
    prs_pal_free(ptr);
    PR_INT_ENABLE();
}

PR_EXPORT void* pr_malloc_global(prs_size_t size)
{
    return pr_malloc(size);
}

PR_EXPORT void pr_free_global(void* ptr)
{
    pr_free(ptr);
}

PR_EXPORT void pr_log(const char* fmt, ...)
{
    PR_INT_DISABLE();
    va_list va;
    va_start(va, fmt);
    prs_log_vprint(0, 0, 0, fmt, va);
    va_end(va);
    PR_INT_ENABLE();
}

PR_EXPORT pr_sched_id_t pr_sched_get_current(void)
{
    struct prs_task* task = pr_get_current_task();
    return task->sched_id;
}

PR_EXPORT pr_sched_id_t pr_sched_find(const char* name)
{
    PR_INT_DISABLE();
    const prs_object_id_t object_id = prs_sched_find(name);
    PR_INT_ENABLE();
    return object_id;
}

PR_EXPORT pr_task_id_t pr_task_get_current(void)
{
    struct prs_worker* worker = pr_get_current_worker();
    PRS_ASSERT(worker);
    struct prs_task* task = prs_worker_get_current_task(worker);
    PRS_ASSERT(task);
    return task->id;
}

PR_EXPORT pr_result_t pr_task_get_prio(pr_task_id_t task_id, pr_task_prio_t* prio)
{
    PR_INT_DISABLE();
    struct prs_task* task = prs_god_lock(task_id);
    if (!task) {
        PRS_ERROR("Task not found");
        PR_INT_ENABLE();
        return PR_NOT_FOUND;
    }
    *prio = prs_task_get_prio(task);
    prs_god_unlock(task_id);
    PR_INT_ENABLE();
    return PR_OK;
}

PR_EXPORT pr_result_t pr_task_set_prio(pr_task_id_t task_id, pr_task_prio_t prio)
{
    PR_INT_DISABLE();
    struct prs_task* task = prs_god_lock(task_id);
    if (!task) {
        PRS_ERROR("Task not found");
        PR_INT_ENABLE();
        return PR_NOT_FOUND;
    }
    prs_task_set_prio(task, prio);
    prs_god_unlock(task_id);
    PR_INT_ENABLE();
    return PR_OK;
}

PR_EXPORT prs_size_t pr_task_get_stack_size(pr_task_id_t task_id)
{
    PR_INT_DISABLE();
    struct prs_task* task = prs_god_lock(task_id);
    if (!task) {
        PRS_ERROR("Task not found");
        PR_INT_ENABLE();
        return 0;
    }
    const prs_size_t stack_size = task->stack_size;
    prs_god_unlock(task_id);
    PR_INT_ENABLE();
    return stack_size;
}

pr_task_id_t pr_task_create(struct pr_task_create_params* task_create_params)
{
    struct prs_task_create_params params = {
        .userdata = task_create_params->userdata,
        .stack_size = task_create_params->stack_size,
        .prio = task_create_params->prio,
        .entry = task_create_params->entry
    };
    prs_str_copy(params.name, task_create_params->name, sizeof(params.name));
    PR_INT_DISABLE();
    struct prs_task* task = prs_task_create(&params);
    pr_task_id_t id = PRS_OBJECT_ID_INVALID;
    if (task) {
        prs_sched_add_task(task_create_params->sched_id, task->id);
        id = task->id;
    }
    PR_INT_ENABLE();
    return id;
}

PR_EXPORT pr_task_id_t pr_task_find(const char* name)
{
    PR_INT_DISABLE();
    const prs_object_id_t object_id = prs_task_find(name);
    PR_INT_ENABLE();
    return object_id;
}

PR_EXPORT union pr_msg* pr_msg_alloc(pr_msg_id_t msg_id, prs_size_t size)
{
    struct prs_task* task = pr_get_current_task();

    PR_INT_DISABLE();

    struct prs_msg* pmsg = pr_malloc_global(PRS_MSG_OVERHEAD + size);
    PRS_FATAL_WHEN(!pmsg);

    memset(&pmsg->node, 0, sizeof(pmsg->node));
    pmsg->owner = task->id;
    pmsg->sender = task->id;

    pr_msg_id_t* msg = (pr_msg_id_t*)pmsg->data;
    *msg = msg_id;

    PR_INT_ENABLE();

    return (union pr_msg*)msg;
}

PR_EXPORT void pr_msg_free(union pr_msg* msg)
{
    PR_INT_DISABLE();
    pr_free_global(PRS_MSG_FROM_DATA(msg));
    PR_INT_ENABLE();
}

PR_EXPORT void pr_msg_send(pr_task_id_t task_id, union pr_msg* msg)
{
    struct prs_msg* pmsg = PRS_MSG_FROM_DATA(msg);
    PR_INT_DISABLE();
    struct prs_task* task = prs_god_lock(task_id);
    if (!task) {
        prs_log_print("pr_msg_send(): task %u not found", task_id);
        PRS_ERROR("Task not found");
        pr_msg_free(msg);
        PR_INT_ENABLE();
        return;
    }

    pmsg->owner = task->id;
    pmsg->sender = pr_get_current_task()->id;
    prs_msgq_send(task->msgq, pmsg);
    prs_god_unlock(task_id);
    PR_INT_ENABLE();
}

PR_EXPORT union pr_msg* pr_msg_recv(void)
{
    struct prs_task* task = pr_get_current_task();
    PR_INT_DISABLE();
    struct prs_msg* pmsg = prs_msgq_recv(task->msgq);
    PR_INT_ENABLE();
    return (union pr_msg*)pmsg->data;
}

static prs_bool_t pr_msgq_filter_function(void* userdata, struct prs_msg* msg)
{
    pr_msg_id_t* filter = userdata;
    pr_msg_id_t* msg_id_ptr = (pr_msg_id_t*)msg->data;
    const pr_msg_id_t msg_id = *msg_id_ptr; /* GCC complains if this is done in a single statement */

    const pr_msg_id_t count = *filter;
    PRS_ASSERT(count > 0);
    PRS_ASSERT(count < 256);
    for (pr_msg_id_t i = 1; i <= count; ++i) {
        if (filter[i] == msg_id) {
            return PRS_TRUE;
        }
    }
    return PRS_FALSE;
}

PR_EXPORT union pr_msg* pr_msg_recv_filter(pr_msg_id_t* filter)
{
    PRS_KILL_TASK_WHEN(!filter);
    PRS_KILL_TASK_WHEN(*filter > 16);
    struct prs_task* task = pr_get_current_task();
    PR_INT_DISABLE();
    struct prs_msg* pmsg = prs_msgq_recv_filter(task->msgq, filter, (*filter + 1) * sizeof(*filter), pr_msgq_filter_function);
    PR_INT_ENABLE();
    return (union pr_msg*)pmsg->data;
}

PR_EXPORT union pr_msg* pr_msg_recv_timeout(pr_ticks_t ticks)
{
    struct prs_task* task = pr_get_current_task();
    PR_INT_DISABLE();
    struct prs_msg* pmsg = prs_msgq_recv_timeout(task->msgq, ticks);
    union pr_msg* msg = 0;
    if (pmsg) {
        msg = (union pr_msg*)pmsg->data;
    }
    PR_INT_ENABLE();
    return msg;
}

PR_EXPORT union pr_msg* pr_msg_recv_filter_timeout(pr_msg_id_t* filter, pr_ticks_t ticks)
{
    PRS_KILL_TASK_WHEN(!filter);
    PRS_KILL_TASK_WHEN(*filter > 16);
    struct prs_task* task = pr_get_current_task();
    PR_INT_DISABLE();
    struct prs_msg* pmsg = prs_msgq_recv_filter_timeout(task->msgq, filter, (*filter + 1) * sizeof(*filter), pr_msgq_filter_function, ticks);
    union pr_msg* msg = 0;
    if (pmsg) {
        msg = (union pr_msg*)pmsg->data;
    }
    PR_INT_ENABLE();
    return msg;
}

PR_EXPORT pr_task_id_t pr_msg_get_sender(union pr_msg* msg)
{
    struct prs_msg* pmsg = PRS_MSG_FROM_DATA(msg);
    return pmsg->sender;
}

PR_EXPORT pr_sem_id_t pr_sem_create(struct pr_sem_create_params* params)
{
    struct prs_sem_create_params sem_create_params = {
        .max_count = params->max_count,
        .initial_count = params->initial_count
    };
    PR_INT_DISABLE();
    struct prs_sem* sem = prs_sem_create(&sem_create_params);
    PRS_ASSERT(sem);
    PR_INT_ENABLE();
    return *(pr_sem_id_t*)sem;
}

PR_EXPORT void pr_sem_destroy(pr_sem_id_t sem_id)
{
    PR_INT_DISABLE();
    struct prs_sem* sem = prs_god_lock(sem_id);
    if (sem) {
        prs_sem_destroy(sem);
        prs_god_unlock(sem_id);
    }
    PR_INT_ENABLE();
}

PR_EXPORT void pr_sem_wait(pr_sem_id_t sem_id)
{
    PR_INT_DISABLE();
    struct prs_sem* sem = prs_god_lock(sem_id);
    if (sem) {
        prs_sem_wait(sem);
        prs_god_unlock(sem_id);
    }
    PR_INT_ENABLE();
}

PR_EXPORT pr_result_t pr_sem_wait_timeout(pr_sem_id_t sem_id, prs_ticks_t timeout)
{
    PR_INT_DISABLE();
    struct prs_sem* sem = prs_god_lock(sem_id);
    prs_result_t result = PR_NOT_FOUND;
    if (sem) {
        result = prs_sem_wait_timeout(sem, timeout);
        prs_god_unlock(sem_id);
    }
    PR_INT_ENABLE();
    return result;
}

PR_EXPORT void pr_sem_signal(pr_sem_id_t sem_id)
{
    PR_INT_DISABLE();
    struct prs_sem* sem = prs_god_lock(sem_id);
    if (sem) {
        prs_sem_signal(sem);
        prs_god_unlock(sem_id);
    }
    PR_INT_ENABLE();
}

PR_EXPORT void pr_yield(void)
{
    PR_INT_DISABLE();
    prs_sched_yield();
    PR_INT_ENABLE();
}

PR_EXPORT void pr_stop(void)
{
    PR_INT_DISABLE();
    prs_sched_block();
    PR_INT_ENABLE();
}

PR_EXPORT void pr_sleep_ms(int ms)
{
    pr_sleep_ticks(PRS_TICKS_FROM_MS(ms));
}

PR_EXPORT void pr_sleep_us(int us)
{
    pr_sleep_ticks(PRS_TICKS_FROM_US(us));
}

PR_EXPORT void pr_sleep_ticks(pr_ticks_t ticks)
{
    PR_INT_DISABLE();
    prs_sched_sleep(ticks);
    PR_INT_ENABLE();
}

PR_EXPORT void pr_error(enum pr_error_type error, const char* expr, const char* file, int line)
{
    PR_INT_DISABLE();
    enum prs_error_type e;
    switch (error) {
        case PR_ERROR_TYPE_CONTINUE:
            e = PRS_ERROR_TYPE_CONTINUE;
            break;
        case PR_ERROR_TYPE_KILL_TASK:
            e = PRS_ERROR_TYPE_KILL_TASK;
            break;
        case PR_ERROR_TYPE_FATAL:
            e = PRS_ERROR_TYPE_FATAL;
            break;
        default:
            e = PRS_ERROR_TYPE_FATAL; /* Wmaybe-uninitialized */
            PRS_KILL_TASK("Unknown error type");
    }
    prs_error(e, expr, file, line);
    PR_INT_ENABLE();
}

PR_EXPORT void pr_exit(int status)
{
    PR_INT_DISABLE();
    struct prs_task* task = pr_get_current_task();
    PRS_ASSERT(task);
    PRS_FTRACE("status %d from task %s, process id %u", status, task->name, task->proc_id);
    const prs_task_id_t proc_task_id = prs_task_find(PRS_PROC_SVC_NAME);
    PRS_ASSERT(proc_task_id);
    prs_proc_send_kill(proc_task_id, task->proc_id, PRS_TRUE);
    prs_task_destroy(task); /* Should not get here */
    PR_INT_ENABLE();
}

PR_EXPORT int pr_atexit(void (*function)(void))
{
    PR_INT_DISABLE();
    struct prs_task* task = pr_get_current_task();
    PRS_ASSERT(task);
    struct prs_proc* proc = prs_god_lock(task->proc_id);
    if (proc) {
        prs_proc_atexit(proc, function);
        prs_god_unlock(task->proc_id);
    }
    PR_INT_ENABLE();
    return -1;
}

PR_EXPORT void pr_system_exit(int status)
{
    PR_INT_DISABLE();
    prs_exit(status);
    PR_INT_ENABLE();
}

PR_EXPORT struct prs_systeminfo* pr_systeminfo_get(void)
{
    PR_INT_DISABLE();
    struct prs_systeminfo* systeminfo = prs_systeminfo_get();
    PR_INT_ENABLE();
    return systeminfo;
}

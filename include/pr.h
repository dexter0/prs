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
 *  This file contains the PR API declarations.
 *
 *  The PR API is the default API used by dynamically loaded PRS executables to make use of the PRS core.
 */

#ifndef _PR_H
#define _PR_H

#include <prs/pal/lib.h>
#include <prs/config.h>
#include <prs/types.h>

/**
 * \brief
 *  The CRT entry point for dynamically loaded executables.
 */
#define PR_ENTRYPOINT                   _pr_entrypoint

/**
 * \brief
 *  This macro specifies that a function can be used by dynamically loaded PRS executables.
 */
#if defined(PR_APP)
#define PR_EXPORT                       PRS_PAL_LIB_IMPORT
#else
#define PR_EXPORT                       PRS_PAL_LIB_EXPORT
#endif

/**
 * \brief
 *  This enumeration contains all the possible result values.
 */
typedef enum pr_result {
    PR_OK = 0,
    PR_UNKNOWN,
    PR_NOT_IMPLEMENTED,
    PR_OUT_OF_MEMORY,
    PR_PLATFORM_ERROR,
    PR_INVALID_STATE,
    PR_NOT_FOUND,
    PR_ALREADY_EXISTS,
    PR_EMPTY,
    PR_LOCKED,
    PR_TIMEOUT
} pr_result_t;

/**
 * \brief
 *  This is the previous value of the interrupt flags as returned by \ref pr_int_disable. When non-zero,
 *  \ref pr_int_enable should be called to restore the previous mode.
 */
typedef int pr_int_flag_t;

/**
 * \brief
 *  From this point, prevent interruptions from occurring. If the interruptions were already disabled, this returns
 *  zero.
 */
PR_EXPORT pr_int_flag_t pr_int_disable(void);

/**
 * \brief
 *  Enable interruptions.
 */
PR_EXPORT void pr_int_enable(void);

/**
 * \brief
 *  Type that contains elapsed ticks since PRS was started.
 */
typedef PRS_TICKS_TYPE pr_ticks_t;

/**
 * \brief
 *  Returns the number of ticks elapsed since PRS was started.
 */
PR_EXPORT pr_ticks_t pr_ticks_get(void);

/**
 * \brief
 *  Returns the number of ticks per second.
 */
PR_EXPORT pr_ticks_t pr_ticks_per_second(void);

/**
 * \brief
 *  Allocate process memory. This is memory that should not be accessed by other dynamically loaded PRS executables.
 */
PR_EXPORT void* pr_malloc(prs_size_t size);

/**
 * \brief
 *  Free process memory.
 */
PR_EXPORT void pr_free(void* ptr);

/**
 * \brief
 *  Allocate global memory. This is memory that can be used globally.
 */
PR_EXPORT void* pr_malloc_global(prs_size_t size);

/**
 * \brief
 *  Free global memory.
 */
PR_EXPORT void pr_free_global(void* ptr);

/**
 * \brief
 *  Print the specified text to the log.
 * \param fmt
 *  Format string.
 * \param ...
 *  Variable arguments.
 */
PR_EXPORT void pr_log(const char* fmt, ...);

/**
 * \brief
 *  Scheduler object ID.
 */
typedef prs_object_id_t pr_sched_id_t;

/**
 * \brief
 *  Returns the scheduler object ID that is running the current task.
 */
PR_EXPORT pr_sched_id_t pr_sched_get_current(void);

/**
 * \brief
 *  Task object ID.
 */
typedef prs_object_id_t pr_task_id_t;

/**
 * \brief
 *  Task priority type.
 */
typedef PRS_TASK_PRIO_TYPE pr_task_prio_t;

/**
 * \brief
 *  Task creation parameters.
 */
struct pr_task_create_params {
    /** \brief The task name that can be used to locate the task with \ref pr_task_find. */
    char                                name[PRS_MAX_TASK_NAME];
    /** \brief Data that is passed as a parameter to the entry point of the task. */
    void*                               userdata;
    /** \brief Initial stack size of the task. */
    prs_size_t                          stack_size;
    /** \brief Priority of the task. */
    pr_task_prio_t                      prio;
    /** \brief Entry point of the task. */
    void                                (*entry)(void* userdata);
    /** \brief Scheduler on which the task will run. */
    pr_sched_id_t                       sched_id;
};

/**
 * \brief
 *  Returns the scheduler object ID corresponding to the specified name.
 */
PR_EXPORT pr_sched_id_t pr_sched_find(const char* name);

/**
 * \brief
 *  Returns the currently executing task.
 */
PR_EXPORT pr_task_id_t pr_task_get_current(void);

/**
 * \brief
 *  Gets the priority of the specified task.
 */
PR_EXPORT pr_result_t pr_task_get_prio(pr_task_id_t task_id, pr_task_prio_t* prio);

/**
 * \brief
 *  Sets the priority of the specified task.
 * \note
 *  It is not possible to set the priority of a task other than the currently executing task.
 */
PR_EXPORT pr_result_t pr_task_set_prio(pr_task_id_t task_id, pr_task_prio_t prio);

/**
 * \brief
 *  Returns the specified task's stack size.
 */
PR_EXPORT prs_size_t pr_task_get_stack_size(pr_task_id_t task_id);

/**
 * \brief
 *  Create the task as specified by the parameters.
 * \param task_create_params
 *  The task's parameters.
 * \return
 *  Returns the task object ID of the created task, or zero if the task creation failed.
 */
PR_EXPORT pr_task_id_t pr_task_create(struct pr_task_create_params* task_create_params);

/**
 * \brief
 *  Returns the task object ID corresponding to the specified name.
 */
PR_EXPORT pr_task_id_t pr_task_find(const char* name);

/**
 * \brief
 *  Process object ID.
 */
typedef prs_object_id_t pr_proc_id_t;

/**
 * \brief
 *  Message ID.
 */
typedef prs_uint32_t pr_msg_id_t;

/**
 * \brief
 *  Message union. The first field of the union is always a \ref pr_msg_id_t.
 */
union pr_msg;

/**
 * \brief
 *  Allocate a message.
 * \param msg_id
 *  ID of the message.
 * \param size
 *  Size of the message.
 * \return
 *  The allocated message.
 */
PR_EXPORT union pr_msg* pr_msg_alloc(pr_msg_id_t msg_id, prs_size_t size);

/**
 * \brief
 *  Free the message.
 */
PR_EXPORT void pr_msg_free(union pr_msg* msg);

/**
 * \brief
 *  Send the message to the specified task.
 * \param task_id
 *  Task object ID specifying the task to send the message to.
 * \param msg
 *  Message to send.
 */
PR_EXPORT void pr_msg_send(pr_task_id_t task_id, union pr_msg* msg);

/**
 * \brief
 *  Receive a message from the currently executing task's message queue. If no message is currently waiting in the
 *  queue, wait until a message is sent from another task.
 * \return
 *  The received message.
 */
PR_EXPORT union pr_msg* pr_msg_recv(void);

/**
 * \brief
 *  Receive a message from the currently executing task's message queue using the specified filter. If no message is
 *  currently waiting in the queue, wait until a message is sent from another task.
 * \param filter
 *  Filter to use.
 * \return
 *  The received message.
 */
PR_EXPORT union pr_msg* pr_msg_recv_filter(pr_msg_id_t* filter);

/**
 * \brief
 *  Receive a message from the currently executing task's message queue. If no message is currently waiting in the
 *  queue, wait for the specified time until a message is sent from another task.
 * \param ticks
 *  Number of ticks to wait for a message to be received.
 * \return
 *  The received message.
 */
PR_EXPORT union pr_msg* pr_msg_recv_timeout(pr_ticks_t ticks);

/**
 * \brief
 *  Receive a message from the currently executing task's message queue using the specified filter. If no message is
 *  currently waiting in the queue, wait for the specified time until a message is sent from another task.
 * \param filter
 *  Filter to use.
 * \param ticks
 *  Number of ticks to wait for a message to be received.
 * \return
 *  The received message.
 */
PR_EXPORT union pr_msg* pr_msg_recv_filter_timeout(pr_msg_id_t* filter, pr_ticks_t ticks);

/**
 * \brief
 *  Returns the task object ID of the last task that sent the specified message.
 */
PR_EXPORT pr_task_id_t pr_msg_get_sender(union pr_msg* msg);

/**
 * \brief
 *  Semaphore creation parameters.
 */
struct pr_sem_create_params {
    /** \brief The semaphore's maximum count. */
    prs_int_t                           max_count;
    /** \brief The semaphore's initial count. */
    prs_int_t                           initial_count;
};

/**
 * \brief
 *  Semaphore object ID.
 */
typedef prs_object_id_t pr_sem_id_t;

/**
 * \brief
 *  Create the sempahore as specified by the parameters.
 * \param params
 *  The semaphore's parameters.
 * \return
 *  Returns the semaphore object ID of the created task, or zero if the semaphore creation failed.
 */
PR_EXPORT pr_sem_id_t pr_sem_create(struct pr_sem_create_params* params);

/**
 * \brief
 *  Destroy the semaphore.
 */
PR_EXPORT void pr_sem_destroy(pr_sem_id_t sem_id);

/**
 * \brief
 *  Decrements the semaphore. If the semaphore count is below zero, wait in queue for the count to increment.
 * \param sem_id
 *  Semaphore object ID that specifies the semaphore to wait for.
 */
PR_EXPORT void pr_sem_wait(pr_sem_id_t sem_id);

/**
 * \brief
 *  Decrements the semaphore. If the semaphore count is below zero, wait in queue for the count to increment or for
 *  the specified timeout to occur.
 * \param sem_id
 *  Semaphore object ID that specifies the semaphore to wait for.
 * \param timeout
 *  Time to wait, in ticks.
 * \return
 *  \ref PR_TIMEOUT if the timeout occurred.
 *  \ref PR_OK if the semaphore was signaled before the timeout.
 */
PR_EXPORT pr_result_t pr_sem_wait_timeout(pr_sem_id_t sem_id, pr_ticks_t timeout);

/**
 * \brief
 *  Increments the semaphore and signals a waiting task if the count was negative.
 * \param sem_id
 *  Semaphore object ID that specifies the semaphore to signal.
 */
PR_EXPORT void pr_sem_signal(pr_sem_id_t sem_id);

/**
 * \brief
 *  Yield the current task so that the scheduler can choose another one to execute (if need be).
 */
PR_EXPORT void pr_yield(void);

/**
 * \brief
 *  Stop the current task execution.
 */
PR_EXPORT void pr_stop(void);

/**
 * \brief
 *  Stop the current task execution for the number of milliseconds specified.
 */
PR_EXPORT void pr_sleep_ms(int ms);

/**
 * \brief
 *  Stop the current task execution for the number of microseconds specified.
 */
PR_EXPORT void pr_sleep_us(int us);

/**
 * \brief
 *  Stop the current task execution for the number of ticks specified.
 */
PR_EXPORT void pr_sleep_ticks(pr_ticks_t ticks);

/**
 * \brief
 *  This enumeration contains all the possible PR error types.
 */
enum pr_error_type {
    PR_ERROR_TYPE_CONTINUE,
    PR_ERROR_TYPE_KILL_TASK,
    PR_ERROR_TYPE_FATAL
};

/**
 * \brief
 *  From the current location, reports an error in the log.
 * \param desc
 *  Description of the error.
 */
#define PR_ERROR(desc)                  pr_error(PR_ERROR_TYPE_CONTINUE, desc, __FILE__, __LINE__)

/**
 * \brief
 *  From the current location, reports an error in the log if \p cond is \p true. Must be used like an \p if statement.
 * \param cond
 *  Condition that is verified.
 */
#define PR_ERROR_IF(cond)               if ((cond) && (pr_error(PR_ERROR_TYPE_CONTINUE, #cond, __FILE__, __LINE__), 1))

/**
 * \brief
 *  From the current location, reports an error in the log when \p cond is \p true.
 * \param cond
 *  Condition that is verified.
 */
#define PR_ERROR_WHEN(cond)             do { if (cond) { pr_error(PR_ERROR_TYPE_CONTINUE, #cond, __FILE__, __LINE__); } } while (0);

/**
 * \brief
 *  From the current location, reports and error and kills the current task.
 * \param desc
 *  Description of the error.
 */
#define PR_KILL_TASK(desc)              pr_error(PR_ERROR_TYPE_KILL_TASK, desc, __FILE__, __LINE__)

/**
 * \brief
 *  From the current location, reports and error and kills the current task when \p cond is \p true.
 * \param cond
 *  Condition that is verified.
 */
#define PR_KILL_TASK_WHEN(cond)         do { if (cond) { pr_error(PR_ERROR_TYPE_KILL_TASK, #cond, __FILE__, __LINE__); } } while (0);

/**
 * \brief
 *  From the current location, reports and error and exits PRS.
 * \param desc
 *  Description of the error.
 */
#define PR_FATAL(desc)                  pr_error(PR_ERROR_TYPE_FATAL, desc, __FILE__, __LINE__)

/**
 * \brief
 *  From the current location, reports and error and exits PRS when \p cond is \p true.
 * \param cond
 *  Condition that is verified.
 */
#define PR_FATAL_WHEN(cond)             do { if (cond) { pr_error(PR_ERROR_TYPE_FATAL, #cond, __FILE__, __LINE__); } } while (0);

/**
 * \brief
 *  Stop the current task execution for the number of ticks specified.
 * \param error
 *  Behavior of the error.
 * \param expr
 *  Expression that is printed into the log.
 * \param file
 *  File where the error occurred.
 * \param line
 *  Line in \p file where the error occurred.
 * \note
 *  This function should not be called directly. Use \ref PR_ERROR, \ref PR_ERROR_IF, \ref PR_ERROR_WHEN,
 *  \ref PR_KILL_TASK, \ref PR_KILL_TASK_WHEN, \ref PR_FATAL or \ref PR_FATAL_WHEN instead.
 */
PR_EXPORT void pr_error(enum pr_error_type error, const char* expr, const char* file, int line);

/**
 * \brief
 *  Exits the currently executing dynamically loaded process.
 * \param status
 *  Exit status code.
 */
PR_EXPORT void pr_exit(int status);

/**
 * \brief
 *  Registers a callback that will be run when the currently executing process exits.
 * \param function
 *  Function to run when the process exits.
 */
PR_EXPORT int pr_atexit(void (*function)(void));

/**
 * \brief
 *  Exits PRS.
 * \param status
 *  Exit status code.
 */
PR_EXPORT void pr_system_exit(int status);

/**
 * \brief
 *  Returns the system information.
 */
PR_EXPORT struct prs_systeminfo* pr_systeminfo_get(void);

#endif /* _PR_H */

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
 *  This file contains the PRS initialization and uninitialization definitions.
 *
 *  PRS initialization is done through three distinct stages:
 *    - \p init0: This is code contained in the \ref prs_init function. This stage is responsible for:
 *       -# Fetching information about hardware, such as the number of cores and the memory page size.
 *       -# Initializing the GOD and GPD.
 *       -# Initializing the log module.
 *       -# Initializing the exception module.
 *       -# Initializing the clock module.
 *       -# Starting one scheduler per core, unless specified otherwise.
 *       -# Starting the \p init1 task in the PRS environment.
 *    - \p init1: This is code contained in the \ref prs_init1_task function in the PRS environment. This stage is
 *      responsible for:
 *       -# Starting PRS services such as the log and process services.
 *       -# Starting the \p init2 executable.
 *    - \p init2: This is the startup (main) code of the first external executable loaded by PRS. This stage runs the
 *      user application.
 */

#include <stdio.h>
#include <stdlib.h>

#include <prs/pal/atomic.h>
#include <prs/pal/exit.h>
#include <prs/pal/os.h>
#include <prs/pal/thread.h>
#include <prs/pal/wls.h>
#include <prs/sched/swcoop.h>
#include <prs/sched/swprio.h>
#include <prs/svc/log.h>
#include <prs/svc/proc.h>
#include <prs/svc/proc.msg>
#if defined(PRS_TEST)
#include <prs/svc/test.h>
#endif /* PRS_TEST */
#include <prs/clock.h>
#include <prs/config.h>
#include <prs/error.h>
#include <prs/excp.h>
#include <prs/god.h>
#include <prs/gpd.h>
#include <prs/init.h>
#include <prs/log.h>
#include <prs/proc.h>
#include <prs/sched.h>
#include <prs/str.h>
#include <prs/systeminfo.h>
#include <prs/task.h>
#include <prs/worker.h>

#include "task.h"

#define PRS_CLOCK_PRIO                  PRS_PAL_THREAD_PRIO_REALTIME
#if PRS_PAL_OS == PRS_PAL_OS_WINDOWS
#define PRS_WORKER_PRIO                 PRS_PAL_THREAD_PRIO_HIGH
#else
#define PRS_WORKER_PRIO                 PRS_PAL_THREAD_PRIO_NORMAL
#endif

//#define PRS_PRINT_OBJECTS_ON_EXIT

static prs_size_t s_prs_core_count = 0;
static prs_sched_id_t s_prs_scheduler_ids[PRS_MAX_CPU];
static PRS_ATOMIC prs_bool_t s_prs_uninit;
static PRS_ATOMIC prs_sched_id_t s_prs_main_sched_id;
static PRS_ATOMIC prs_bool_t s_prs_main_exit;
static PRS_ATOMIC int s_prs_exit_status = 0;

static void prs_uninit_final(prs_sched_id_t except_id);

static prs_sched_id_t prs_find_last_sched(void)
{
    for (int i = PRS_MAX_CPU - 1; i >= 0; --i) {
        char sched_name[PRS_MAX_SCHED_NAME];
        snprintf(sched_name, sizeof(sched_name), "swprio%d", i);
        const prs_sched_id_t sched_id = prs_sched_find(sched_name);
        if (sched_id) {
            return sched_id;
        }
    }
    return 0;
}

/**
 * \brief
 *  Continues the initialization of PRS inside a PRS scheduler and starts the \p init2 executable.
 * \param userdata
 *  Task userdata. Unused.
 */
static void prs_init1_task(void* userdata)
{
    struct prs_worker* worker = prs_worker_current();
    PRS_ASSERT(worker);

    prs_worker_int_disable(worker);
    printf("init1 task started\n");
    fflush(stdout);
    prs_worker_int_enable(worker);

    prs_svc_log_init(prs_find_last_sched(), PRS_MAX_TASK_PRIO - 1);
    prs_log_print("Log service initialized");

    prs_proc_init();
    prs_log_print("Process loader initialized");

    prs_svc_proc_init(prs_task_current()->sched_id, PRS_MAX_TASK_PRIO - 1);
    prs_log_print("Process service initialized");

#if defined(PRS_TEST)
    prs_svc_test_init(prs_task_current()->sched_id, PRS_MAX_TASK_PRIO - 1);
    prs_log_print("Test service initialized");
#endif /* PRS_TEST */

    for (int i = 0; i < prs_proc_get_argc(); ++i) {
        prs_log_print("argv[%d]: %s", i, prs_proc_get_argv(i));
    }

    const pr_task_id_t proc_task_id = prs_task_find(PRS_PROC_SVC_NAME);
    PRS_FATAL_WHEN(!proc_task_id);

    const char* filename = (prs_proc_get_argc() > 1) ? prs_proc_get_argv(1) : "init2.exe";
    const char* cmdline = filename;
    struct prs_task_create_params init2_task_params = {
        .name = "init2",
        .userdata = 0,
        .stack_size = 16384,
        .prio = PRS_MAX_TASK_PRIO - 1
    };
    const pr_proc_id_t proc_id = prs_proc_send_exec(proc_task_id, filename, cmdline, &init2_task_params,
        prs_task_current()->sched_id);
    if (!proc_id) {
        prs_log_print("Couldn't load init2 process. Exiting.");
        prs_exit(-1);
    }

    prs_worker_int_disable(worker);
    printf("init2 task started (%s)\n", filename);
    fflush(stdout);
    prs_worker_int_enable(worker);
}

/**
 * \brief
 *  Begins the initialization of PRS.
 * \param params
 *  PRS parameters.
 * \return
 *  The exit code obtained from \ref prs_exit or \ref prs_exit_from_excp.
 */
int prs_init(struct prs_init_params* params)
{
    prs_result_t result;

    prs_pal_atomic_store(&s_prs_main_sched_id, PRS_OBJECT_ID_INVALID);

    prs_pal_os_init();
    prs_wls_init();

    printf("init0 started\n");

    struct prs_god_create_params god_params = {
        .max_entries = PRS_MAX_OBJECTS
    };
    result = prs_god_create(&god_params);
    PRS_FATAL_WHEN(result != PRS_OK);

    prs_gpd_init();

    prs_log_init();

    struct prs_systeminfo* systeminfo = prs_systeminfo_get();
    prs_log_print("PRS %s %s compiled with %s %s running on %s, %s %s %s",
        systeminfo->version, systeminfo->config, systeminfo->compiler, systeminfo->compiler_version,
        systeminfo->computer, systeminfo->arch, systeminfo->os, systeminfo->os_version);

    prs_excp_init();

    /*
     * When not using the current thread for clock interrupts, we must ensure that the clock thread priority is
     * higher than the normal worker thread priority. If that's not the case, the clock interrupt code may not be
     * called consistently over time as the OS will not schedule it.
     */
    PRS_STATIC_ASSERT(PRS_WORKER_PRIO <= PRS_CLOCK_PRIO);

    struct prs_clock_init_params clock_params = {
        .use_current_thread = params->use_current_thread,
        .affinity =  1, /* Clock interrupt on first core when not using the current thread */
        .prio = PRS_CLOCK_PRIO /* Priority when not using the current thread */
    };
    prs_clock_init(&clock_params);

    s_prs_core_count = prs_pal_os_get_core_count();
    if (s_prs_core_count >= PRS_MAX_CPU) {
        prs_log_print("Warning: There are %u cores on the system, but PRS_MAX_CPU is %u.",
            s_prs_core_count, PRS_MAX_CPU);
        s_prs_core_count = PRS_MAX_CPU;
    }
    prs_log_print("Core count: %u", s_prs_core_count);

    prs_bool_t first = PRS_TRUE;
    for (int i = 0; i < s_prs_core_count; ++i) {
        if (!(params->core_mask & (1 << i))) {
            s_prs_scheduler_ids[i] = PRS_OBJECT_ID_INVALID;
            continue;
        }

        struct prs_pal_thread_create_params pal_main_thread_params = {
            .stack_size = 4096,
            .prio = PRS_WORKER_PRIO,
            .affinity = (1 << i),
            .from_current = (first ? params->use_current_thread : PRS_FALSE)
        };

        struct prs_pal_thread* pal_thread = prs_pal_thread_create(&pal_main_thread_params);
        PRS_FATAL_WHEN(!pal_thread);

        struct prs_sched_create_params sched_params = {
            .userdata = 0,
            .ops = *prs_sched_swprio_ops()
        };
        prs_str_printf(sched_params.name, sizeof(sched_params.name), "swprio%d", i);
        prs_sched_id_t sched_id;
        result = prs_sched_create(&sched_params, &sched_id);
        PRS_FATAL_WHEN(result != PRS_OK);
        s_prs_scheduler_ids[i] = sched_id;

        if (pal_main_thread_params.from_current) {
            prs_pal_atomic_store(&s_prs_main_sched_id, sched_id);
            prs_pal_atomic_store(&s_prs_main_exit, PRS_FALSE);
        }

        result = prs_sched_add_thread(sched_id, pal_thread);
        PRS_FATAL_WHEN(result != PRS_OK);

        if (first) {
            struct prs_task_create_params init1_task_params = {
                .name = "init1",
                .userdata = 0,
                .stack_size = 16384,
                .prio = PRS_MAX_TASK_PRIO - 1,
                .entry = prs_init1_task
            };
            struct prs_task* init1_task = prs_task_create(&init1_task_params);
            PRS_FATAL_WHEN(!init1_task);

            const prs_task_id_t init1_task_id = prs_task_get_id(init1_task);
            result = prs_sched_add_task(sched_id, init1_task_id);
            PRS_FATAL_WHEN(result != PRS_OK);

            first = PRS_FALSE;
        }
    }

    prs_pal_atomic_store(&s_prs_uninit, PRS_FALSE);

    for (int i = s_prs_core_count - 1; i >= 0; --i) {
        if (!(params->core_mask & (1 << i))) {
            continue;
        }

        const prs_sched_id_t sched_id = s_prs_scheduler_ids[i];
        result = prs_sched_start(sched_id);
        PRS_FATAL_WHEN(result != PRS_OK);
    }

    if (params->use_current_thread) {
        PRS_ASSERT(params->use_current_thread);

        /*
         * Verify if we have been asked to exit. If we haven't, we have to wait until another worker calls exit().
         */
        const prs_bool_t must_exit = prs_pal_atomic_load(&s_prs_main_exit);
        if (!must_exit) {
            for (;;) {
            }
        }

        const prs_sched_id_t main_sched_id = prs_pal_atomic_load(&s_prs_main_sched_id);
        prs_uninit_final(main_sched_id);

        const int status = prs_pal_atomic_load(&s_prs_exit_status);
#if defined(PRS_EXIT)
        prs_fast_exit(status);
#endif /* defined(PRS_EXIT) */
        return status;
    }

    return 0;
}

static void prs_uninit_final(prs_sched_id_t except_id)
{
    prs_log_print("Stopping schedulers");
    for (int i = s_prs_core_count - 1; i >= 0; --i) {
        const prs_sched_id_t sched_id = s_prs_scheduler_ids[i];
        if (sched_id == PRS_OBJECT_ID_INVALID) {
            continue;
        }
        if (sched_id == except_id) {
            continue;
        }

        prs_sched_stop(sched_id);
    }

    prs_log_print("Destroying schedulers");
    for (int i = s_prs_core_count - 1; i >= 0; --i) {
        const prs_sched_id_t sched_id = s_prs_scheduler_ids[i];
        if (sched_id == PRS_OBJECT_ID_INVALID) {
            continue;
        }
        if (sched_id == except_id) {
            continue;
        }

        prs_sched_destroy(sched_id);
    }

    prs_log_print("Flushing logs");
    prs_log_flush();
}

static void prs_uninit_wait(void)
{
    struct prs_worker* worker = prs_worker_current();
    PRS_ASSERT(worker);

    PRS_FTRACE("waiting");
    prs_worker_int_enable(prs_worker_current());
    for (;;) {
    }
}

static void prs_uninit(prs_bool_t from_excp)
{
    const prs_bool_t already_uninitializing = prs_pal_atomic_exchange(&s_prs_uninit, PRS_TRUE);
    if (already_uninitializing) {
        prs_log_print("Already uninitializing: may exit abruptly. Flushing logs.");
        prs_log_flush();
        if (from_excp) {
            /* In exception handler, do not wait here */
            return;
        } else if (!prs_worker_current()) {
            /* No worker on this thread, just return */
            return;
        } else {
            /* The best we can do here is wait... */
            prs_uninit_wait();
        }
    }

    prs_log_print("Stopping clock");
    prs_clock_uninit();

    struct prs_task* current_task = prs_task_current();
    const prs_sched_id_t current_sched_id = current_task ? current_task->sched_id : PRS_OBJECT_ID_INVALID;

    if (!from_excp) {
        const prs_sched_id_t main_sched_id = prs_pal_atomic_load(&s_prs_main_sched_id);
        if (main_sched_id != PRS_OBJECT_ID_INVALID) {
            if (main_sched_id != current_sched_id) {
                prs_log_print("Deferring unitialization to main thread");
                prs_pal_atomic_store(&s_prs_main_exit, PRS_TRUE);

                /*
                 * If we are currently running on the main thread, this will switch contexts with the main one. Otherwise,
                 * this will interrupt the thread and make it change its context.
                 */
                prs_sched_destroy(main_sched_id);
                prs_uninit_wait();
            }
        }
    }

    prs_uninit_final(current_sched_id);
}

#if defined(PRS_PRINT_OBJECTS_ON_EXIT)
static void prs_exit_print(void* userdata, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    vprintf(fmt, va);
    va_end(va);
}
#endif

/**
 * \brief
 *  Exits the current process without uninitializing PRS.
 * \param status
 *  Status code that will be returned to the OS.
 * \note
 *  If \ref PRS_EXIT is defined, this function will be called by \ref prs_exit and \ref prs_exit_from_excp once PRS
 *  is uninitialized.
 */
void prs_fast_exit(int status)
{
    printf("\nPRS fast exit with status %d at tick %u\n", status, prs_clock_get());
    fflush(stdout);

#if defined(PRS_PRINT_OBJECTS_ON_EXIT)
    prs_god_print(0, prs_exit_print);
#endif

    prs_pal_exit(status);
}

/**
 * \brief
 *  Exits PRS gracefully.
 * \param status
 *  Status code that will be returned to the OS or from \ref prs_init.
 */
void prs_exit(int status)
{
    prs_log_print("prs_exit(%d) called", status);
    prs_uninit(PRS_FALSE);

    prs_fast_exit(status);
}

/**
 * \brief
 *  Exits PRS gracefully from an exception.
 * \param status
 *  Status code that will be returned to the OS or from \ref prs_init.
 */
void prs_exit_from_excp(int status)
{
    prs_log_print("prs_exit_from_excp(%d) called", status);
    prs_uninit(PRS_TRUE);

    prs_fast_exit(status);
}

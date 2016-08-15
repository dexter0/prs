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
 *  This file contains the process module definitions.
 *
 *  The process module handles the creation and destruction of the data structures required to load and unload external
 *  processes. More specifically:
 *    - It handles the graceful destruction of objects associated with a process. For example, a task that was spawned
 *      from a process will be destroyed once the process is destroyed, even if it was not done executing.
 *    - It calls the PAL to do the actual process loading.
 *    - It provides functions to retrieve the command line parameters of the process (argc, argv).
 *    - It calls registered callbacks (through \ref prs_proc_atexit) when the process is destroyed.
 *    - It provides the \ref prs_proc_is_user_text to find out if an instruction belongs to a loaded process.
 */

#include <stddef.h>
#include <string.h>

#include <prs/pal/malloc.h>
#include <prs/pal/proc.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/god.h>
#include <prs/gpd.h>
#include <prs/log.h>
#include <prs/idllist.h>
#include <prs/mpsciq.h>
#include <prs/proc.h>
#include <prs/rtc.h>
#include <prs/sem.h>
#include <prs/str.h>
#include <prs/task.h>
#include <prs/types.h>
#include <pr.h>

#include "proc.h"
#include "task.h"

struct prs_proc_object {
    struct prs_mpsciq_node              node;
    prs_object_id_t                     object_id;
};

struct prs_proc_range {
    PRS_ATOMIC prs_uintptr_t            base;
    PRS_ATOMIC prs_size_t               size;
};

struct prs_proc_data {
    struct prs_idllist*                 list;
    struct prs_proc_range               range_table[PRS_MAX_OBJECTS];
    PRS_ATOMIC prs_proc_range_table_index_t
                                        range_table_count;
};

static struct prs_proc_data* s_prs_proc_data = 0;

static void prs_proc_run_atexit_callbacks(struct prs_proc* proc)
{
    PRS_FTRACE("begin for process id %u", proc->id);
    prs_mpscq_foreach(proc->atexit_callbacks, node) {
        void (*function)(void) = prs_mpscq_get_data(proc->atexit_callbacks, node);
        function();
    }
    PRS_FTRACE("end for process id %u", proc->id);
}

static void prs_proc_range_table_add(struct prs_proc* proc)
{
    const prs_proc_range_table_index_t count = prs_pal_atomic_load(&s_prs_proc_data->range_table_count);
    prs_proc_range_table_index_t index = count;
    for (prs_proc_range_table_index_t i = count - 1; i >= 0; --i) {
        struct prs_proc_range* range = &s_prs_proc_data->range_table[i];
        if (!range->base) {
            PRS_ASSERT(!range->size);
            index = i;
            break;
        }
    }
    PRS_ASSERT(index >= 0);
    PRS_FATAL_WHEN(index >= PRS_MAX_OBJECTS);

    const prs_uintptr_t base = (prs_uintptr_t)prs_pal_proc_get_base(proc->pal_proc);
    PRS_ASSERT(base);
    const prs_size_t size = prs_pal_proc_get_size(proc->pal_proc);
    PRS_ASSERT(size);

    struct prs_proc_range* new_range = &s_prs_proc_data->range_table[index];
    prs_pal_atomic_store(&new_range->size, size);
    prs_pal_atomic_store(&new_range->base, base);
    if (index >= count) {
        prs_pal_atomic_store(&s_prs_proc_data->range_table_count, index + 1);
    }

    proc->range_table_index = index;
}

static void prs_proc_range_table_del(struct prs_proc* proc)
{
    const prs_proc_range_table_index_t index = proc->range_table_index;
    PRS_FATAL_WHEN(index >= PRS_MAX_OBJECTS);

    struct prs_proc_range* range = &s_prs_proc_data->range_table[index];
    prs_pal_atomic_store(&range->base, 0);
    prs_pal_atomic_store(&range->size, 0);
}

static void prs_proc_object_free(void* object)
{
    struct prs_proc* proc = object;

    if (proc->atexit_callbacks) {
        prs_proc_run_atexit_callbacks(proc);
        prs_mpscq_destroy(proc->atexit_callbacks);
    }
    if (proc->cmdline_buffer) {
        prs_pal_free(proc->cmdline_buffer);
    }
    if (proc->main_params.argv) {
        prs_pal_free(proc->main_params.argv);
    }
    if (proc->pal_proc) {
        prs_pal_proc_destroy(proc->pal_proc);
    }
    if (prs_idllist_is_inserted(s_prs_proc_data->list, &proc->node)) {
        prs_idllist_remove(s_prs_proc_data->list, &proc->node);
        prs_proc_range_table_del(proc);
    }
    if (proc->objects) {
        PRS_ASSERT(!prs_mpsciq_begin(proc->objects));
        prs_mpsciq_destroy(proc->objects);
    }
    prs_pal_free(proc);
}

static void prs_proc_object_print(void* object, void* userdata, void (*fct)(void*, const char*, ...))
{
    struct prs_proc* proc = object;

    fct(userdata, "Process id=%u '%s'\n",
        proc->id,
        proc->filename);
}

static struct prs_object_ops s_prs_proc_object_ops = {
    .destroy = 0,
    .free = prs_proc_object_free,
    .print = prs_proc_object_print
};

static void prs_proc_parse_cmdline(struct prs_proc* proc, char* cmdline)
{
    const int len = strlen(cmdline) + 1;
    proc->cmdline_buffer = prs_pal_malloc(len);
    PRS_FATAL_WHEN(!proc->cmdline_buffer);
    memcpy(proc->cmdline_buffer, cmdline, len);

    prs_int_t argc = 0;
    prs_bool_t in_quote = PRS_FALSE;
    prs_bool_t in_arg = PRS_FALSE;
    for (char* p = proc->cmdline_buffer; *p; ++p) {
        if (*p == ' ') {
            if (!in_quote && in_arg) {
                in_arg = PRS_FALSE;
            }
        } else {
            if (*p == '\"') {
                in_quote = PRS_BOOL(!in_quote);
            }
            if (!in_arg) {
                in_arg = PRS_TRUE;
                ++argc;
            }
        }
    }

    proc->main_params.argc = argc;
    proc->main_params.argv = prs_pal_malloc(sizeof(*proc->main_params.argv) * argc);

    argc = 0;
    in_quote = PRS_FALSE;
    in_arg = PRS_FALSE;
    for (char* p = proc->cmdline_buffer; *p; ++p) {
        if (*p == ' ') {
            if (!in_quote && in_arg) {
                in_arg = PRS_FALSE;
                *p = '\0';
            }
        } else {
            if (*p == '\"') {
                in_quote = PRS_BOOL(!in_quote);
                *p = '\0';
            }
            if (!in_arg) {
                in_arg = PRS_TRUE;
                proc->main_params.argv[argc++] = (in_quote ? p + 1 : p);
            }
        }
    }
}

/**
 * \brief
 *  Executes the specified process.
 * \param params
 *  Process execution parameters.
 */
struct prs_proc* prs_proc_exec(struct prs_proc_exec_params* params)
{
    struct prs_proc* proc = prs_pal_malloc_zero(sizeof(*proc));
    PRS_FATAL_WHEN(!proc);

    proc->id = prs_god_alloc_and_lock(proc, &s_prs_proc_object_ops);
    PRS_FATAL_WHEN(proc->id == PRS_OBJECT_ID_INVALID);

    struct prs_mpsciq_create_params mpsciq_params = {
        .node_offset = offsetof(struct prs_proc_object, node)
    };
    proc->objects = prs_mpsciq_create(&mpsciq_params);
    PRS_FATAL_WHEN(!proc->objects);

    proc->atexit_callbacks = prs_mpscq_create();
    PRS_FATAL_WHEN(!proc->atexit_callbacks);

    prs_str_copy(proc->filename, params->filename, sizeof(proc->filename));
    prs_proc_parse_cmdline(proc, params->cmdline);

    struct prs_pal_proc_load_params proc_load_params;
    prs_str_copy(proc_load_params.filename, proc->filename, sizeof(proc_load_params.filename));
    proc->pal_proc = prs_pal_proc_load(&proc_load_params);
    if (!proc->pal_proc) {
        goto cleanup;
    }

    prs_idllist_insert_before(s_prs_proc_data->list, 0, &proc->node);

    prs_proc_range_table_add(proc);

    struct prs_task_create_params task_params = {
        .userdata = &proc->main_params,
        .stack_size = params->main_task_params.stack_size,
        .prio = params->main_task_params.prio,
        .entry = prs_pal_proc_get_entry_point(proc->pal_proc)
    };
    prs_str_copy(task_params.name, params->main_task_params.name, sizeof(task_params.name));

    proc->main_task = prs_task_create(&task_params);
    if (!proc->main_task) {
        goto cleanup;
    }
    proc->main_task->proc_id = proc->id;
    prs_task_set_proc(proc->main_task, proc->id);

    const prs_result_t result = prs_sched_add_task(params->sched_id, proc->main_task->id);
    if (result != PRS_OK) {
        goto cleanup;
    }

    return proc;

    cleanup:

    if (proc) {
        if (proc->id) {
            prs_god_unlock(proc->id);
        }
    }

    return 0;
}

/**
 * \brief
 *  Destroys the specified process.
 * \param proc
 *  Process to destroy.
 */
void prs_proc_destroy(struct prs_proc* proc)
{
    if (!proc->destroyed) {
        PRS_FTRACE("%s (%u)", proc->filename, proc->id);
        struct prs_mpsciq_node* node = prs_mpsciq_begin(proc->objects);
        while (node) {
            struct prs_mpsciq_node* next = prs_mpsciq_next(proc->objects, node);
            struct prs_proc_object* proc_object = prs_mpsciq_get_data(proc->objects, node);
            const prs_object_id_t object_id = proc_object->object_id;
            prs_god_object_destroy(object_id);
            node = next;
        }

        proc->destroyed = PRS_TRUE;
    }
}

/**
 * \brief
 *  Registers an object to be associated with the specified process.
 *
 *  Objects have their reference counter incremented when they are associated with a process. When that process is
 *  destroyed, all objects associated with it are also destroyed.
 * \param proc
 *  Process to register from.
 * \param object_id
 *  Object ID of the object to associate with the process.
 */
void prs_proc_register_object(struct prs_proc* proc, prs_object_id_t object_id)
{
    struct prs_proc_object* proc_object = prs_pal_malloc_zero(sizeof(*proc_object));
    PRS_FATAL_WHEN(!proc_object);
    proc_object->object_id = object_id;
    prs_mpsciq_push(proc->objects, &proc_object->node);
    void* ptr = prs_god_lock(proc->id);
    PRS_FATAL_WHEN(!ptr);
}

/**
 * \brief
 *  Unregisters an object from the specified process.
 * \param proc
 *  Process to unregister from.
 * \param object_id
 *  Object ID of the object to unregister from the process.
 */
prs_result_t prs_proc_unregister_object(struct prs_proc* proc, prs_object_id_t object_id)
{
    prs_result_t result = PRS_NOT_FOUND;
    prs_mpsciq_foreach(proc->objects, node) {
        struct prs_proc_object* proc_object = prs_mpsciq_get_data(proc->objects, node);
        if (proc_object->object_id == object_id) {
            prs_mpsciq_remove(proc->objects, node);
            prs_god_unlock(proc->id);
            result = PRS_OK;
            break;
        }
    }
    return result;
}

/**
 * \brief
 *  Registers a destruction callback to be called once the specified process is destroyed.
 * \param proc
 *  Process for which the callback will be called.
 * \param function
 *  Function to call when the process is destroyed.
 */
void prs_proc_atexit(struct prs_proc* proc, void (*function)(void))
{
    prs_mpscq_push(proc->atexit_callbacks, function);
}

/**
 * \brief
 *  Performs garbage collection on the loaded processes.
 *
 *  Because tasks may be uninterruptible for some time, it is possible that the destruction of a process may not be
 *  done by a single call. Therefore, garbage collection must be performed periodically in order to clean destructed
 *  processes and free them.
 */
void prs_proc_gc(void)
{
    prs_result_t result;
    do {
        result = PRS_EMPTY;
        prs_idllist_foreach(s_prs_proc_data->list, node) {
            struct prs_proc* proc = prs_idllist_get_data(s_prs_proc_data->list, node);
            if (proc->destroyed) {
                result = prs_god_try_unlock_final(proc->id);
                if (result == PRS_OK) {
                    break;
                }
            }
        }
    } while (result == PRS_OK);
}

static void prs_proc_init_main(void)
{
    struct prs_proc* main_proc = prs_pal_malloc_zero(sizeof(*main_proc));
    PRS_FATAL_WHEN(!main_proc);

    struct prs_mpsciq_create_params mpsciq_params = {
        .node_offset = offsetof(struct prs_proc_object, node)
    };
    main_proc->objects = prs_mpsciq_create(&mpsciq_params);
    PRS_FATAL_WHEN(!main_proc->objects);

    main_proc->atexit_callbacks = prs_mpscq_create();
    PRS_FATAL_WHEN(!main_proc->atexit_callbacks);

    char cmdline[PRS_MAX_CMDLINE];
    main_proc->pal_proc = prs_pal_proc_create_main(main_proc->filename, cmdline);
    prs_proc_parse_cmdline(main_proc, cmdline);

    main_proc->id = prs_god_alloc_and_lock(main_proc, &s_prs_proc_object_ops);
    PRS_FATAL_WHEN(main_proc->id == PRS_OBJECT_ID_INVALID);

    main_proc->main_task = prs_task_current();
    const prs_result_t result = prs_task_set_proc(main_proc->main_task, main_proc->id);
    PRS_FATAL_WHEN(result != PRS_OK);

    prs_idllist_insert_before(s_prs_proc_data->list, 0, &main_proc->node);

    prs_proc_range_table_add(main_proc);
}

/**
 * \brief
 *  Initializes the process module.
 *
 *  In addition to initializing data structures required to list processes, this function also creates an implicit main
 *  process which represents the process the PRS environment is running into.
 */
void prs_proc_init(void)
{
    s_prs_proc_data = prs_pal_malloc_zero(sizeof(*s_prs_proc_data));
    PRS_FATAL_WHEN(!s_prs_proc_data);

    struct prs_idllist_create_params idllist_params = {
        .node_offset = offsetof(struct prs_proc, node)
    };
    s_prs_proc_data->list = prs_idllist_create(&idllist_params);
    PRS_FATAL_WHEN(!s_prs_proc_data->list);

    prs_proc_init_main();
}

/**
 * \brief
 *  Uninitializes the process module.
 */
void prs_proc_uninit(void)
{
}

/**
 * \brief
 *  Returns the number of command line arguments that the process which owns the current task has received.
 */
int prs_proc_get_argc(void)
{
    struct prs_task* task = prs_task_current();
    PRS_ASSERT(task);
    struct prs_proc* proc = prs_god_lock(task->proc_id);
    if (proc) {
        const int result = proc->main_params.argc;
        prs_god_unlock(task->proc_id);
        return result;
    } else {
        return 0;
    }
}

/**
 * \brief
 *  Returns the requested command line argument that the process which owns the current task has received. If there is
 *  no such argument, \p null is returned.
 */
const char* prs_proc_get_argv(int arg)
{
    struct prs_task* task = prs_task_current();
    PRS_ASSERT(task);
    struct prs_proc* proc = prs_god_lock(task->proc_id);
    if (proc) {
        PRS_RTC_IF (arg >= proc->main_params.argc) {
            prs_god_unlock(task->proc_id);
            return 0;
        }
        const char* result = proc->main_params.argv[arg];
        prs_god_unlock(task->proc_id);
        return result;
    } else {
        return 0;
    }
}

/**
 * \brief
 *  Returns if the specified instruction pointer belongs to any loaded processes (including the PRS environment).
 */
prs_bool_t prs_proc_is_user_text(void* instr_ptr)
{
    prs_result_t result = PRS_FALSE;
    const prs_uintptr_t ip = (prs_uintptr_t)instr_ptr;
    const prs_proc_range_table_index_t count = prs_pal_atomic_load(&s_prs_proc_data->range_table_count);
    for (prs_proc_range_table_index_t i = 0; i < count; ++i) {
        struct prs_proc_range* range = &s_prs_proc_data->range_table[i];
        const prs_uintptr_t base = prs_pal_atomic_load(&range->base);
        if (base && base < ip) {
            const prs_size_t size = prs_pal_atomic_load(&range->size);
            if (size && ip < base + size) {
                result = PRS_TRUE;
                break;
            }
        }
    }
    return result;
}

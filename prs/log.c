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
 *  This file contains the log module definitions.
 *
 *  The log module uses an underlying multi-producer multi-consumer ring buffer in order to let multiple workers
 *  simultaneously write and flush the log entries.
 */

#include <fcntl.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <unistd.h>

#include <prs/pal/atomic.h>
#include <prs/pal/cycles.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/clock.h>
#include <prs/error.h>
#include <prs/log.h>
#include <prs/mpmcring.h>
#include <prs/spinlock.h>
#include <prs/str.h>
#include <prs/worker.h>

#define PRS_MAX_LOG_ENTRY_SIZE          256

#define PRS_MAX_LOG_ENTRIES             4096

/**
 * \def PRS_LOG_IMMEDIATE
 * \brief
 *  Make \ref prs_log_print statements to immediately do syscalls in order to write to the log target(s).
 */
//#define PRS_LOG_IMMEDIATE

/**
 * \def PRS_LOG_STDOUT
 * \brief
 *  Print to stdout as well as to the log file.
 */
#if defined(PRS_LOG_IMMEDIATE)
#define PRS_LOG_STDOUT
#endif

/**
 * \def PRS_LOG_WORKER
 * \brief
 *  Print the worker id at the beginning of the trace.
 */
//#define PRS_LOG_WORKER

/**
 * \def PRS_LOG_CYCLES
 * \brief
 *  Print the cycles at the beginning of the trace.
 */
//#define PRS_LOG_CYCLES

struct prs_log {
    struct prs_mpmcring*                ring;
    FILE*                               fp;
    int                                 fd;
    int                                 fd_stdout;
    PRS_ATOMIC prs_uint_t               overflow_count;

    struct prs_spinlock*                spinlock;
};

struct prs_log_entry {
    prs_size_t                          size;
    char                                buf[1];
};

static struct prs_log* s_prs_log = 0;

/**
 * \brief
 *  Initializes the log module.
 */
void prs_log_init(void)
{
    struct prs_log* log = prs_pal_malloc(sizeof(*s_prs_log));
    if (!log) {
        return;
    }

    struct prs_mpmcring_create_params ring_params = {
        .data_size = offsetof(struct prs_log_entry, buf) + PRS_MAX_LOG_ENTRY_SIZE,
        .max_entries = PRS_MAX_LOG_ENTRIES
    };
    log->ring = prs_mpmcring_create(&ring_params);
    PRS_FATAL_WHEN(!log->ring);

    log->fp = fopen(PRS_LOG_PATH, "w+");
    PRS_FATAL_WHEN(!log->fp);

    log->spinlock = prs_spinlock_create();
    PRS_FATAL_WHEN(!log->spinlock);

    prs_pal_atomic_store(&log->overflow_count, 0);

    /* We use POSIX APIs from here because they are allowed in signal handlers */
    log->fd = fileno(log->fp);
    log->fd_stdout = fileno(stdout);
    s_prs_log = log;

    prs_log_print("Log initialized, %u entries reserved", PRS_MAX_LOG_ENTRIES);
}

/**
 * \brief
 *  Uninitializes the log module.
 */
void prs_log_uninit(void)
{
    /*
     * It's not possible to close the logs here as the prs_uninit() sequence cannot guarantee that no worker will be
     * writing to the log while it's being unitialized.
     */
    /*
    struct prs_log* log = s_prs_log;
    s_prs_log = 0;
    prs_spinlock_destroy(log->spinlock);
    prs_mpmcring_destroy(log->ring);
    fclose(log->fp);
    prs_pal_free(log);
    */
}

/**
 * \brief
 *  Queues a log trace to be printed to the log.
 * \param file
 *  File where the trace was emitted from. Can be \p null.
 * \param line
 *  Line where the trace was emitted from.
 * \param function
 *  Function where the trace was emitted from. Can be \p null.
 * \param fmt
 *  C-style format string.
 * \param va
 *  Variable arguments list to be used along with the \p fmt format string.
 * \note
 *  When \ref PRS_LOG_IMMEDIATE is defined, the trace is immediately printed to the log target(s) in addition to being
 *  queued.
 */
void prs_log_vprint(const char* file, int line, const char* function, const char* fmt, va_list va)
{
    va_list local_va;
    va_copy(local_va, va);

    struct prs_log_entry* entry = prs_mpmcring_alloc(s_prs_log->ring);
    if (!entry) {
        prs_pal_atomic_fetch_add(&s_prs_log->overflow_count, 1);
        return;
    }
    char* buf = entry->buf;
    int offset = 0;

    const prs_uint_t overflow_count = prs_pal_atomic_exchange(&s_prs_log->overflow_count, 0);
    if (overflow_count) {
        offset = prs_str_append_printf(buf, offset, PRS_MAX_LOG_ENTRY_SIZE, "OVF: %u\n", overflow_count);
    }

#if defined(PRS_LOG_CYCLES)
    offset = prs_str_append_printf(buf, offset, PRS_MAX_LOG_ENTRY_SIZE, "{%llu} ", prs_cycles_now());
#endif
    offset = prs_str_append_printf(buf, offset, PRS_MAX_LOG_ENTRY_SIZE, "[%6u] ", prs_clock_get());
#if defined(PRS_LOG_WORKER)
    struct prs_worker* worker = prs_worker_current();
    offset = prs_str_append_printf(buf, offset, PRS_MAX_LOG_ENTRY_SIZE, "#%2u ", worker->id);
#endif
    if (function) {
        offset = prs_str_append(buf, function, offset, PRS_MAX_LOG_ENTRY_SIZE);
        if (fmt) {
            offset = prs_str_append(buf, ": ", offset, PRS_MAX_LOG_ENTRY_SIZE);
        }
    }
    if (fmt) {
        offset = prs_str_append_vprintf(buf, offset, PRS_MAX_LOG_ENTRY_SIZE, fmt, local_va);
    }
    offset = prs_str_append(buf, "\n", offset, PRS_MAX_LOG_ENTRY_SIZE);
    entry->size = offset;

#if defined(PRS_LOG_IMMEDIATE)
    const ssize_t iresult = write(s_prs_log->fd, entry->buf, entry->size);
    PRS_ASSERT(iresult == entry->size);
#if defined(PRS_LOG_STDOUT)
    const ssize_t result = write(s_prs_log->fd_stdout, entry->buf, entry->size);
    PRS_ASSERT(result == entry->size);
#endif /* PRS_LOG_STDOUT */
#endif /* PRS_LOG_IMMEDIATE */

    prs_mpmcring_push(s_prs_log->ring, entry);
}

/**
 * \brief
 *  Queues a log trace to be printed to the log.
 * \param fmt
 *  C-style format string.
 * \param ...
 *  Variable arguments.
 * \note
 *  When \ref PRS_LOG_IMMEDIATE is defined, the trace is immediately printed to the log target(s) in addition to being
 *  queued.
 */
void prs_log_print(const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    prs_log_vprint(0, 0, 0, fmt, va);
    va_end(va);
}

/**
 * \brief
 *  Queues a log trace to be printed to the log.
 * \param file
 *  File where the trace was emitted from. Can be \p null.
 * \param line
 *  Line where the trace was emitted from.
 * \param function
 *  Function where the trace was emitted from. Can be \p null.
 * \param fmt
 *  C-style format string.
 * \param ...
 *  Variable arguments.g.
 * \note
 *  When \ref PRS_LOG_IMMEDIATE is defined, the trace is immediately printed to the log target(s) in addition to being
 *  queued.
 * \note
 *  This function is not meant to be used directly. Use the \ref PRS_FTRACE macro instead.
 */
void prs_log_ftrace(const char* file, int line, const char* function, const char* fmt, ...)
{
    va_list va;
    va_start(va, fmt);
    prs_log_vprint(file, line, function, fmt, va);
    va_end(va);
}

/**
 * \brief
 *  Flushes the log queue to the log target(s).
 * \return
 *  The number of unique entries (lines) that were written to the log target(s).
 * \note
 *  When \ref PRS_LOG_IMMEDIATE is defined, the traces are not written to the log but merely removed from the queue.
 */
prs_int_t prs_log_flush(void)
{
    prs_int_t count = 0;

    /*
     * The spinlock allows exception handlers to flush the log at all times, without fearing a race condition in the
     * write() syscall.
     */
    prs_spinlock_lock(s_prs_log->spinlock);

    struct prs_log_entry* entry;
    while ((entry = prs_mpmcring_pop(s_prs_log->ring)) != 0) {
#if !defined(PRS_LOG_IMMEDIATE)
        const ssize_t iresult = write(s_prs_log->fd, entry->buf, entry->size);
        PRS_ASSERT(iresult == entry->size);
#if defined(PRS_LOG_STDOUT)
        const ssize_t result = write(s_prs_log->fd_stdout, entry->buf, entry->size);
        PRS_ASSERT(result == entry->size);
#endif /* PRS_LOG_STDOUT */
#endif /* !PRS_LOG_IMMEDIATE */
        prs_mpmcring_free(s_prs_log->ring, entry);
        ++count;
    }

    prs_spinlock_unlock(s_prs_log->spinlock);

    return count;
}

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
 *  This file contains the POSIX exception definitions.
 */

#include <prs/pal/atomic.h>
#include <prs/pal/excp.h>
#include <prs/pal/malloc.h>
#include <prs/pal/os.h>
#include <prs/alloc/stack.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/log.h>
#include <prs/types.h>
#include <prs/worker.h>

#include "signal.h"
#include "../../task.h"

static const int prs_pal_excp_handled_signals[] = {
    SIGFPE,
    SIGILL,
    SIGSEGV,
    SIGBUS,
    SIGSYS
};
static const int prs_pal_excp_handled_signal_count =
    sizeof(prs_pal_excp_handled_signals) / sizeof(prs_pal_excp_handled_signals[0]);

static PRS_ATOMIC prs_bool_t s_prs_pal_excp_sigaction_done = PRS_FALSE;
static struct sigaction s_prs_pal_excp_prev_sigaction;

static void prs_pal_excp_sighandler(int signo, siginfo_t* sinfo, void* ucontext)
{
    PRS_PRECONDITION(ucontext);

    struct prs_worker* worker = prs_worker_current();
    if (!worker) {
        /* No worker for this thread. Do not process this exception here. */
        prs_pal_signal_chain(signo, sinfo, ucontext, &s_prs_pal_excp_prev_sigaction);
        return;
    }

    void* extra = sinfo->si_addr;

    PRS_FTRACE("#%d, extra=%p", signo, extra);

    enum prs_excp_type type = PRS_EXCP_TYPE_UNKNOWN;
    switch (signo) {
        case SIGFPE:
            /* Could also be an integer error */
            type = PRS_EXCP_TYPE_FLOATING_POINT;
            break;
        case SIGBUS:
            type = PRS_EXCP_TYPE_BUS;
            break;
        case SIGILL:
            type = PRS_EXCP_TYPE_ILLEGAL_INSTRUCTION;
            break;
        case SIGSEGV: {
            struct prs_task* task = prs_worker_get_current_task(worker);
            if (task && prs_stack_address_in_range(task->stack, extra)) {
                type = PRS_EXCP_TYPE_STACK_OVERFLOW;
            } else {
                type = PRS_EXCP_TYPE_SEGMENTATION_FAULT;
            }
            break;
        }
        case SIGSYS:
            type = PRS_EXCP_TYPE_OS;
            break;
    }

    prs_excp_raise(type, extra, worker, (struct prs_pal_context*)ucontext);
}

prs_result_t prs_pal_excp_init_worker(struct prs_worker* worker)
{
    prs_result_t result = PRS_OK;

    if (!prs_pal_atomic_exchange(&s_prs_pal_excp_sigaction_done, PRS_TRUE)) {
        for (int i = 0; i < prs_pal_excp_handled_signal_count; ++i) {
            const int signum = prs_pal_excp_handled_signals[i];
            const prs_result_t result = prs_pal_signal_action(signum, prs_pal_excp_sighandler,
                &s_prs_pal_excp_prev_sigaction);
            if (result != PRS_OK) {
                return result;
            }
        }
    }

    for (int i = 0; i < prs_pal_excp_handled_signal_count; ++i) {
        const int signum = prs_pal_excp_handled_signals[i];
        const prs_result_t result = prs_pal_signal_unblock(signum);
        if (result != PRS_OK) {
            return result;
        }
    }

    return result;
}

void prs_pal_excp_uninit_worker(struct prs_worker* worker)
{
    for (int i = 0; i < prs_pal_excp_handled_signal_count; ++i) {
        const int signum = prs_pal_excp_handled_signals[i];
        prs_pal_signal_block(signum);
    }
}

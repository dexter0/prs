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
 *  This file contains the POSIX signal definitions.
 */

#include <signal.h>
#include <string.h>

#include <prs/alloc/stack.h>
#include <prs/pal/tls.h>
#include <prs/error.h>
#include <prs/result.h>
#include <prs/types.h>

#include "signal.h"

static PRS_TLS prs_bool_t s_prs_pal_signal_initialized = PRS_FALSE;
static PRS_TLS void* s_prs_pal_signal_stack = 0;

static prs_result_t prs_pal_signal_init(void)
{
    if (!s_prs_pal_signal_initialized) {
        /* Set up the alternate stack for this thread */
        int error;
        prs_size_t stack_size;
        void* stack_top = prs_stack_create(SIGSTKSZ, &stack_size);
        PRS_FATAL_WHEN(!stack_top);
        s_prs_pal_signal_stack = (void*)((prs_uintptr_t)stack_top - stack_size);
        stack_t altstack = {
            .ss_sp = s_prs_pal_signal_stack,
            .ss_flags = 0,
            .ss_size = stack_size
        };
        error = sigaltstack(&altstack, 0);
        if (error) {
            return PRS_PLATFORM_ERROR;
        }

        /* Block all signals by default */
        sigset_t sigset;
        error = sigfillset(&sigset);
        if (error) {
            return PRS_PLATFORM_ERROR;
        }
        error = pthread_sigmask(SIG_BLOCK, &sigset, 0);
        if (error) {
            return PRS_PLATFORM_ERROR;
        }
        s_prs_pal_signal_initialized = PRS_TRUE;
    }
    return PRS_OK;
}

static prs_result_t prs_pal_signal_set(int signum, int action)
{
    prs_result_t result;
    result = prs_pal_signal_init();
    if (result != PRS_OK) {
        return result;
    }
    
    int error;
    sigset_t sigset;
    error = sigemptyset(&sigset);
    if (error) {
        return PRS_PLATFORM_ERROR;
    }
    error = sigaddset(&sigset, signum);
    if (error) {
        return PRS_PLATFORM_ERROR;
    }
    error = pthread_sigmask(action, &sigset, 0);
    if (error) {
        return PRS_PLATFORM_ERROR;
    }
    return PRS_OK;
}

prs_result_t prs_pal_signal_block(int signum)
{
    return prs_pal_signal_set(signum, SIG_BLOCK);
}

prs_result_t prs_pal_signal_unblock(int signum)
{
    return prs_pal_signal_set(signum, SIG_UNBLOCK);
}

prs_result_t prs_pal_signal_action(int signum, void (*handler)(int, siginfo_t*, void*), struct sigaction* prev)
{
    int error;
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_sigaction = handler;
    error = sigfillset(&sa.sa_mask); /* No nested signals */
    if (error) {
        return PRS_PLATFORM_ERROR;
    }
    sa.sa_flags = SA_ONSTACK | SA_SIGINFO;

    error = sigaction(signum, &sa, prev);
    if (error) {
        return PRS_PLATFORM_ERROR;
    }

    return PRS_OK;
}

void prs_pal_signal_chain(int signum, siginfo_t* sinfo, void* context, struct sigaction* action)
{
    if (action) {
        if (action->sa_flags & SA_SIGINFO) {
            if (action->sa_sigaction) {
                action->sa_sigaction(signum, sinfo, context);
            }
        } else {
            if (action->sa_handler) {
                action->sa_handler(signum);
            }
        }
    }
}

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
 *  This file contains the Windows exception definitions.
 */

#include <stdio.h>
#include <windows.h>

#include <prs/pal/excp.h>
#include <prs/pal/malloc.h>
#include <prs/pal/os.h>
#include <prs/error.h>
#include <prs/log.h>
#include <prs/rtc.h>
#include <prs/types.h>
#include <prs/worker.h>

static prs_bool_t s_prs_pal_excp_registered = PRS_FALSE;

static LONG prs_pal_excp_windows_vectored_handler(PEXCEPTION_POINTERS ExceptionInfo)
{
    struct prs_worker* worker = prs_worker_current();
    if (!worker) {
        /* No worker for this thread. Do not process this exception here. */
        PRS_FTRACE("no worker");
        return EXCEPTION_CONTINUE_SEARCH;
    }

    PEXCEPTION_RECORD record = ExceptionInfo->ExceptionRecord;
    PCONTEXT wincontext = ExceptionInfo->ContextRecord;

    PRS_FTRACE("code 0x%08X", record->ExceptionCode);

    enum prs_excp_type type = PRS_EXCP_TYPE_UNKNOWN;
    void* extra = 0;
    switch (record->ExceptionCode) {
        case STATUS_ACCESS_VIOLATION:
            type = PRS_EXCP_TYPE_SEGMENTATION_FAULT;
            extra = (void *)record->ExceptionInformation[1];
            break;

        case STATUS_GUARD_PAGE_VIOLATION:
            type = PRS_EXCP_TYPE_STACK_OVERFLOW;
            extra = (void *)record->ExceptionInformation[1];
            break;

        case STATUS_INVALID_HANDLE:
        case STATUS_INVALID_PARAMETER:
        case STATUS_ASSERTION_FAILURE:
            type = PRS_EXCP_TYPE_OS;
            break;

        case STATUS_ILLEGAL_INSTRUCTION:
        case STATUS_PRIVILEGED_INSTRUCTION:
            type = PRS_EXCP_TYPE_ILLEGAL_INSTRUCTION;
            break;

        case STATUS_CONTROL_C_EXIT:
            /* This isn't going to work most of the time. We use SetConsoleCtrlHandler() in the PAL OS module. */
            type = PRS_EXCP_TYPE_USER_INTERRUPT;
            break;

        case STATUS_FLOAT_MULTIPLE_FAULTS:
        case STATUS_FLOAT_MULTIPLE_TRAPS:
        case STATUS_FLOAT_DENORMAL_OPERAND:
        case STATUS_FLOAT_DIVIDE_BY_ZERO:
        case STATUS_FLOAT_INEXACT_RESULT:
        case STATUS_FLOAT_INVALID_OPERATION:
        case STATUS_FLOAT_OVERFLOW:
        case STATUS_FLOAT_STACK_CHECK:
        case STATUS_FLOAT_UNDERFLOW:
            type = PRS_EXCP_TYPE_FLOATING_POINT;
            break;

        case STATUS_INTEGER_DIVIDE_BY_ZERO:
        case STATUS_INTEGER_OVERFLOW:
            type = PRS_EXCP_TYPE_INTEGER;
            break;
    }

    prs_excp_raise(type, extra, worker, (struct prs_pal_context*)wincontext);

    return EXCEPTION_CONTINUE_EXECUTION;
}

prs_result_t prs_pal_excp_init_worker(struct prs_worker* worker)
{
    prs_result_t result = PRS_OK;

    if (!s_prs_pal_excp_registered) {
        s_prs_pal_excp_registered = PRS_TRUE;
        const PVOID add_result = AddVectoredExceptionHandler(1, prs_pal_excp_windows_vectored_handler);
        if (!add_result) {
            result = PRS_PLATFORM_ERROR;
        }
    }

    return result;
}

void prs_pal_excp_uninit_worker(struct prs_worker* worker)
{
    if (s_prs_pal_excp_registered) {
        s_prs_pal_excp_registered = PRS_FALSE;
        RemoveVectoredExceptionHandler(prs_pal_excp_windows_vectored_handler);
    }
}

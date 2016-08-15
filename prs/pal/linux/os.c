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
 *  This file contains the Linux operating system definitions.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/utsname.h>
#include <unistd.h>

#include <prs/pal/os.h>
#include <prs/excp.h>
#include <prs/result.h>

#include "../posix/signal.h"

static struct utsname s_prs_pal_linux_uname;
static prs_size_t s_prs_pal_linux_core_count;
static prs_size_t s_prs_pal_linux_page_size;
static prs_size_t s_prs_pal_linux_huge_page_size;

static void prs_pal_os_user_interrupt(int signo, siginfo_t* sinfo, void* ucontext)
{
    prs_excp_raise(PRS_EXCP_TYPE_USER_INTERRUPT, 0, 0, (struct prs_pal_context*)ucontext);
}

void prs_pal_os_init(void)
{
    const int error = uname(&s_prs_pal_linux_uname);
    if (error) {
        fprintf(stderr, "uname() failed\n");
        exit(-1);
    }

    s_prs_pal_linux_core_count = sysconf(_SC_NPROCESSORS_ONLN);
    s_prs_pal_linux_page_size = sysconf(_SC_PAGESIZE);
    s_prs_pal_linux_huge_page_size = 0;

    prs_result_t result = prs_pal_signal_action(SIGINT, prs_pal_os_user_interrupt, 0);
    if (result != PRS_OK) {
        fprintf(stderr, "Couldn't register SIGINT\n");
        exit(-1);
    }

    result = prs_pal_signal_unblock(SIGINT);
    if (result != PRS_OK) {
        fprintf(stderr, "Couldn't unblock SIGINT\n");
        exit(-1);
    }
}

void prs_pal_os_uninit(void)
{

}

prs_size_t prs_pal_os_get_core_count(void)
{
    return s_prs_pal_linux_core_count;
}

prs_size_t prs_pal_os_get_page_size(void)
{
    return s_prs_pal_linux_page_size;
}

prs_size_t prs_pal_os_get_huge_page_size(void)
{
    return s_prs_pal_linux_huge_page_size;
}

const char* prs_pal_os_get_version(void)
{
    return s_prs_pal_linux_uname.release;
}

const char* prs_pal_os_get_computer(void)
{
    return s_prs_pal_linux_uname.nodename;
}

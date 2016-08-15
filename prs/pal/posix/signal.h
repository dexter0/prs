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
 *  This file contains the POSIX signal declarations.
 */

#ifndef _PRS_PAL_SIGNAL_H
#define _PRS_PAL_SIGNAL_H

#include <prs/pal/os.h>
#if PRS_PAL_OS != PRS_PAL_OS_LINUX
#error signal.h only supported on Linux
#endif

#include <signal.h>

#include <prs/result.h>

prs_result_t prs_pal_signal_block(int signum);
prs_result_t prs_pal_signal_unblock(int signum);

prs_result_t prs_pal_signal_action(int signum, void (*handler)(int, siginfo_t*, void*), struct sigaction* prev);
void prs_pal_signal_chain(int signum, siginfo_t* sinfo, void* context, struct sigaction* action);

#endif /* _PRS_PAL_SIGNAL_H */

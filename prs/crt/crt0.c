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
 *  This file contains the PRS CRT definitions.
 *
 *  When an executable is loaded by PRS, it will spawn a task that will call the executable's entry point. The default
 *  entry point for PRS executables is \ref PR_ENTRYPOINT. The entry point runs C constructors and registers its
 *  destructors using \ref pr_atexit. The \ref pr_main call is then made so that the user code can run. If the user
 *  code did not already call \ref pr_exit to exit the process, it is called automatically when \ref pr_main returns.
 *
 *  The entry point defined in this file is intentionally different from the usual one, as it would call native CRT
 *  initializers which were already run.
 */

#include <prs/pal/crt.h>
#include <prs/proc.h>
#include <pr.h>

/**
 * \brief
 *  Entry point of a dynamically loaded PRS executable.
 * \param userdata
 *  Process parameters.
 */
void PR_ENTRYPOINT(void* userdata)
{
    struct prs_proc_main_params* params = userdata;

    _prs_pal_crt_ctors();
    pr_atexit(_prs_pal_crt_dtors);

    extern int pr_main(int argc, char* argv[]);
    const int ret = pr_main(params->argc, params->argv);

    pr_exit(ret);
}

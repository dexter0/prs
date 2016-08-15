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
 *  This file contains the PAL process declarations.
 *
 *  The PAL process module handles process loading and process management related to the operating system.
 */

#ifndef _PRS_PAL_PROC_H
#define _PRS_PAL_PROC_H

#include <prs/config.h>
#include <prs/object.h>
#include <prs/result.h>
#include <prs/task.h>
#include <prs/types.h>

struct prs_pal_proc;

/**
 * \brief
 *  This structure provides the PAL process loading parameters that should be passed to \ref prs_pal_proc_load.
 */
struct prs_pal_proc_load_params {
    /** \brief Path of the process that should be loaded. */
    char                                filename[PRS_MAX_PATH];
};

/**
 * \brief
 *  Create the PAL process object for the main PRS process. This does not load an actual process, it only gathers
 *  information about the current process.
 */
struct prs_pal_proc* prs_pal_proc_create_main(char* filename, char* cmdline);

/**
 * \brief
 *  Loads the specified process into the current process' virtual address space and load dynamic library dependencies.
 * \param params
 *  Load parameters.
 */
struct prs_pal_proc* prs_pal_proc_load(struct prs_pal_proc_load_params* params);

/**
 * \brief
 *  Removes the specified process from current process' virtual address space.
 * \param pal_proc
 *  Process to remove.
 */
void prs_pal_proc_destroy(struct prs_pal_proc* pal_proc);

/**
 * \brief
 *  Returns the specified process' entry point address.
 * \param pal_proc
 *  Process to get the entry point from.
 */
void* prs_pal_proc_get_entry_point(struct prs_pal_proc* pal_proc);

/**
 * \brief
 *  Returns the specified process' base address in virtual memory.
 * \param pal_proc
 *  Process to get the base address from.
 */
void* prs_pal_proc_get_base(struct prs_pal_proc* pal_proc);

/**
 * \brief
 *  Returns the specified process' allocated virtual memory size.
 * \param pal_proc
 *  Process to get the size from.
 */
prs_size_t prs_pal_proc_get_size(struct prs_pal_proc* pal_proc);

#endif /* _PRS_PAL_PROC_H */

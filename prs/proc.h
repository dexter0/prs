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
 *  This file contains the private declarations for a process control block (\ref prs_proc).
 */

#ifndef _PRSP_PROC_H
#define _PRSP_PROC_H

#include <prs/pal/proc.h>
#include <prs/config.h>
#include <prs/idllist.h>
#include <prs/mpscq.h>

typedef prs_int_t prs_proc_range_table_index_t;

struct prs_proc {
    struct prs_idllist_node             node;

    prs_proc_id_t                       id;
    char                                filename[PRS_MAX_PATH];
    struct prs_proc_main_params         main_params;
    struct prs_pal_proc*                pal_proc;
    char*                               cmdline_buffer;
    struct prs_task*                    main_task;

    prs_proc_range_table_index_t        range_table_index;

    struct prs_mpsciq*                  objects;
    struct prs_mpscq*                   atexit_callbacks;

    prs_bool_t                          destroyed;
};

#endif /* _PRSP_PROC_H */

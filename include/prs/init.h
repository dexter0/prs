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
 *  This file contains the PRS initialization and uninitialization declarations.
 */

#ifndef _PRS_INIT_H
#define _PRS_INIT_H

#include <prs/types.h>

/**
 * \brief
 *  PRS initialization parameters.
 */
struct prs_init_params {
    /** \brief If PRS should use the currently executing thread as a worker. */
    prs_bool_t                          use_current_thread;
    /** \brief The CPU cores that PRS should spawn a worker and scheduler on. */
    prs_core_mask_t                     core_mask;
};

int prs_init(struct prs_init_params* params);

void prs_fast_exit(int status);
void prs_exit(int status);
void prs_exit_from_excp(int status);

#endif /* _PRS_INIT_H */

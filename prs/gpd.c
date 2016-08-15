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
 *  This file contains the global pointer directory definitions.
 *
 *  There can only be one global pointer directory (GPD) at a time. It is used to dereference pointers simultaneously
 *  across multiple workers using a unique ID. It can be accessed through the \ref prs_pd implementation and is
 *  obtained through \ref prs_gpd_get.
 */

#include <prs/error.h>
#include <prs/gpd.h>
#include <prs/pd.h>

static struct prs_pd* s_prs_gpd = 0;

/**
 * \brief
 *  Initializes the global pointer directory.
 */
void prs_gpd_init(void)
{
    struct prs_pd_create_params params = {
        .max_entries = PRS_MAX_GPD_ENTRIES,
        .area = 0
    };
    s_prs_gpd = prs_pd_create(&params);
    PRS_FATAL_WHEN(!s_prs_gpd);
}

/**
 * \brief
 *  Uninitializes the global pointer directory.
 */
void prs_gpd_uninit(void)
{
    prs_pd_destroy(s_prs_gpd);
}

/**
 * \brief
 *  Returns a pointer to the global pointer directory.
 */
struct prs_pd* prs_gpd_get(void)
{
    return s_prs_gpd;
}

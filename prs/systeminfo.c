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
 *  This file contains system information definitions.
 */

#include <prs/pal/arch.h>
#include <prs/pal/compiler.h>
#include <prs/pal/config.h>
#include <prs/pal/os.h>
#include <prs/systeminfo.h>
#include <prs/version.h>

static struct prs_systeminfo s_prs_systeminfo;

/**
 * \brief
 *  Returns the system information.
 * \note
 *  This function actually retrieves the system information only the first time it is called.
 */
struct prs_systeminfo* prs_systeminfo_get(void)
{
    static prs_bool_t s_systeminfo_set = PRS_FALSE;
    
    if (!s_systeminfo_set) {
        s_prs_systeminfo.os_id = PRS_PAL_OS;
        s_prs_systeminfo.os = PRS_PAL_OS_NAME;
        s_prs_systeminfo.os_version = prs_pal_os_get_version();
        s_prs_systeminfo.arch_id = PRS_PAL_ARCH;
        s_prs_systeminfo.arch = PRS_PAL_ARCH_NAME;
        s_prs_systeminfo.arch_bits = PRS_PAL_POINTER_SIZE * 8;
        //s_prs_systeminfo.march;
        s_prs_systeminfo.compiler_id = PRS_PAL_COMPILER;
        s_prs_systeminfo.compiler = PRS_PAL_COMPILER_NAME;
        s_prs_systeminfo.compiler_version = PRS_PAL_COMPILER_VERSION;
        s_prs_systeminfo.computer = prs_pal_os_get_computer();
        s_prs_systeminfo.config_id = PRS_PAL_CONFIG;
        s_prs_systeminfo.config = PRS_PAL_CONFIG_NAME;
        s_prs_systeminfo.version_number = 0;
        s_prs_systeminfo.version = PRS_VERSION_NAME;
        prs_size_t core_count = prs_pal_os_get_core_count();
        if (core_count > PRS_MAX_CPU) {
            /* Clamp the number of CPUs to the maximum supported by PRS */
            core_count = PRS_MAX_CPU;
        }
        s_prs_systeminfo.core_count = core_count;
        
        s_systeminfo_set = PRS_TRUE;
    }
    
    return &s_prs_systeminfo;
}

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
 *  This file contains system information declarations.
 */

#ifndef _PRS_SYSTEMINFO_H
#define _PRS_SYSTEMINFO_H

#include <prs/types.h>

/**
 * \brief
 *  This structure contains fields that describe the system.
 */
struct prs_systeminfo {
    /** \brief Operating system identifier obtained through \ref PRS_PAL_OS. */
    prs_int_t                           os_id;
    /** \brief Operating system name obtained through \ref PRS_PAL_OS_NAME. */
    const char*                         os;
    /** \brief Operating system version obtained through \ref prs_pal_os_get_version. */
    const char*                         os_version;
    /** \brief Architecture identifier obtained through \ref PRS_PAL_ARCH. */
    prs_int_t                           arch_id;
    /** \brief Architecture name obtained through \ref PRS_PAL_ARCH_NAME. */
    const char*                         arch;
    /** \brief Architecture pointer size obtained through \ref PRS_PAL_POINTER_SIZE. */
    prs_int_t                           arch_bits;
    /** \brief Microarchitecture name. \note Not implemented. */
    const char*                         march;
    /** \brief Compiler identifier obtained through \ref PRS_PAL_COMPILER. */
    prs_int_t                           compiler_id;
    /** \brief Compiler name obtained through \ref PRS_PAL_COMPILER_NAME. */
    const char*                         compiler;
    /** \brief Compiler version obtained through \ref PRS_PAL_COMPILER_VERSION. */
    const char*                         compiler_version;
    /** \brief System name obtained through \ref prs_pal_os_get_computer. */
    const char*                         computer;
    /** \brief Configuration identifier obtained through \ref PRS_PAL_CONFIG. */
    prs_int_t                           config_id;
    /** \brief Configuration name obtained through \ref PRS_PAL_CONFIG_NAME. */
    const char*                         config;
    /** \brief Version number. \note Not implemented. */
    prs_int_t                           version_number;
    /** \brief Version name. */
    const char*                         version;
    /** \brief Number of cores in the system. */
    prs_int_t                           core_count;
};

struct prs_systeminfo* prs_systeminfo_get(void);

#endif /* _PRS_SYSTEMINFO_H */

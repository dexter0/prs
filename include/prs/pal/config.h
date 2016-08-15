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
 *  This file contains the PAL configuration preprocessor definitions.
 */

#ifndef _PRS_PAL_CONFIG_H
#define _PRS_PAL_CONFIG_H

/** \brief Debug configuration. */
#define PRS_PAL_CONFIG_DEBUG            0
/** \brief Release configuration. */
#define PRS_PAL_CONFIG_RELEASE          1

#if defined(DEBUG)
    #define PRS_PAL_CONFIG              PRS_PAL_CONFIG_DEBUG
    #define PRS_PAL_CONFIG_NAME         "debug"
#else /* DEBUG */
    #define PRS_PAL_CONFIG              PRS_PAL_CONFIG_RELEASE
    #define PRS_PAL_CONFIG_NAME         "release"
#endif /* DEBUG */

/**
 * \def PRS_PAL_CONFIG
 * \brief
 *  Defines the current configuration.
 *
 *  It is one of the following:
 *    - \ref PRS_PAL_CONFIG_DEBUG
 *    - \ref PRS_PAL_CONFIG_RELEASE
 */
#if !defined(PRS_PAL_CONFIG)
    #error PRS_PAL_CONFIG is not defined
    #define PRS_PAL_CONFIG /* doxygen */
#endif

/**
 * \def PRS_PAL_CONFIG_NAME
 * \brief
 *  Defines the current configuration name in a string literal format.
 */
#if !defined(PRS_PAL_CONFIG_NAME)
    #error PRS_PAL_CONFIG_NAME is not defined
    #define PRS_PAL_CONFIG /* doxygen */
#endif

#endif /* _PRS_PAL_CONFIG_H */
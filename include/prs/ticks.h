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
 *  This file contains the system tick declarations.
 */

#ifndef _PRS_TICKS_H
#define _PRS_TICKS_H

#include <prs/config.h>

/** \brief System ticks type. */
typedef PRS_TICKS_TYPE prs_ticks_t;

/** \brief Converts seconds to ticks. */
#define PRS_TICKS_FROM_SECS(secs)       ((prs_ticks_t)secs * (PRS_HZ))
/** \brief Converts milliseconds to ticks. */
#define PRS_TICKS_FROM_MS(ms)           ((prs_ticks_t)ms * (PRS_HZ) / 1000)
/** \brief Converts microseconds to ticks. */
#define PRS_TICKS_FROM_US(us)           ((prs_ticks_t)us * (PRS_HZ) / 1000000)

/** \brief Converts ticks to seconds. */
#define PRS_TICKS_TO_SECS(ticks)        (ticks / (PRS_HZ))
/** \brief Converts ticks to milliseconds. */
#define PRS_TICKS_TO_MS(ticks)          (ticks * 1000 / (PRS_HZ))
/** \brief Converts ticks to microseconds. */
#define PRS_TICKS_TO_US(ticks)          (ticks * 1000000 / (PRS_HZ))

#endif /* _PRS_TICKS_H */

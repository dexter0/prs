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
 *  This file contains the global configuration preprocessor directives.
 */

#ifndef _PRS_CONFIG_H
#define _PRS_CONFIG_H

/**
 * \def PRS_RUN_TIME_CHECKING
 * \brief
 *  Run-time checking adds optional run-time conditions to PRS APIs.
 */
#define PRS_RUN_TIME_CHECKING

/**
 * \def PRS_EXIT
 * \brief
 *  When defined, call \p exit when \ref prs_exit is called. Only when this option is commented out that \ref prs_init
 *  can return when the \p use_current_thread parameter is used.
 */
#define PRS_EXIT

/**
 * \def PRS_CLEAN
 * \brief
 *  When defined, stop schedulers and flush logs when a fatal error occurs.
 */
#define PRS_CLEAN

/**
 * \def PRS_ASSERTIONS
 * \brief
 *  Add assertions to debug code to report undefined behavior.
 */
#if defined(DEBUG)
#define PRS_ASSERTIONS
#endif

/**
 * \def PRS_FUNCTION_TRACES
 * \brief
 *  Function traces (\ref PRS_FTRACE) add function call descriptions to the log.
 * \note
 *  When enabled, this feature significantly decreases performance. It is best to use it for debugging only.
 */
//#define PRS_FUNCTION_TRACES

/**
 * \brief
 *  Maximum number of CPUs in the system. If the actual number of CPUs is higher than this value, it is clamped to it.
 */
#define PRS_MAX_CPU                     32

/**
 * \brief
 *  Must hold one bit per CPU
 */
#define PRS_CPU_TYPE                    prs_uint32_t

/**
 * \brief
 *  Maximum path length
 */
#define PRS_MAX_PATH                    256

/**
 * \brief
 *  Maximum command line length
 */
#define PRS_MAX_CMDLINE                 256

/**
 * \brief
 *  Maximum scheduler name length
 */
#define PRS_MAX_SCHED_NAME              32

/**
 * \brief
 *  Maximum task name length
 */
#define PRS_MAX_TASK_NAME               32

/**
 * \brief
 *  Maximum task priority value
 */
#define PRS_MAX_TASK_PRIO               32

/**
 * \brief
 *  Type that must hold one bit per priority level
 */
#define PRS_TASK_PRIO_TYPE              prs_uint32_t

/**
 * \brief
 *  Type that must hold a period of ticks that is long enough for the system's normal function
 */
#define PRS_TICKS_TYPE                  prs_uint32_t

/**
 * \brief
 *  Clock ticks per second. The period will determine the frequency of \ref prs_clock_get updates
 */
#define PRS_HZ                          1000

/**
 * \brief
 *  Maximum number of objects that may be allocated simultaneously
 * \note
 *  Must be a power of 2
 */
#define PRS_MAX_OBJECTS                 4096

/**
 * \brief
 *  Maximum amount of virtual memory reserved for a task stack
 */
#define PRS_MAX_STACK_SIZE              (1*1024*1024)

/**
 * \brief
 *  Maximum number of entries in the global pointer directory
 */
#define PRS_MAX_GPD_ENTRIES             (PRS_MAX_OBJECTS * 4)

/**
 * \brief
 *  Default location of the PRS log file
 */
#if !defined(PRS_LOG_PATH)
#define PRS_LOG_PATH                    "prs.log"
#endif /* !PRS_LOG_PATH */

/**
 * \brief
 *  Default location of the test results database
 */
#if !defined(PRS_TEST_DB_PATH)
#define PRS_TEST_DB_PATH                "prstestdb.sqlite"
#endif /* !PRS_TEST_DB_PATH */

#endif /* _PRS_CONFIG_H */

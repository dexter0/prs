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
 *  This file contains the PAL thread declarations.
 */

#ifndef _PRS_PAL_THREAD_H
#define _PRS_PAL_THREAD_H

#include <prs/pal/context.h>
#include <prs/result.h>
#include <prs/types.h>

struct prs_pal_thread;

/**
 * \brief
 *  Enumeration of the thread priorities.
 */
enum prs_pal_thread_prio {
    /** \brief Idle priority. */
    PRS_PAL_THREAD_PRIO_IDLE = 0,
    /** \brief Low priority. */
    PRS_PAL_THREAD_PRIO_LOW,
    /** \brief Normal (default) priority. */
    PRS_PAL_THREAD_PRIO_NORMAL,
    /** \brief High priority. */
    PRS_PAL_THREAD_PRIO_HIGH,
    /** \brief Real-time priority. */
    PRS_PAL_THREAD_PRIO_REALTIME
};

/**
 * \brief
 *  This structure provides the thread creation parameters that should be passed to \ref prs_pal_thread_create.
 */
struct prs_pal_thread_create_params {
    /** \brief Thread stack size. */
    prs_size_t                          stack_size;
    /** \brief Thread priority. */
    enum prs_pal_thread_prio            prio;
    /** \brief Thread affinity. */
    prs_core_mask_t                     affinity;
    /**
     * \brief
     *  If the thread should run from the calling thread, i.e. when \ref prs_pal_thread_start is called, the calling
     *  thread will run the entry point of the thread.
     */
    prs_bool_t                          from_current;
};

/**
 * \brief
 *  This structure provides the thread callback parameters that should be passed to
 *  \ref prs_pal_thread_set_callback_params.
 */
struct prs_pal_thread_callback_params {
    /** \brief Data that is passed as a parameter to the callback functions. */
    void*                               userdata;
    /** \brief Entry point of the thread. */
    void                                (*entry)(void* userdata);
    /**
     * \brief
     *  Callback to use when the thread is interrupted. The \p context parameter contains the register context of the
     *  interrupted thread. When \p context is \p null, this function call runs in the interrupted thread.
     */
    void                                (*interrupt)(void* userdata, struct prs_pal_context* context);
    /** \brief Callback to use to know if the specified thread context is interruptible. */
    prs_bool_t                          (*interruptible)(void* userdata, struct prs_pal_context* context);
};

/**
 * \brief
 *  Creates a thread.
 * \param params
 *  Creation parameters.
 */
struct prs_pal_thread* prs_pal_thread_create(struct prs_pal_thread_create_params* params);

/**
 * \brief
 *  Destroys a thread.
 * \param pal_thread
 *  Thread to destroy.
 */
prs_result_t prs_pal_thread_destroy(struct prs_pal_thread* pal_thread);

/**
 * \brief
 *  Sets the callback parameters for the thread.
 * \param pal_thread
 *  Thread to set callback parameters for.
 * \param params
 *  Callback parameters to set.
 */
prs_result_t prs_pal_thread_set_callback_params(struct prs_pal_thread* pal_thread,
    struct prs_pal_thread_callback_params* params);

/**
 * \brief
 *  Start the specified thread. If the \p from_current parameter was set to \p true when the thread was created, the
 *  entry point of the thread is called by this function.
 * \param pal_thread
 *  Thread to start.
 */
prs_result_t prs_pal_thread_start(struct prs_pal_thread* pal_thread);

/**
 * \brief
 *  Joins the specified thread. This blocks until the thread returns from its entry point.
 * \param pal_thread
 *  Thread to join.
 */
prs_result_t prs_pal_thread_join(struct prs_pal_thread* pal_thread);

/**
 * \brief
 *  Interrupt the specified thread. Once the thread is interrupted, the \p interruptible callback will be called to
 *  verify if the thread was indeed interruptible. If it was, the \p interrupt callback is used to process the
 *  interruption.
 * \param pal_thread
 *  Thread to interrupt.
 */
prs_result_t prs_pal_thread_interrupt(struct prs_pal_thread* pal_thread);

/**
 * \brief
 *  Suspends the specified thread. When suspended, the thread can only be resumed by calling
 *  \ref prs_pal_thread_resume.
 * \param pal_thread
 *  Thread to suspend.
 */
prs_result_t prs_pal_thread_suspend(struct prs_pal_thread* pal_thread);

/**
 * \brief
 *  Resumes a thread that was suspended with \ref prs_pal_thread_suspend.
 * \param pal_thread
 *  Thread to resume.
 */
prs_result_t prs_pal_thread_resume(struct prs_pal_thread* pal_thread);

#endif /* _PRS_PAL_THREAD_H */

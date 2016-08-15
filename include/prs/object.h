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
 *  This file contains the PRS object declarations.
 */

#ifndef _PRS_OBJECT_H
#define _PRS_OBJECT_H

#include <prs/types.h>

/**
 * \brief
 *  Invalid object ID.
 */
#define PRS_OBJECT_ID_INVALID           ((prs_object_id_t)0)

/**
 * \brief
 *  Standard PRS object operations. These operations are used by the global object directory.
 */
struct prs_object_ops {
    /** \brief Initiates the destruction of the object. */
    void                                (*destroy)(void* object);
    /** \brief Frees the object's data structures when it is no longer referenced. */
    void                                (*free)(void* object);
    /** \brief Prints brief information about the object. */
    void                                (*print)(void* object, void* userdata, void (*fct)(void*, const char*, ...));
};

/** \brief Process object ID type. */
typedef prs_object_id_t prs_proc_id_t;
/** \brief Task object ID type. */
typedef prs_object_id_t prs_task_id_t;
/** \brief Scheduler object ID type. */
typedef prs_object_id_t prs_sched_id_t;
/** \brief Worker object ID type. */
typedef prs_object_id_t prs_worker_id_t;
/** \brief Semaphore object ID type. */
typedef prs_object_id_t prs_sem_id_t;

#endif /* _PRS_OBJECT_H */

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
 *  This file contains the message declarations.
 */

#ifndef _PRS_MSG_H
#define _PRS_MSG_H

#include <prs/mpsciq.h>
#include <prs/msgq.h>
#include <prs/task.h>

/**
 * \brief
 *  This structure contains the header fields for all PRS messages exchanged between tasks.
 * \note
 *  This structure should only be used directly by PRS internal functions.
 */
struct prs_msg {
    /** \brief Node used when the message is in a message queue. */
    struct prs_mpsciq_node              node;
    /** \brief Owner of the message. */
    prs_task_id_t                       owner;
    /** \brief Last sender of the message. */
    prs_task_id_t                       sender;
    /** \brief Data (payload) of the message. The data may extend beyond this field. */
    prs_uint8_t                         data[PRS_PAL_POINTER_SIZE];
};

/** \brief Size of the message header. */
#define PRS_MSG_OVERHEAD                (offsetof(struct prs_msg, data))
/** \brief Returns the message header from the message payload. */
#define PRS_MSG_FROM_DATA(data)         ((struct prs_msg*)((prs_uint8_t*)data - PRS_MSG_OVERHEAD))

#endif /* _PRS_MSG_H */

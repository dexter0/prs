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
 *  This file contains the PR API message ID declarations.
 *
 *  The following declarations allow PRS services to each use unique message IDs for their communication with tasks.
 */

#ifndef _PR_MSG_H
#define _PR_MSG_H

#include <pr.h>

/** \brief Class of the message. */
enum pr_id_class {
    /** \brief Use this class for custom messages. */
    PR_ID_CLASS_USER = 0,
    /** \brief This class is used for the PR API messages. */
    PR_ID_CLASS_PR = 176
};

/**
 * \brief
 *  This macro is used to generate the class-level message ID bits.
 */
#define PR_ID_CLASS(class)              ((pr_msg_id_t)(class) << 24)

/** \brief Service of the message. */
enum pr_id_svc {
    /** \brief Process service. */
    PR_ID_SVC_PROC = 1,
    /** \brief Test service. */
    PR_ID_SVC_TEST = 2
};

/**
 * \brief
 *  This macro is used to generate the service-level message ID bits.
 */
#define PR_ID_SVC(svc)                  ((pr_msg_id_t)(svc) << 16)

/**
 * \brief
 *  This macro is used to generate the message IDs.
 */
#define PR_ID_CREATE(class, svc, id)    (PR_ID_CLASS(class) | PR_ID_SVC(svc) | (pr_msg_id_t)(id))

#endif /* _PR_MSG_H */
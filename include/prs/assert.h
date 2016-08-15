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
 *  This file contains the assertion declarations.
 */

#ifndef _PRS_ASSERT_H
#define _PRS_ASSERT_H

#include <prs/types.h>
#include <prs/config.h>

/**
 * \brief
 *  If \p expr is \p false, raises an assertion.
 * \param expr
 *  Expression that is asserted.
 * \note
 *  Assertions are only evaluated when compiling in a \p debug (\p DEBUG=1) configuration. They have no effect in a
 *  normal (\p release) configuration.
 * \see
 *  PRS_ASSERT
 *  PRS_PRECONDITION
 *  PRS_POSTCONDITION
 */
#if defined(PRS_ASSERTIONS)
#define PRS_ASSERT(expr)                do { if (!(expr)) { prs_assert(#expr, __FILE__, __LINE__); } } while (0);
#else /* PRS_ASSERTIONS */
#define PRS_ASSERT(expr)                do { (void)sizeof(expr); } while (0);
#endif /* PRS_ASSERTIONS */

/**
 * \copydoc PRS_ASSERT
 */
#define PRS_PRECONDITION(expr)          PRS_ASSERT(expr)

/**
 * \copydoc PRS_ASSERT
 */
#define PRS_POSTCONDITION(expr)         PRS_ASSERT(expr)

/**
 * \brief
 *  If \p expr is \p false, generates an error for the current compilation unit.
 * \param expr
 *  Expression that is asserted. Must be resolvable at compile-time.
 */
#if __STDC_VERSION__ >= 201112L
#include <assert.h>
#define PRS_STATIC_ASSERT(expr)         static_assert(expr, #expr)
#else /* __STDC_VERSION__ >= 201112L */
#define PRS_STATIC_ASSERT(expr)         do { enum { prs_static_assert = 1 / (expr) }; } while (0);
#endif /* __STDC_VERSION__ >= 201112L */

void prs_assert(const char* expr, const char* file, int line);

#endif /* _PRS_ASSERT_H */

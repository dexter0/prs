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
 *  This file contains the PAL assertion declarations.
 */

#ifndef _PRS_PAL_ASSERT_H
#define _PRS_PAL_ASSERT_H

/**
 * \brief
 *  This function is called by PRS to generate an assertion failure.
 * \param expr
 *  Expression that caused the assertion failure.
 * \param file
 *  File where the assertion occurred.
 * \param line
 *  Line in \p file where the assertion occurred.
 */
void prs_pal_assert(const char* expr, const char* file, int line);

#endif /* _PRS_PAL_ASSERT_H */
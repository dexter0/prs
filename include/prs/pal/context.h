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
 *  This file contains the PAL register context declarations.
 */

#ifndef _PRS_PAL_CONTEXT_H
#define _PRS_PAL_CONTEXT_H

#include <prs/types.h>

struct prs_pal_context;

/**
 * \brief
 *  Allocates a context.
 */
struct prs_pal_context* prs_pal_context_alloc(void);

/**
 * \brief
 *  Frees a context allocated by \ref prs_pal_context_alloc.
 */
void prs_pal_context_free(struct prs_pal_context* context);

/**
 * \brief
 *  Copies a context.
 * \param dst
 *  Destination context.
 * \param src
 *  Source context.
 */
void prs_pal_context_copy(struct prs_pal_context* dst, struct prs_pal_context* src);

/**
 * \brief
 *  Fills the context with the provided stack and function entry point.
 * \param context
 *  Context to write to.
 * \param stack
 *  Stack where the stack frame will be written to.
 * \param function
 *  Function that will be called once the context is restored.
 * \param argc
 *  Number of arguments of \p function.
 * \param ...
 *  Variable arguments.
 */
void prs_pal_context_make(struct prs_pal_context* context, void* stack, void (*function)(), int argc, ...);

/**
 * \brief
 *  Pushes a new stack frame on top of the stack in \p context. The stack frame is a call to \p function. When
 *  \p function returns, the stack and the register context will be identical as they were before this call.
 * \param context
 *  Context to write to.
 * \param function
 *  Function that will be called once the context is restored.
 * \param argc
 *  Number of arguments of \p function.
 * \param ...
 *  Variable arguments.
 */
void prs_pal_context_add(struct prs_pal_context* context, void (*function)(), int argc, ...);

/**
 * \brief
 *  Save the current register context in \p save and use the context in \p restore as the current register context.
 * \param save
 *  Context to save to.
 * \param restore
 *  Context to restore from.
 */
void prs_pal_context_swap(struct prs_pal_context* save, struct prs_pal_context* restore);

/**
 * \brief
 *  Returns if the current stack pointer in \p context is using the specified \p stack.
 * \param context
 *  Context to verify.
 * \param stack
 *  Top of the stack to verify.
 * \param stack_size
 *  Size of the specified \p stack.
 */
prs_bool_t prs_pal_context_is_using_stack(struct prs_pal_context* context, void* stack, prs_size_t stack_size);

/**
 * \brief
 *  Returns the value of the intruction pointer in \p context.
 * \param context
 *  Context to get the instruction pointer from.
 */
void* prs_pal_context_get_ip(struct prs_pal_context* context);

#endif /* _PRS_PAL_CONTEXT_H */

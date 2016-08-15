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
 *  This file contains the Windows register context declarations.
 */

#include <stdarg.h>
#include <stddef.h>
#include <windows.h>

#include <prs/pal/malloc.h>
#include <prs/pal/context.h>
#include <prs/assert.h>
#include <prs/error.h>

/* When adding a stack frame, we do not use the whole CONTEXT structure */
#define PRS_CONTEXT_PARTIAL_SIZE        offsetof(CONTEXT, VectorRegister)

struct prs_pal_context {
    CONTEXT                             wincontext;
};

struct prs_pal_context* prs_pal_context_alloc(void)
{
    struct prs_pal_context* context = prs_pal_malloc_zero(sizeof(struct prs_pal_context));
    PRS_FATAL_WHEN(!context || ((prs_uintptr_t)context & 0xF));
    return context;
}

void prs_pal_context_free(struct prs_pal_context* context)
{
    prs_pal_free(context);
}

void prs_pal_context_copy(struct prs_pal_context* dst, struct prs_pal_context* src)
{
    memcpy(&dst->wincontext, &src->wincontext, sizeof(dst->wincontext));
}

__asm__ (
    ".global _prs_pal_context_return\n"
    ".seh_proc _prs_pal_context_return\n"
    "_prs_pal_context_return:\n"
    "    .cfi_startproc\n"
    "    pushq %rbp\n"
    "    .seh_pushreg %rbp\n"
    "    .cfi_def_cfa_offset 16\n"
    "    .cfi_offset 6, -16\n"
    "    movq %rsp, %rbp\n"
    "    .seh_setframe %rbp, 0\n"
    "    .cfi_def_cfa_register 6\n"
    /*
     * Here, we would need to 'subq $xxx, %rsp', but we do not know exactly how much stack space is needed by the
     * called function.
     *     "    subq $xx, %rsp\n"
     *     "    .seh_stackalloc xx\n"
     */
    "    .seh_endprologue\n"
    /* The C code returns here. rbp+16 points to the CONTEXT structure that we need to restore. */
    ".global _prs_pal_context_return_trampoline\n"
    "_prs_pal_context_return_trampoline:\n"
    /* prs_pal_context_swap doesn't really care about the stack, so we don't have to create a proper stack frame */
    "    movq $0x0, %rcx\n"
    "    movq %rbp, %rdx\n"
    "    addq $0x10, %rdx\n"
    "    call prs_pal_context_swap\n"
    /*
     * Even though prs_pal_context_swap doesn't return, let's output the instructions to be in line with normal C code.
     * Because of the missing prologue above, we do not do the following:
     *     "    addq $xx, %rsp\n"
     */
    "    popq %rbp\n"
    "    ret\n"
    "    .cfi_endproc\n"
    "    .seh_endproc\n"
);

static void prs_pal_context_add_internal(struct prs_pal_context* context, void (*function)(), int argc, va_list va)
{
    PRS_PRECONDITION(context);
    PRS_PRECONDITION(function);

    va_list local_va;
    va_copy(local_va, va);

    /*
     * This function adds some information on top of the provided context's stack. Here is a rundown of the stack's
     * contents once the data has been added:
     *
     *  ^   +-----------------------------------------------+---------+
     * TOP  | Return address to prs_pal_context_return or 0     |       8 | <- rsp
     *  ^   +-----------------------------------------------+---------+
     *      | Argument #1 (rcx)                             |       8 |
     *  ^   | Argument #2 (rdx)                             |       8 |
     *      | Argument #3 (r8)                              |       8 |
     *      | Argument #4 (r9)                              |       8 |
     *      | Optional argument #5                          |       8 |
     *      | Optional argument #6                          |       8 |
     *      | ...                                           |     ... |
     *      | Alignment on 16 bytes                         |     0-8 |
     *      | Previous context rbp                          |       8 | <- rbp
     *      | Return address used for debugging             |       8 |
     *      +-----------------------------------------------+---------+
     *  ^   | (optional) Partial CONTEXT structure          |       n |
     *      | (optional) Alignment                          |     0-8 |
     *  ^   +-----------------------------------------------+---------+
     * BOT  | context's existing stack                      |     ... | <- previous context rsp
     *  ^   +-----------------------------------------------+---------+
     *
     * rbp is a callee-saved register, so it is safe to use it to store the address on the stack for the context that
     * will be restored.
     *
     * On Windows, there is no "red-zone" that would prevent us from writing on the stack.
     */
    PCONTEXT wincontext = &context->wincontext;

    /* Find the actual number of arguments on the stack */
    const int stack_argc = (argc > 4) ? argc : 4;

    /* Find the new top of the stack */
    prs_uintptr_t sp = wincontext->Rsp;

    prs_bool_t context_align = PRS_FALSE;
    if (wincontext->Rip) {
        sp -= PRS_CONTEXT_PARTIAL_SIZE;
        context_align = PRS_BOOL(sp & 0xF);
        if (context_align) {
            sp -= sizeof(prs_uintptr_t);
        }
    }

    sp -= 2 * sizeof(prs_uintptr_t);
    sp -= stack_argc * sizeof(prs_uintptr_t);
    sp -= sizeof(prs_uintptr_t);

    /*
     * To comply with the AMD64 ABI, rsp must not be aligned to a 16-byte boundary when the target function prologue is
     * executed.
     */
    const prs_bool_t frame_align = PRS_BOOL(!(sp & 0xF));
    if (frame_align) {
        sp -= sizeof(prs_uintptr_t);
    }

    /* Write the stack data, starting with the return address */
    prs_uintptr_t* psp = (prs_uintptr_t*)sp;
    PRS_ASSERT((prs_uintptr_t)psp & 0xF);
    if (wincontext->Rip) {
        extern void _prs_pal_context_return_trampoline(void);
        *psp++ = (prs_uintptr_t)_prs_pal_context_return_trampoline;
    } else {
        *psp++ = 0;
    }

    /* Fill the function arguments on the stack */
    DWORD64 new_rcx = wincontext->Rcx;
    DWORD64 new_rdx = wincontext->Rdx;
    DWORD64 new_r8 = wincontext->R8;
    DWORD64 new_r9 = wincontext->R9;
    for (int i = 0; i < stack_argc; ++i) {
        if (i < argc) {
            const prs_uintptr_t arg = va_arg(local_va, prs_uintptr_t);
            switch (i) {
                case 0:
                    new_rcx = arg;
                    break;
                case 1:
                    new_rdx = arg;
                    break;
                case 2:
                    new_r8 = arg;
                    break;
                case 3:
                    new_r9 = arg;
                    break;
            }
            *psp = arg;
        }
        ++psp;
    }

    /* Align, push rbp, save to rbp, push fake return address */
    if (frame_align) {
        PRS_ASSERT((prs_uintptr_t)psp & 0xF);
        ++psp;
    }
    const DWORD64 new_rbp = (DWORD64)psp;
    PRS_ASSERT(!(new_rbp & 0xF));
    *psp++ = (prs_uintptr_t)wincontext->Rbp;
    *psp++ = (prs_uintptr_t)wincontext->Rip;

    if (wincontext->Rip) {
        PRS_ASSERT(!((prs_uintptr_t)psp & 0xF));
        /* Copy the context */
        memcpy(psp, wincontext, PRS_CONTEXT_PARTIAL_SIZE);
        psp += PRS_CONTEXT_PARTIAL_SIZE / sizeof(prs_uintptr_t);
    }

    /* Final alignment */
    PRS_ASSERT((DWORD64)(psp + (context_align ? 1 : 0)) == wincontext->Rsp);

    /* Set the new register values */
    wincontext->Rip = (DWORD64)function;
    wincontext->Rbp = new_rbp;
    wincontext->Rsp = (DWORD64)sp;
    wincontext->Rcx = new_rcx;
    wincontext->Rdx = new_rdx;
    wincontext->R8 = new_r8;
    wincontext->R9 = new_r9;
}

void prs_pal_context_make(struct prs_pal_context* context, void* stack, void (*function)(), int argc, ...)
{
    PRS_PRECONDITION(context);
    PRS_PRECONDITION(stack);

    PCONTEXT wincontext = &context->wincontext;
#if defined(DEBUG)
    memset(wincontext, 0, sizeof(*wincontext));
#endif
    wincontext->MxCsr = (DWORD64)0x1f80; /* ldmxcsr fails if we load reserved bits */

    wincontext->Rip = (DWORD64)0;
    wincontext->Rsp = (DWORD64)stack;
    wincontext->Rbp = (DWORD64)0;

    va_list va;
    va_start(va, argc);
    prs_pal_context_add_internal(context, function, argc, va);
    va_end(va);
}

void prs_pal_context_add(struct prs_pal_context* context, void (*function)(), int argc, ...)
{
    PRS_PRECONDITION(context);
    PRS_PRECONDITION(function);

    va_list va;
    va_start(va, argc);
    prs_pal_context_add_internal(context, function, argc, va);
    va_end(va);
}

void prs_pal_context_swap(struct prs_pal_context* save, struct prs_pal_context* restore)
{
    __asm__(
        "prs_pal_context_swap_save:\n"

        /* Test is save is specified */
        "test %%rcx, %%rcx\n"
        "jz prs_pal_context_swap_restore\n"

        /* Save registers into the first context (save, rcx) */
        /* It is not required to store additional registers as the caller of this function already has taken care of
         * saving them according to the ABI. */
        "mov %%rsp, %c[rsp](%%rcx)\n"
        "mov $prs_pal_context_swap_return, %%rax\n"
        "mov %%rax, %c[rip](%%rcx)\n"
        "mov %%rbp, %c[rbp](%%rcx)\n"
        "mov %%rsi, %c[rsi](%%rcx)\n"
        "mov %%rdi, %c[rdi](%%rcx)\n"
        "mov %%rbx, %c[rbx](%%rcx)\n"
        "mov %%r12, %c[r12](%%rcx)\n"
        "mov %%r13, %c[r13](%%rcx)\n"
        "mov %%r14, %c[r14](%%rcx)\n"
        "mov %%r15, %c[r15](%%rcx)\n"
        "movaps %%xmm6, %c[xmm6](%%rcx)\n"
        "movaps %%xmm7, %c[xmm7](%%rcx)\n"
        "movaps %%xmm8, %c[xmm8](%%rcx)\n"
        "movaps %%xmm9, %c[xmm9](%%rcx)\n"
        "movaps %%xmm10, %c[xmm10](%%rcx)\n"
        "movaps %%xmm11, %c[xmm11](%%rcx)\n"
        "movaps %%xmm12, %c[xmm12](%%rcx)\n"
        "movaps %%xmm13, %c[xmm13](%%rcx)\n"
        "movaps %%xmm14, %c[xmm14](%%rcx)\n"
        "movaps %%xmm15, %c[xmm15](%%rcx)\n"
        "pushfq\n"
        "pop %%rax\n"
        "mov %%eax, %c[EFlags](%%rcx)\n"
        :
        : [rsp] "e" (offsetof(CONTEXT, Rsp)),
          [rip] "e" (offsetof(CONTEXT, Rip)),
          [rbp] "e" (offsetof(CONTEXT, Rbp)),
          [rsi] "e" (offsetof(CONTEXT, Rsi)),
          [rdi] "e" (offsetof(CONTEXT, Rdi)),
          [rbx] "e" (offsetof(CONTEXT, Rbx)),
          [r12] "e" (offsetof(CONTEXT, R12)),
          [r13] "e" (offsetof(CONTEXT, R13)),
          [r14] "e" (offsetof(CONTEXT, R14)),
          [r15] "e" (offsetof(CONTEXT, R15)),
          [xmm6] "e" (offsetof(CONTEXT, Xmm6)),
          [xmm7] "e" (offsetof(CONTEXT, Xmm7)),
          [xmm8] "e" (offsetof(CONTEXT, Xmm8)),
          [xmm9] "e" (offsetof(CONTEXT, Xmm9)),
          [xmm10] "e" (offsetof(CONTEXT, Xmm10)),
          [xmm11] "e" (offsetof(CONTEXT, Xmm11)),
          [xmm12] "e" (offsetof(CONTEXT, Xmm12)),
          [xmm13] "e" (offsetof(CONTEXT, Xmm13)),
          [xmm14] "e" (offsetof(CONTEXT, Xmm14)),
          [xmm15] "e" (offsetof(CONTEXT, Xmm15)),
          [EFlags] "e" (offsetof(CONTEXT, EFlags))
        :
    );

    __asm__(
        "prs_pal_context_swap_restore:\n"

        /* Test if restore is specified */
        "test %%rdx, %%rdx\n"
        "jz prs_pal_context_swap_return\n"

        /* Restore registers from the second context (restore, rdx) */
        /* Because we do not know how this context was saved, we have to restore more registers than those above. */
        /* rip is pushed on the stack, and ret is used to jump to the restored context's last instruction. */
        /* Restore rsp only when we are done restoring because the context might be stored on the stack. */
        "mov %c[rbp](%%rdx), %%rbp\n"
        "mov %c[rsi](%%rdx), %%rsi\n"
        "mov %c[rdi](%%rdx), %%rdi\n"
        "mov %c[rcx](%%rdx), %%rcx\n"
        "mov %c[r8](%%rdx), %%r8\n"
        "mov %c[r9](%%rdx), %%r9\n"
        "mov %c[r10](%%rdx), %%r10\n"
        "mov %c[r11](%%rdx), %%r11\n"
        "mov %c[r12](%%rdx), %%r12\n"
        "mov %c[r13](%%rdx), %%r13\n"
        "mov %c[r14](%%rdx), %%r14\n"
        "mov %c[r15](%%rdx), %%r15\n"
        "ldmxcsr %c[mxcsr](%%rdx)\n"
        :
        : [rbp] "e" (offsetof(CONTEXT, Rbp)),
          [rsi] "e" (offsetof(CONTEXT, Rsi)),
          [rdi] "e" (offsetof(CONTEXT, Rdi)),
          [rcx] "e" (offsetof(CONTEXT, Rcx)),
          [r8] "e" (offsetof(CONTEXT, R8)),
          [r9] "e" (offsetof(CONTEXT, R9)),
          [r10] "e" (offsetof(CONTEXT, R10)),
          [r11] "e" (offsetof(CONTEXT, R11)),
          [r12] "e" (offsetof(CONTEXT, R12)),
          [r13] "e" (offsetof(CONTEXT, R13)),
          [r14] "e" (offsetof(CONTEXT, R14)),
          [r15] "e" (offsetof(CONTEXT, R15)),
          [mxcsr] "e" (offsetof(CONTEXT, MxCsr))
        :
    );

    __asm__(
        "movaps %c[xmm0](%%rdx), %%xmm0\n"
        "movaps %c[xmm1](%%rdx), %%xmm1\n"
        "movaps %c[xmm2](%%rdx), %%xmm2\n"
        "movaps %c[xmm3](%%rdx), %%xmm3\n"
        "movaps %c[xmm4](%%rdx), %%xmm4\n"
        "movaps %c[xmm5](%%rdx), %%xmm5\n"
        "movaps %c[xmm6](%%rdx), %%xmm6\n"
        "movaps %c[xmm7](%%rdx), %%xmm7\n"
        "movaps %c[xmm8](%%rdx), %%xmm8\n"
        "movaps %c[xmm9](%%rdx), %%xmm9\n"
        "movaps %c[xmm10](%%rdx), %%xmm10\n"
        "movaps %c[xmm11](%%rdx), %%xmm11\n"
        "movaps %c[xmm12](%%rdx), %%xmm12\n"
        "movaps %c[xmm13](%%rdx), %%xmm13\n"
        "movaps %c[xmm14](%%rdx), %%xmm14\n"
        "movaps %c[xmm15](%%rdx), %%xmm15\n"

        /*
         * Save the new rsp in rax, and "push" rip, eflags and rdx there. This will ensure that we do not use the
         * restore context after we moved the stack pointer.
         * EFlags must be restored this late because of the 'sub' instruction used below.
         * It's possible that these pushed elements overlap with the context values, but they will only touch xmm
         * registers which have already been restored above.
         */
        "mov %c[rsp](%%rdx), %%rax\n"

        "mov %c[rip](%%rdx), %%rbx\n"
        "mov %%rbx, -0x8(%%rax)\n"

        "xor %%rbx, %%rbx\n"
        "mov %c[EFlags](%%rdx), %%ebx\n"
        "mov %%rbx, -0x10(%%rax)\n"

        "mov %c[rdx](%%rdx), %%rbx\n"
        "mov %%rbx, -0x18(%%rax)\n"

        /* Finally, restore rax, rbx, rsp, rdx, eflags and rip */
        "mov %c[rax](%%rdx), %%rax\n"
        "mov %c[rbx](%%rdx), %%rbx\n"
        "mov %c[rsp](%%rdx), %%rdx\n"
        "sub $0x18, %%rdx\n"
        "mov %%rdx, %%rsp\n"
        "pop %%rdx\n"
        "popfq\n"
        "retq\n"

        "prs_pal_context_swap_return:\n"
        :
        : [rsp] "e" (offsetof(CONTEXT, Rsp)),
          [rip] "e" (offsetof(CONTEXT, Rip)),
          [rax] "e" (offsetof(CONTEXT, Rax)),
          [rbx] "e" (offsetof(CONTEXT, Rbx)),
          [EFlags] "e" (offsetof(CONTEXT, EFlags)),
          [rdx] "e" (offsetof(CONTEXT, Rdx)),
          [xmm0] "e" (offsetof(CONTEXT, Xmm0)),
          [xmm1] "e" (offsetof(CONTEXT, Xmm1)),
          [xmm2] "e" (offsetof(CONTEXT, Xmm2)),
          [xmm3] "e" (offsetof(CONTEXT, Xmm3)),
          [xmm4] "e" (offsetof(CONTEXT, Xmm4)),
          [xmm5] "e" (offsetof(CONTEXT, Xmm5)),
          [xmm6] "e" (offsetof(CONTEXT, Xmm6)),
          [xmm7] "e" (offsetof(CONTEXT, Xmm7)),
          [xmm8] "e" (offsetof(CONTEXT, Xmm8)),
          [xmm9] "e" (offsetof(CONTEXT, Xmm9)),
          [xmm10] "e" (offsetof(CONTEXT, Xmm10)),
          [xmm11] "e" (offsetof(CONTEXT, Xmm11)),
          [xmm12] "e" (offsetof(CONTEXT, Xmm12)),
          [xmm13] "e" (offsetof(CONTEXT, Xmm13)),
          [xmm14] "e" (offsetof(CONTEXT, Xmm14)),
          [xmm15] "e" (offsetof(CONTEXT, Xmm15))
        :
    );
}

prs_bool_t prs_pal_context_is_using_stack(struct prs_pal_context* context, void* stack, prs_size_t stack_size)
{
    PCONTEXT wincontext = &context->wincontext;
    const prs_uintptr_t sp = wincontext->Rsp;
    return (sp >= (prs_uintptr_t)stack && sp < (prs_uintptr_t)stack + stack_size);
}

void* prs_pal_context_get_ip(struct prs_pal_context* context)
{
    return (void*)context->wincontext.Rip;
}

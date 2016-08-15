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
 *  This file contains the POSIX register context declarations.
 */

#include <stdarg.h>
#include <stddef.h>
#include <string.h>
#include <ucontext.h>

#include <prs/assert.h>
#include <prs/pal/malloc.h>
#include <prs/pal/context.h>
#include <prs/assert.h>
#include <prs/error.h>

#define PRS_CONTEXT_RED_ZONE_SIZE       128

struct prs_pal_context {
    ucontext_t                          ucontext;

    /*
     * Since ucontext_t is not padded, we must align the fpstate structure on a 16-byte boundary, otherwise xmm
     * operations will fail.
     */
    __attribute__((aligned(16)))
    struct _libc_fpstate                fpstate;
};

struct prs_pal_context* prs_pal_context_alloc(void)
{
    struct prs_pal_context* context = prs_pal_malloc_zero(sizeof(struct prs_pal_context));
    context->ucontext.uc_mcontext.fpregs = &context->fpstate;
    return context;
}

void prs_pal_context_free(struct prs_pal_context* context)
{
    prs_pal_free(context);
}

void prs_pal_context_copy(struct prs_pal_context* dst, struct prs_pal_context* src)
{
    memcpy(&dst->ucontext, &src->ucontext, sizeof(dst->ucontext));
    memcpy(&dst->fpstate, src->ucontext.uc_mcontext.fpregs, sizeof(dst->fpstate));
    dst->ucontext.uc_mcontext.fpregs = &dst->fpstate;
}

__asm__ (
    ".global _prs_pal_context_return\n"
    "_prs_pal_context_return:\n"
    "    .cfi_startproc\n"
    "    pushq %rbp\n"
    "    .cfi_def_cfa_offset 16\n"
    "    .cfi_offset 6, -16\n"
    "    movq %rsp, %rbp\n"
    "    .cfi_def_cfa_register 6\n"
    /*
     * Here, we would need to 'subq $xxx, %rsp', but we do not know exactly how much stack space is needed by the
     * called function.
     *     "    subq $xx, %rsp\n"
     */
    /* The C code returns here. rbp+16 points to the ucontext structure that we need to restore. */
    ".global _prs_pal_context_return_trampoline\n"
    "_prs_pal_context_return_trampoline:\n"
    /* prs_pal_context_swap doesn't really care about the stack, so we don't have to create a proper stack frame */
    "    movq $0x0, %rdi\n"
    "    movq %rbp, %rsi\n"
    "    addq $0x10, %rsi\n"
    "    call prs_pal_context_swap\n"
    /*
     * Even though prs_pal_context_swap doesn't return, let's output the instructions to be in line with normal C code.
     * Because of the missing prologue above, we do not do the following:
     *     "    addq $xx, %rsp\n"
     */
    "    popq %rbp\n"
    "    ret\n"
    "    .cfi_endproc\n"
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
     * TOP  | Return address to prs_pal_context_return or 0 |       8 | <- rsp
     *  ^   +-----------------------------------------------+---------+
     *      | Optional argument #7                          |     0-8 |
     *  ^   | Optional argument #8                          |     0-8 |
     *      | ...                                           |     ... |
     *      | Alignment on 16 bytes                         |     0-8 |
     *      | Previous context rbp                          |       8 | <- rbp
     *      | Return address used for debugging             |       8 |
     *      +-----------------------------------------------+---------+
     *      | (optional) ucontext structure                 |       n |
     *      | (optional) Alignment                          |     0-8 |
     *  ^   +-----------------------------------------------+---------+
     *      | (optional) Red Zone                           |     128 |
     *  ^   +-----------------------------------------------+---------+
     * BOT  | context's existing stack                      |     ... | <- previous context rsp
     *  ^   +-----------------------------------------------+---------+
     *
     * rbp is a callee-saved register, so it is safe to use it to store the address on the stack for the context that
     * will be restored.
     *
     */
    ucontext_t* ucontext = &context->ucontext;

    /* Find the actual number of arguments on the stack */
    const int stack_argc = (argc > 6) ? argc - 6 : 0;

    /* Find the new top of the stack */
    prs_uintptr_t sp = ucontext->uc_mcontext.gregs[REG_RSP];

    prs_bool_t context_align = PRS_FALSE;
    if (ucontext->uc_mcontext.gregs[REG_RIP]) {
        sp -= PRS_CONTEXT_RED_ZONE_SIZE; /* Red Zone */
        sp -= sizeof(*context);
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
    if (ucontext->uc_mcontext.gregs[REG_RIP]) {
        extern void _prs_pal_context_return_trampoline(void);
        *psp++ = (prs_uintptr_t)_prs_pal_context_return_trampoline;
    } else {
        *psp++ = 0;
    }

    /* Fill the function arguments on the stack */
    greg_t new_rdi = ucontext->uc_mcontext.gregs[REG_RDI];
    greg_t new_rsi = ucontext->uc_mcontext.gregs[REG_RSI];
    greg_t new_rdx = ucontext->uc_mcontext.gregs[REG_RDX];
    greg_t new_rcx = ucontext->uc_mcontext.gregs[REG_RCX];
    greg_t new_r8 = ucontext->uc_mcontext.gregs[REG_R8];
    greg_t new_r9 = ucontext->uc_mcontext.gregs[REG_R9];
    for (int i = 0; i < argc; ++i) {
        const greg_t arg = va_arg(local_va, prs_uintptr_t);
        switch (i) {
            case 0:
                new_rdi = arg;
                break;
            case 1:
                new_rsi = arg;
                break;
            case 2:
                new_rdx = arg;
                break;
            case 3:
                new_rcx = arg;
                break;
            case 4:
                new_r8 = arg;
                break;
            case 5:
                new_r9 = arg;
                break;
            default:
                *psp++ = (prs_uintptr_t)arg;
                break;
        }
    }

    /* Align, push rbp, save to rbp, push fake return address */
    if (frame_align) {
        PRS_ASSERT((prs_uintptr_t)psp & 0xF);
        ++psp;
    }
    const greg_t new_rbp = (greg_t)psp;
    *psp++ = (prs_uintptr_t)ucontext->uc_mcontext.gregs[REG_RBP];
    *psp++ = (prs_uintptr_t)ucontext->uc_mcontext.gregs[REG_RIP];

    if (ucontext->uc_mcontext.gregs[REG_RIP]) {
        /* Copy the context - we can't just memcpy() everything, as the context we received might be from a signal handler */
        struct prs_pal_context* dstcontext = (struct prs_pal_context*)psp;
        memcpy(&dstcontext->ucontext, ucontext, sizeof(dstcontext->ucontext));
        dstcontext->ucontext.uc_mcontext.fpregs = &dstcontext->fpstate;
        if (ucontext->uc_mcontext.fpregs) {
            memcpy(&dstcontext->fpstate, ucontext->uc_mcontext.fpregs, sizeof(dstcontext->fpstate));
        }
        psp += sizeof(*context) / sizeof(prs_uintptr_t);
        psp += PRS_CONTEXT_RED_ZONE_SIZE / sizeof(prs_uintptr_t);
    }

    /* Final alignment */
    PRS_ASSERT((greg_t)(psp + (context_align ? 1 : 0)) == ucontext->uc_mcontext.gregs[REG_RSP]);

    /* Set the new register values */
    ucontext->uc_mcontext.gregs[REG_RIP] = (greg_t)function;
    ucontext->uc_mcontext.gregs[REG_RBP] = new_rbp;
    ucontext->uc_mcontext.gregs[REG_RSP] = (greg_t)sp;
    ucontext->uc_mcontext.gregs[REG_RDI] = new_rdi;
    ucontext->uc_mcontext.gregs[REG_RSI] = new_rsi;
    ucontext->uc_mcontext.gregs[REG_RDX] = new_rdx;
    ucontext->uc_mcontext.gregs[REG_RCX] = new_rcx;
    ucontext->uc_mcontext.gregs[REG_R8] = new_r8;
    ucontext->uc_mcontext.gregs[REG_R9] = new_r9;
}

void prs_pal_context_make(struct prs_pal_context* context, void* stack, void (*function)(), int argc, ...)
{
    PRS_PRECONDITION(context);
    PRS_PRECONDITION(stack);

    ucontext_t* ucontext = &context->ucontext;
#if defined(DEBUG)
    memset(ucontext, 0, sizeof(*ucontext));
    ucontext->uc_mcontext.fpregs = &context->fpstate;
#endif
    ucontext->uc_mcontext.fpregs->mxcsr = (__uint32_t)0x1f80; /* ldmxcsr fails if we load reserved bits */

    ucontext->uc_mcontext.gregs[REG_RIP] = (greg_t)0;
    ucontext->uc_mcontext.gregs[REG_RSP] = (greg_t)stack;
    ucontext->uc_mcontext.gregs[REG_RBP] = (greg_t)0;

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
#define PRS_CONTEXT_XMM(i)              offsetof(struct _libc_fpstate, _xmm[i])
    __asm__(
        "prs_pal_context_swap_save:\n"

        /* Test is save is specified */
        "test %%rdi, %%rdi\n"
        "jz prs_pal_context_swap_restore\n"

        /* Save registers into the first context (save, rdi) */
        /* It is not required to store additional registers as the caller of this function already has taken care of
         * saving them according to the ABI. */
        "mov %%rsp, %c[rsp](%%rdi)\n"
        /*
         * Here, we cannot put the prs_pal_context_swap_return address directly in rax if we want to compile position-
         * independent code (-fPIC). Instead, we use a RIP relative addressing trick to get the address instead.
         */
        "lea (%%rip), %%rax\n"
        "rip_ref:\n"
        "add $(prs_pal_context_swap_return - rip_ref), %%rax\n"
        "mov %%rax, %c[rip](%%rdi)\n"
        "mov %%rbp, %c[rbp](%%rdi)\n"
        "mov %%rbx, %c[rbx](%%rdi)\n"
        "mov %%r12, %c[r12](%%rdi)\n"
        "mov %%r13, %c[r13](%%rdi)\n"
        "mov %%r14, %c[r14](%%rdi)\n"
        "mov %%r15, %c[r15](%%rdi)\n"
        "pushfq\n"
        "pop %%rax\n"
        "mov %%eax, %c[eflags](%%rdi)\n"
        "mov %c[fpstate](%%rdi), %%rdi\n"
        "movaps %%xmm8, %c[xmm8](%%rdi)\n"
        "movaps %%xmm9, %c[xmm9](%%rdi)\n"
        "movaps %%xmm10, %c[xmm10](%%rdi)\n"
        "movaps %%xmm11, %c[xmm11](%%rdi)\n"
        "movaps %%xmm12, %c[xmm12](%%rdi)\n"
        "movaps %%xmm13, %c[xmm13](%%rdi)\n"
        "movaps %%xmm14, %c[xmm14](%%rdi)\n"
        "movaps %%xmm15, %c[xmm15](%%rdi)\n"
        :
        : [rsp] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_RSP])),
          [rip] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_RIP])),
          [rbp] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_RBP])),
          [rbx] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_RBX])),
          [r12] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_R12])),
          [r13] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_R13])),
          [r14] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_R14])),
          [r15] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_R15])),
          [eflags] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_EFL])),
          [fpstate] "e" (offsetof(ucontext_t, uc_mcontext.fpregs)),
          [xmm8] "e" (PRS_CONTEXT_XMM(8)),
          [xmm9] "e" (PRS_CONTEXT_XMM(9)),
          [xmm10] "e" (PRS_CONTEXT_XMM(10)),
          [xmm11] "e" (PRS_CONTEXT_XMM(11)),
          [xmm12] "e" (PRS_CONTEXT_XMM(12)),
          [xmm13] "e" (PRS_CONTEXT_XMM(13)),
          [xmm14] "e" (PRS_CONTEXT_XMM(14)),
          [xmm15] "e" (PRS_CONTEXT_XMM(15))
        :
    );

    __asm__(
        "prs_pal_context_swap_restore:\n"

        /* Test if restore is specified */
        "test %%rsi, %%rsi\n"
        "jz prs_pal_context_swap_return\n"

        /* Restore registers from the second context (restore, rsi) */
        /* Because we do not know how this context was saved, we have to restore more registers than those above. */
        /* rip is pushed on the stack, and ret is used to jump to the restored context's last instruction. */
        /* Restore rsp only when we are done restoring because the context might be stored on the stack. */
        "mov %c[fpstate](%%rsi), %%rax\n"
        "ldmxcsr %c[mxcsr](%%rax)\n"
        "movaps %c[xmm0](%%rax), %%xmm0\n"
        "movaps %c[xmm1](%%rax), %%xmm1\n"
        "movaps %c[xmm2](%%rax), %%xmm2\n"
        "movaps %c[xmm3](%%rax), %%xmm3\n"
        "movaps %c[xmm4](%%rax), %%xmm4\n"
        "movaps %c[xmm5](%%rax), %%xmm5\n"
        "movaps %c[xmm6](%%rax), %%xmm6\n"
        "movaps %c[xmm7](%%rax), %%xmm7\n"
        "movaps %c[xmm8](%%rax), %%xmm8\n"
        "movaps %c[xmm9](%%rax), %%xmm9\n"
        "movaps %c[xmm10](%%rax), %%xmm10\n"
        "movaps %c[xmm11](%%rax), %%xmm11\n"
        "movaps %c[xmm12](%%rax), %%xmm12\n"
        "movaps %c[xmm13](%%rax), %%xmm13\n"
        "movaps %c[xmm14](%%rax), %%xmm14\n"
        "movaps %c[xmm15](%%rax), %%xmm15\n"

        :
        : [fpstate] "e" (offsetof(ucontext_t, uc_mcontext.fpregs)),
          [mxcsr] "e" (offsetof(struct _libc_fpstate, mxcsr)),
          [xmm0] "e" (PRS_CONTEXT_XMM(0)),
          [xmm1] "e" (PRS_CONTEXT_XMM(1)),
          [xmm2] "e" (PRS_CONTEXT_XMM(2)),
          [xmm3] "e" (PRS_CONTEXT_XMM(3)),
          [xmm4] "e" (PRS_CONTEXT_XMM(4)),
          [xmm5] "e" (PRS_CONTEXT_XMM(5)),
          [xmm6] "e" (PRS_CONTEXT_XMM(6)),
          [xmm7] "e" (PRS_CONTEXT_XMM(7)),
          [xmm8] "e" (PRS_CONTEXT_XMM(8)),
          [xmm9] "e" (PRS_CONTEXT_XMM(9)),
          [xmm10] "e" (PRS_CONTEXT_XMM(10)),
          [xmm11] "e" (PRS_CONTEXT_XMM(11)),
          [xmm12] "e" (PRS_CONTEXT_XMM(12)),
          [xmm13] "e" (PRS_CONTEXT_XMM(13)),
          [xmm14] "e" (PRS_CONTEXT_XMM(14)),
          [xmm15] "e" (PRS_CONTEXT_XMM(15))
        :
    );

    __asm__(
        "mov %c[rbp](%%rsi), %%rbp\n"
        "mov %c[rdi](%%rsi), %%rdi\n"
        "mov %c[rcx](%%rsi), %%rcx\n"
        "mov %c[rdx](%%rsi), %%rdx\n"
        "mov %c[r8](%%rsi), %%r8\n"
        "mov %c[r9](%%rsi), %%r9\n"
        "mov %c[r10](%%rsi), %%r10\n"
        "mov %c[r11](%%rsi), %%r11\n"
        "mov %c[r12](%%rsi), %%r12\n"
        "mov %c[r13](%%rsi), %%r13\n"
        "mov %c[r14](%%rsi), %%r14\n"
        "mov %c[r15](%%rsi), %%r15\n"

        /*
         * Save the new rsp in rax, and "push" rip, eflags and rsi there. This will ensure that we do not use the
         * restore context after we moved the stack pointer.
         * eflags must be restored this late because of the 'sub' instruction used below.
         * Unfortunately, these pushed elements overlap with the red zone.
         */
        "mov %c[rsp](%%rsi), %%rax\n"

        "mov %c[rip](%%rsi), %%rbx\n"
        "mov %%rbx, -0x8(%%rax)\n"

        "xor %%rbx, %%rbx\n"
        "mov %c[eflags](%%rsi), %%ebx\n"
        "mov %%rbx, -0x10(%%rax)\n"

        "mov %c[rsi](%%rsi), %%rbx\n"
        "mov %%rbx, -0x18(%%rax)\n"

        /* Finally, restore rax, rbx, rsp, rsi, eflags and rip */
        "mov %c[rax](%%rsi), %%rax\n"
        "mov %c[rbx](%%rsi), %%rbx\n"
        "mov %c[rsp](%%rsi), %%rsi\n"
        "sub $0x18, %%rsi\n"
        "mov %%rsi, %%rsp\n"
        "pop %%rsi\n"
        "popfq\n"
        "retq\n"

        "prs_pal_context_swap_return:\n"
        :
        : [rip] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_RIP])),
          [rsp] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_RSP])),
          [rbp] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_RBP])),
          [rsi] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_RSI])),
          [rdi] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_RDI])),
          [rax] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_RAX])),
          [rbx] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_RBX])),
          [rcx] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_RCX])),
          [rdx] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_RDX])),
          [r8] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_R8])),
          [r9] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_R9])),
          [r10] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_R10])),
          [r11] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_R11])),
          [r12] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_R12])),
          [r13] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_R13])),
          [r14] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_R14])),
          [r15] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_R15])),
          [eflags] "e" (offsetof(ucontext_t, uc_mcontext.gregs[REG_EFL]))
        :
    );
}

prs_bool_t prs_pal_context_is_using_stack(struct prs_pal_context* context, void* stack, prs_size_t stack_size)
{
    ucontext_t* ucontext = &context->ucontext;
    const prs_uintptr_t sp = ucontext->uc_mcontext.gregs[REG_RSP];
    return (sp >= (prs_uintptr_t)stack && sp < (prs_uintptr_t)stack + stack_size);
}

void* prs_pal_context_get_ip(struct prs_pal_context* context)
{
    return (void*)context->ucontext.uc_mcontext.gregs[REG_RIP];
}

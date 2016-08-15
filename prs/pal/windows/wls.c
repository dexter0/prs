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
 *  This file contains the Windows worker local storage definitions.
 */

#include <windows.h>

#include <prs/pal/arch.h>
#include <prs/pal/compiler.h>
#include <prs/pal/inline.h>
#include <prs/pal/wls.h>

/* https://en.wikipedia.org/wiki/Win32_Thread_Information_Block */
typedef struct _TEB {
    BYTE Reserved1[1952];
    PVOID Reserved2[412];
    PVOID TlsSlots[64];
    BYTE Reserved3[8];
    PVOID Reserved4[26];
    PVOID ReservedForOle;
    PVOID Reserved5[4];
    PVOID TlsExpansionSlots;
} TEB;
typedef TEB *PTEB;

static prs_int_t s_prs_wls_tls_slot = 0;

static PRS_INLINE PTEB prs_wls_get_teb(void)
{
    /* http://www.nynaeve.net/?p=185 */
    /*
     * On x86, the TEB is located at fs:0x18.
     * On AMD64, the TEB is located at gs:0x30 (0x30 is 0x18 multiplied by 2 because of the larger pointer size).
     */
    PTEB teb = 0;
    __asm__(
        "movq %%gs:0x30, %%rcx\n"
        "movq %%rcx, %0\n"
        : "=r" (teb)
        :
        : "rcx"
    );
    return teb;
}

prs_result_t prs_wls_init(void)
{
    s_prs_wls_tls_slot = (prs_int_t)TlsAlloc();
    return PRS_OK;
}

void prs_wls_uninit(void)
{
}

prs_result_t prs_wls_worker_init(struct prs_worker* worker)
{
    return PRS_OK;
}

void prs_wls_worker_uninit(struct prs_worker* worker)
{
}

void prs_wls_set(void* data)
{
    /* TlsSetValue(s_prs_wls_tls_slot, data); */
    PTEB teb = prs_wls_get_teb();
    teb->TlsSlots[s_prs_wls_tls_slot] = data;
}

void* prs_wls_get(void)
{
    void* result;

    __asm__(
        "movq %%gs:0x30, %%rcx\n"
        "movq %[index], %%rdx\n"
        "movq %c[offset](%%rcx, %%rdx, %c[size]), %[result]\n"
        : [result] "=r" (result)
        : [offset] "e" (offsetof(TEB, TlsSlots)),
          [index] "r" (s_prs_wls_tls_slot),
          [size] "e" (PRS_PAL_POINTER_SIZE)
        : "rcx", "rdx"
    );

    return result;

    /* return TlsGetValue(s_prs_wls_tls_slot); */
    /*
        PTEB teb = prs_wls_get_teb();
        return teb->TlsSlots[s_prs_wls_tls_slot];
    */
}


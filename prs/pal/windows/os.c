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
 *  This file contains the Windows operating system definitions.
 */

#include <stdio.h>
#include <windows.h>

#include <prs/pal/os.h>
#include <prs/pal/malloc.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/excp.h>
#include <prs/str.h>
#include <prs/types.h>

static SYSTEM_INFO s_prs_pal_windows_system_info;
static prs_size_t s_prs_pal_windows_huge_page_size;
static char* s_prs_pal_windows_version = 0;
static char* s_prs_pal_windows_computer = 0;

static prs_bool_t prs_pal_os_init_large_pages(void)
{
    HANDLE token = 0;
    TOKEN_PRIVILEGES tp;
    BOOL result;

    result = OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token);
    if (!result) {
        return PRS_FALSE;
    }

    result = LookupPrivilegeValue(NULL, "SeLockMemoryPrivilege", &tp.Privileges[0].Luid);
    if (!result) {
        goto cleanup;
    }

    tp.PrivilegeCount = 1;
    tp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    result = AdjustTokenPrivileges(token, FALSE, &tp, 0, 0, 0);

    cleanup:
    CloseHandle(token);

    return PRS_BOOL(result);
}

static void prs_pal_os_get_windows_version(void)
{
    PRS_PRECONDITION(!s_prs_pal_windows_version);

    const char* kernel32 = "\\kernel32.dll";
    const prs_size_t kernel32_len = strlen(kernel32);
    const prs_size_t max_sd_size = PRS_MAX_PATH - kernel32_len;
    char path[PRS_MAX_PATH];
    const UINT sd_size = GetSystemDirectoryA(path, max_sd_size);
    PRS_FATAL_WHEN(sd_size == 0 || sd_size >= max_sd_size);

    prs_str_append(path, kernel32, sd_size, PRS_MAX_PATH);

    const DWORD fvi_size = GetFileVersionInfoSizeA(path, NULL);
    PRS_FATAL_WHEN(!fvi_size);

    void* fvi = prs_pal_malloc(fvi_size);
    PRS_FATAL_WHEN(!fvi);

    BOOL result = GetFileVersionInfoA(path, 0, fvi_size, fvi);
    PRS_FATAL_WHEN(!result);

    void* value;
    UINT value_size;
    result = VerQueryValueA(fvi, "\\", &value, &value_size);
    PRS_FATAL_WHEN(!result || value_size < sizeof(VS_FIXEDFILEINFO));

    const prs_size_t version_string_size = 64;
    VS_FIXEDFILEINFO* ffi = value;
    s_prs_pal_windows_version = prs_pal_malloc(version_string_size);
    prs_str_printf(s_prs_pal_windows_version, version_string_size, "%u.%u.%u",
        HIWORD(ffi->dwProductVersionMS),
        LOWORD(ffi->dwProductVersionMS),
        HIWORD(ffi->dwProductVersionLS));

    prs_pal_free(fvi);
}

static void prs_pal_os_get_windows_computer(void)
{
    PRS_PRECONDITION(!s_prs_pal_windows_computer);

    DWORD computer_string_size = 64;
    s_prs_pal_windows_computer = prs_pal_malloc(computer_string_size);

    const BOOL result = GetComputerNameA(s_prs_pal_windows_computer, &computer_string_size);
    PRS_FATAL_WHEN(!result || computer_string_size == 0);
}

static BOOL prs_pal_os_user_interrupt(DWORD dwCtrlType)
{
    prs_excp_raise(PRS_EXCP_TYPE_USER_INTERRUPT, (void*)(prs_uintptr_t)dwCtrlType, 0, 0);
    return PRS_TRUE;
}

void prs_pal_os_init(void)
{
    const BOOL result = SetConsoleCtrlHandler(prs_pal_os_user_interrupt, TRUE);
    if (!result) {
        fprintf(stderr, "SetConsoleCtrlHandler() failed\n");
        exit(-1);
    }

    GetSystemInfo(&s_prs_pal_windows_system_info);

    const prs_bool_t has_large_pages = prs_pal_os_init_large_pages();
    if (has_large_pages) {
        s_prs_pal_windows_huge_page_size = GetLargePageMinimum();
    } else {
        s_prs_pal_windows_huge_page_size = prs_pal_os_get_page_size();
    }

    prs_pal_os_get_windows_version();
    prs_pal_os_get_windows_computer();

    /*
     * Workaround for Eclipse CDT console
     * https://bugs.eclipse.org/bugs/show_bug.cgi?id=173732
     */
#if defined(DEBUG)
    setvbuf(stdout, NULL, _IONBF, 0);
    setvbuf(stderr, NULL, _IONBF, 0);
#endif /* DEBUG */
}

void prs_pal_os_uninit(void)
{

}

prs_size_t prs_pal_os_get_core_count(void)
{
    return s_prs_pal_windows_system_info.dwNumberOfProcessors;
}

prs_size_t prs_pal_os_get_page_size(void)
{
    return s_prs_pal_windows_system_info.dwPageSize;
}

prs_size_t prs_pal_os_get_huge_page_size(void)
{
    return s_prs_pal_windows_huge_page_size;
}

const char* prs_pal_os_get_version(void)
{
    PRS_PRECONDITION(s_prs_pal_windows_version);
    return s_prs_pal_windows_version;
}

const char* prs_pal_os_get_computer(void)
{
    PRS_PRECONDITION(s_prs_pal_windows_computer);
    return s_prs_pal_windows_computer;
}

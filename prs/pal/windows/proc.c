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
 *  This file contains the Windows process definitions.
 */

#include <stdio.h>
#include <windows.h>
#include <psapi.h>

#include <prs/pal/bitops.h>
#include <prs/pal/malloc.h>
#include <prs/pal/mem.h>
#include <prs/pal/os.h>
#include <prs/pal/proc.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/log.h>
#include <prs/str.h>

#if 0
#define PRS_PAL_PROC_DEBUG_LOG(...) prs_log_print(__VA_ARGS__)
#else
#define PRS_PAL_PROC_DEBUG_LOG(...)
#endif

struct prs_pal_proc {
    prs_size_t                          size;
    void*                               data;

    void*                               entry;
};

struct prs_pal_proc* prs_pal_proc_create_main(char* filename, char* cmdline)
{
    struct prs_pal_proc* proc = prs_pal_malloc_zero(sizeof(*proc));

    DWORD result = GetModuleFileName(0, filename, PRS_MAX_PATH);
    PRS_FATAL_WHEN(!result);

    prs_str_copy(cmdline, GetCommandLineA(), PRS_MAX_CMDLINE);

    /*
     * Workaround for Eclipse CDT Neon, which adds quotes to arguments
     * https://bugs.eclipse.org/bugs/show_bug.cgi?id=494246
     */
#if defined(DEBUG)
    prs_log_print("Removing quotes from command line arguments");
    prs_log_print("Before: %s", cmdline);
    char* dst = cmdline;
    for (char* src = dst; *src; ++src) {
        if (*src == '\'') {
            continue;
        }
        *dst++ = *src;
    }
    *dst = '\0';
    prs_log_print("After : %s", cmdline);
#endif /* DEBUG */

    const HANDLE current_process = GetCurrentProcess();
    const HANDLE current_module = GetModuleHandle(0);
    MODULEINFO info;
    result = GetModuleInformation(current_process, current_module, &info, sizeof(info));
    PRS_FATAL_WHEN(!result);
    proc->data = info.lpBaseOfDll;
    proc->size = info.SizeOfImage;
    prs_log_print("PRS allocated at %p (%lu bytes)", proc->data, proc->size);

    return proc;
}

struct prs_pal_proc* prs_pal_proc_load(struct prs_pal_proc_load_params* params)
{
    HANDLE hFile = 0;
    HANDLE hMap = 0;

    struct prs_pal_proc* proc = prs_pal_malloc_zero(sizeof(*proc));

    hFile = CreateFile(params->filename, GENERIC_READ,
        FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
    if (hFile == INVALID_HANDLE_VALUE) {
        PRS_FTRACE("CreateFile failed");
        goto out;
    }

    hMap = CreateFileMapping(hFile, NULL, PAGE_READONLY, 0, 0, NULL);
    if (hMap == NULL) {
        PRS_FTRACE("CreateFileMapping failed");
        goto out;
    }

    void* file_data = MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, 0);
    if (!file_data) {
        PRS_FTRACE("MapViewOfFile failed");
        goto out;
    }

    PIMAGE_DOS_HEADER pDosHeader = (PIMAGE_DOS_HEADER)file_data;
    if (pDosHeader->e_magic != IMAGE_DOS_SIGNATURE) {
        PRS_FTRACE("DOS signature invalid");
        goto out;
    }

    PIMAGE_NT_HEADERS pNtHeaders = (PIMAGE_NT_HEADERS)((prs_uintptr_t)file_data + pDosHeader->e_lfanew);
    if (pNtHeaders->Signature != IMAGE_NT_SIGNATURE) {
        PRS_FTRACE("NT signature mismatch");
        goto out;
    }

    PRS_PAL_PROC_DEBUG_LOG("Target PE Load Base: 0x%p Image Size: 0x%lx",
                (void*)pNtHeaders->OptionalHeader.ImageBase,
                pNtHeaders->OptionalHeader.SizeOfImage);

    PRS_PAL_PROC_DEBUG_LOG("File mapping at %p", file_data);

    const prs_size_t page_size = prs_pal_os_get_page_size();
    proc->size = prs_bitops_align_size(pNtHeaders->OptionalHeader.SizeOfImage + 1, page_size);
    proc->data = prs_pal_mem_map(proc->size, PRS_PAL_MEM_FLAG_COMMIT |
        PRS_PAL_MEM_FLAG_READ | PRS_PAL_MEM_FLAG_WRITE | PRS_PAL_MEM_FLAG_EXECUTE);
    if (!proc->data) {
        PRS_FTRACE("VirtualAlloc(%lu) failed", proc->size);
        goto out;
    }
    prs_log_print("%s: allocated %p (%lu bytes)", params->filename, proc->data, pNtHeaders->OptionalHeader.SizeOfImage + 1);
    CopyMemory(proc->data, file_data, pNtHeaders->OptionalHeader.SizeOfHeaders);

    for (int i = 0; i < pNtHeaders->FileHeader.NumberOfSections; ++i) {
        PIMAGE_SECTION_HEADER section_header = &IMAGE_FIRST_SECTION(pNtHeaders)[i];
        if (section_header->SizeOfRawData) {
            void* mem_dst = (void*)((prs_uintptr_t)proc->data + section_header->VirtualAddress);
            PRS_PAL_PROC_DEBUG_LOG("Copying Section: %s", section_header->Name);
            memcpy(mem_dst, (void*)((prs_uintptr_t)file_data + section_header->PointerToRawData), section_header->SizeOfRawData);

#if defined(DEBUG)
            if (!strcmp((CHAR*) section_header->Name, ".text")) {
                char path[PRS_MAX_PATH];
                char* dst = path;
                char* src = params->filename;
                prs_bool_t backslash = PRS_FALSE;
                for (int i = 0; i < PRS_MAX_PATH - 1 && *src; ++i) {
                    if (*src == '\\') {
                        backslash = PRS_BOOL(!backslash);
                        *dst++ = *src; /* Eclipse console requires 4 backslashes! */
                    }
                    *dst++ = *src;
                    if (!backslash) {
                        ++src;
                    }
                }
                *dst = '\0';

                printf("### add-symbol-file \"%s\" 0x%p\n", path, mem_dst);
            }
#endif /* DEBUG */
        }
    }

    CloseHandle(hMap);
    hMap = 0;
    CloseHandle(hFile);
    hFile = 0;

    pDosHeader = (PIMAGE_DOS_HEADER)proc->data;
    pNtHeaders = (PIMAGE_NT_HEADERS)((prs_uintptr_t)proc->data + pDosHeader->e_lfanew);

    PIMAGE_DATA_DIRECTORY reloc_dir = &pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if ((prs_uintptr_t)proc->data != pNtHeaders->OptionalHeader.ImageBase) {
        if (!reloc_dir->Size) {
            PRS_FTRACE("Can't relocate, this isn't going to work");
            goto out;
        }

        const ptrdiff_t delta = (prs_uintptr_t)proc->data - pNtHeaders->OptionalHeader.ImageBase;
        PRS_PAL_PROC_DEBUG_LOG("Applying relocation from %p to %p",
            (void*)pNtHeaders->OptionalHeader.ImageBase,
            proc->data);

        PRS_PAL_PROC_DEBUG_LOG("Relocation data: %lu bytes", reloc_dir->Size);

        prs_uintptr_t reloc_data = (prs_uintptr_t)proc->data + reloc_dir->VirtualAddress;
        const prs_uintptr_t reloc_end = reloc_data + reloc_dir->Size;
        for (PIMAGE_BASE_RELOCATION reloc = (PIMAGE_BASE_RELOCATION)reloc_data;
            (prs_uintptr_t)reloc < reloc_end;
            reloc = (PIMAGE_BASE_RELOCATION)((prs_uintptr_t)reloc + reloc->SizeOfBlock))
        {
            const int count = (reloc->SizeOfBlock - sizeof(*reloc)) / sizeof(WORD);
            const prs_uintptr_t base_dest = (prs_uintptr_t)proc->data + reloc->VirtualAddress;
            prs_uint16_t* entries = (prs_uint16_t*)((prs_uintptr_t)reloc + sizeof(*reloc));
            PRS_PAL_PROC_DEBUG_LOG("Relocation block at virt=%p of %u entries (%lu bytes)", reloc, count, reloc->SizeOfBlock);
            for (int i = 0; i < count; ++i) {
                const prs_uint16_t entry = entries[i];
                const prs_uint16_t type = (entry >> 12) & 0xF;
                const prs_uint16_t offset = entry & 0xFFF;
                const prs_uintptr_t dest = base_dest + offset;
                switch (type) {
                    case IMAGE_REL_BASED_ABSOLUTE:
                        /* Nothing to do */
                        break;
                    case IMAGE_REL_BASED_HIGH:
                        *((prs_uint16_t*)dest) += (prs_uint32_t)delta;
                        break;
                    case IMAGE_REL_BASED_LOW:
                        *((prs_uint16_t*)dest) += (prs_uint16_t)LOWORD((prs_uint16_t)delta);
                        break;
                    case IMAGE_REL_BASED_HIGHLOW:
                        *((prs_uint32_t*)dest) += (prs_uint16_t)HIWORD((prs_uint16_t)delta);
                        break;
                    //case IMAGE_REL_BASED_HIGHADJ:
                    //case IMAGE_REL_BASED_MIPS_JMPADDR:
                    //case IMAGE_REL_BASED_ARM_MOV32:
                    //case IMAGE_REL_BASED_THUMB_MOV32:
                    //case IMAGE_REL_BASED_MIPS_JMPADDR16:
                    //case IMAGE_REL_BASED_IA64_IMM64:
                    case IMAGE_REL_BASED_DIR64:
                        *((prs_uint64_t*)dest) += delta;
                        break;
                    default:
                        PRS_FTRACE("Unknown relocation type %d", type);
                        break;
                }
            }

            PRS_ASSERT((prs_uintptr_t)reloc + reloc->SizeOfBlock <= reloc_end);
        }
    }

    PIMAGE_IMPORT_DESCRIPTOR pImportDesc = (PIMAGE_IMPORT_DESCRIPTOR)((prs_uintptr_t)proc->data +
        (prs_uintptr_t)pNtHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);

    if (!pImportDesc) {
        PRS_FTRACE("No directory entry import descriptor found");
        goto out;
    }

    while (pImportDesc && pImportDesc->Name) {
        const char* name = (const char*)proc->data + pImportDesc->Name;
        PRS_PAL_PROC_DEBUG_LOG("Import DLL: %s", name);
        HMODULE hMod = GetModuleHandle(name);
        if (!hMod) {
            if (!strcmp(name, "prs.dll")) {
                PRS_PAL_PROC_DEBUG_LOG("  trying prs.exe instead");
                hMod = GetModuleHandle("prs.exe");
            }
            if (!hMod) {
                PRS_PAL_PROC_DEBUG_LOG("  not already loaded");
                hMod = LoadLibrary(name);
            }
        }
        if (!hMod) {
            PRS_FTRACE("Couln't load DLL %s", name);
            goto out;
        }

        PIMAGE_THUNK_DATA pThunkData = (PIMAGE_THUNK_DATA)((prs_uintptr_t)proc->data + pImportDesc->FirstThunk);
        PIMAGE_THUNK_DATA pThunkDataRef = (pImportDesc->OriginalFirstThunk) ?
            (PIMAGE_THUNK_DATA)((prs_uintptr_t)proc->data + pImportDesc->OriginalFirstThunk) :
            pThunkData;
        while (pThunkDataRef->u1.AddressOfData) {
            if (IMAGE_SNAP_BY_ORDINAL(pThunkDataRef->u1.Ordinal)) {
                pThunkData->u1.Function = (prs_uintptr_t)GetProcAddress(hMod, (LPCSTR)IMAGE_ORDINAL(pThunkDataRef->u1.Ordinal));
            } else {
                PIMAGE_IMPORT_BY_NAME import_by_name = (PIMAGE_IMPORT_BY_NAME)((prs_uintptr_t)proc->data + pThunkDataRef->u1.ForwarderString);
                pThunkData->u1.Function = (prs_uintptr_t)GetProcAddress(hMod, (LPCSTR)&import_by_name->Name);
                PRS_PAL_PROC_DEBUG_LOG("  Loaded thunk %s = %p", (LPCSTR)&import_by_name->Name, (void*)pThunkData->u1.Function);
            }
            ++pThunkDataRef;
            ++pThunkData;
        }

        pImportDesc++;
    }

    for (int i = 0; i < pNtHeaders->FileHeader.NumberOfSections; ++i) {
        PIMAGE_SECTION_HEADER section_header = &IMAGE_FIRST_SECTION(pNtHeaders)[i];
        if (section_header->SizeOfRawData) {
            void* dst = (void*)((prs_uintptr_t)proc->data + section_header->VirtualAddress);
            prs_pal_mem_flags_t flags = PRS_PAL_MEM_FLAG_NONE;
            if (section_header->Characteristics & IMAGE_SCN_MEM_READ) {
                flags |= PRS_PAL_MEM_FLAG_READ;
            }
            if (section_header->Characteristics & IMAGE_SCN_MEM_WRITE) {
                flags |= PRS_PAL_MEM_FLAG_WRITE;
            }
            if (section_header->Characteristics & IMAGE_SCN_MEM_EXECUTE) {
                flags |= PRS_PAL_MEM_FLAG_EXECUTE;
            }
            PRS_PAL_PROC_DEBUG_LOG("Changing protection on section: %s (0x%X)", section_header->Name, flags);
            prs_pal_mem_protect(dst, section_header->SizeOfRawData, flags);
        }
    }

    proc->entry = (void*)((prs_uintptr_t)proc->data + pNtHeaders->OptionalHeader.AddressOfEntryPoint);

    return proc;

    out:

    if (hMap) {
        CloseHandle(hMap);
    }
    if (hFile) {
        CloseHandle(hFile);
    }

    return 0;
}

void prs_pal_proc_destroy(struct prs_pal_proc* pal_proc)
{
    PRS_PAL_PROC_DEBUG_LOG("Unmapping %p (%u bytes)", pal_proc->data, pal_proc->size);
    prs_pal_mem_unmap(pal_proc->data, pal_proc->size);
    prs_pal_free(pal_proc);
}

void* prs_pal_proc_get_entry_point(struct prs_pal_proc* pal_proc)
{
    return pal_proc->entry;
}

void* prs_pal_proc_get_base(struct prs_pal_proc* pal_proc)
{
    return pal_proc->data;
}

prs_size_t prs_pal_proc_get_size(struct prs_pal_proc* pal_proc)
{
    return pal_proc->size;
}

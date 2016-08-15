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
 *  This file contains the Linux process definitions.
 */

#include <dlfcn.h>
#include <fcntl.h>
#include <link.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>

#include <prs/pal/arch.h>
#include <prs/pal/bitops.h>
#include <prs/pal/malloc.h>
#include <prs/pal/mem.h>
#include <prs/pal/os.h>
#include <prs/pal/proc.h>
#include <prs/assert.h>
#include <prs/error.h>
#include <prs/log.h>
#include <prs/str.h>

#if PRS_PAL_ARCH != PRS_PAL_ARCH_AMD64
#error Only AMD64 is supported
#endif

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

static int prs_pal_proc_find_base_address(struct dl_phdr_info* info, size_t size, void* data)
{
    struct prs_pal_proc* proc = data;

    /* Find the ELF section that contains the code of the prs_pal_proc_find_base_address function */
    const prs_uintptr_t addr = (prs_uintptr_t)prs_pal_proc_find_base_address;
    for (int i = 0; i < info->dlpi_phnum; ++i) {
        const ElfW(Phdr)* phdr = &info->dlpi_phdr[i];
        if (phdr->p_type == PT_LOAD) {
            const prs_uintptr_t base = (prs_uintptr_t)info->dlpi_addr + phdr->p_vaddr;
            if (addr >= base && addr < base + phdr->p_memsz) {
                proc->data = (void*)base;
                proc->size = phdr->p_memsz;
                return 1;
            }
        }
    }
    return 0;
}

struct prs_pal_proc* prs_pal_proc_create_main(char* filename, char* cmdline)
{
    struct prs_pal_proc* proc = prs_pal_malloc_zero(sizeof(*proc));

    extern char* __progname;
    prs_str_copy(filename, __progname, PRS_MAX_PATH);

    FILE* fp = fopen("/proc/self/cmdline", "r");
    char* pcmdline = cmdline;
    for (int i = 0; i < PRS_MAX_CMDLINE - 1; ++i) {
        int c = fgetc(fp);
        if (c == EOF) {
            *pcmdline++ = '\0';
            break;
        } else if (c == '\0') {
            c = ' ';
        }
        *pcmdline++ = c;
    }
    cmdline[PRS_MAX_CMDLINE - 1] = '\0';
    fclose(fp);

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

    const int found = dl_iterate_phdr(prs_pal_proc_find_base_address, proc);
    PRS_FATAL_WHEN(!found);
    prs_log_print("PRS allocated at %p (%lu bytes)", proc->data, proc->size);

    return proc;
}

static void* prs_pal_proc_elf_offset(void* base, prs_intptr_t offset)
{
    return (void*)((prs_intptr_t)base + offset);
}

struct prs_pal_proc* prs_pal_proc_load(struct prs_pal_proc_load_params* params)
{
    int error;
    int fd = -1;
    void* base = 0;

    struct prs_pal_proc* proc = prs_pal_malloc_zero(sizeof(*proc));
    if (!proc) {
        goto cleanup;
    }

    fd = open(params->filename, O_RDONLY);
    if (fd < 0) {
        goto cleanup;
    }

    struct stat sb;
    error = fstat(fd, &sb);
    if (error) {
        goto cleanup;
    }

    base = mmap(0, sb.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (!base) {
        goto cleanup;
    }

    ElfW(Ehdr)* ehdr = base;
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3) {
        PRS_FTRACE("Invalid ELF64 magic in '%s'", params->filename);
        goto cleanup;
    }
    if (ehdr->e_ident[EI_CLASS] != ELFCLASS64) {
        PRS_FTRACE("Invalid ELF64 class in '%s'", params->filename);
        goto cleanup;
    }
    if (ehdr->e_ident[EI_DATA] != ELFDATA2LSB) {
        PRS_FTRACE("Invalid ELF64 endianness in '%s'", params->filename);
        goto cleanup;
    }
    if (ehdr->e_ident[EI_VERSION] != EV_CURRENT) {
        PRS_FTRACE("Invalid ELF64 version in '%s'", params->filename);
        goto cleanup;
    }
    if (ehdr->e_ident[EI_OSABI] != ELFOSABI_NONE && ehdr->e_ident[EI_OSABI] != ELFOSABI_LINUX) {
        PRS_FTRACE("Invalid ELF64 ABI in '%s'", params->filename);
        goto cleanup;
    }

    ElfW(Phdr)* phdr_base = prs_pal_proc_elf_offset(base, ehdr->e_phoff);

    /*
     * Find the size of the memory area to reserve. We have to do this now in order to ensure that the mmap()'ed
     * regions will be guaranteed to be located at their respective relative expected addresses.
     */
    prs_bool_t vstart_set = PRS_FALSE;
    prs_uintptr_t vstart = 0;
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        ElfW(Phdr)* phdr = &phdr_base[i];
        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        if (!vstart_set) {
            vstart = phdr->p_vaddr;
            vstart_set = PRS_TRUE;
        }
        proc->size = phdr->p_vaddr - vstart + phdr->p_memsz;
    }
    if (!vstart_set) {
        PRS_FTRACE("Couldn't find PT_LOAD section");
        goto cleanup;
    }

    /* Align the size to the page size (not required, but cleaner) */
    const prs_size_t page_size = prs_pal_os_get_page_size();
    proc->size = prs_bitops_align_size(proc->size, page_size);

    /* Reserve the whole memory region */
    proc->data = prs_pal_mem_map(proc->size, PRS_PAL_MEM_FLAG_COMMIT);
    if (!proc->data) {
        PRS_FTRACE("prs_pal_mem_map() failed");
        goto cleanup;
    }
    
    /* Compute the base address (0) of the mmap()'ed virtual memory */
    void* vbase = prs_pal_proc_elf_offset(proc->data, -(prs_intptr_t)vstart);
    prs_log_print("%s: allocated %p (%lu bytes, virtbase %p)", params->filename, proc->data, proc->size, vbase);

    /* Copy data and set permissions on the sections within the region */
    for (int i = 0; i < ehdr->e_phnum; ++i) {
        ElfW(Phdr)* phdr = &phdr_base[i];
        if (phdr->p_type != PT_LOAD) {
            continue;
        }

        void* pbase = prs_pal_proc_elf_offset(vbase, phdr->p_vaddr);
        prs_size_t psize = phdr->p_memsz;
        void* aligned_pbase = pbase;
        prs_uintptr_t uint_pbase = (prs_uintptr_t)pbase;
        const prs_size_t uint_pbase_unaligned_bytes = uint_pbase % page_size;
        if (uint_pbase_unaligned_bytes) {
            psize += uint_pbase_unaligned_bytes;
            uint_pbase -= uint_pbase_unaligned_bytes;
            aligned_pbase = (void*)uint_pbase;
        }

        /* Allow writing so we can copy the data to this section */
        prs_pal_mem_protect(aligned_pbase, psize, PRS_PAL_MEM_FLAG_WRITE);

        /* Copy and zero the section */
        PRS_PAL_PROC_DEBUG_LOG("Writing %u bytes at %p (virt %p)", phdr->p_memsz, pbase, (void*)phdr->p_vaddr);
        PRS_FATAL_WHEN(phdr->p_filesz > phdr->p_memsz);
        void* psrc = prs_pal_proc_elf_offset(base, phdr->p_offset);
        memcpy(pbase, psrc, phdr->p_filesz);
        memset(prs_pal_proc_elf_offset(pbase, phdr->p_filesz), 0, phdr->p_memsz - phdr->p_filesz);

        /* Set the requested permissions */
        prs_pal_mem_flags_t flags = PRS_PAL_MEM_FLAG_NONE;
        if (phdr->p_flags & PF_R) {
            flags |= PRS_PAL_MEM_FLAG_READ;
        }
        if (phdr->p_flags & PF_W) {
            flags |= PRS_PAL_MEM_FLAG_WRITE;
        }
        if (phdr->p_flags & PF_X) {
            flags |= PRS_PAL_MEM_FLAG_EXECUTE;
        }
        prs_pal_mem_protect(aligned_pbase, psize, flags);
    }

    ElfW(Shdr)* shdr_base = prs_pal_proc_elf_offset(base, ehdr->e_shoff);

    /* Find symbol and dynamic section */
    ElfW(Sym)* sym_base = 0;
    const char* sym_string_base = 0;
    for (int i = 0; i < ehdr->e_shnum; ++i) {
        ElfW(Shdr)* shdr = &shdr_base[i];
        if (shdr->sh_type == SHT_DYNSYM) {
            sym_base = prs_pal_proc_elf_offset(base, shdr->sh_offset);
            sym_string_base = prs_pal_proc_elf_offset(base, shdr_base[shdr->sh_link].sh_offset);
            break;
        }
    }

    if (!sym_base || !sym_string_base) {
        PRS_FTRACE("SHT_DYNSYM not found");
        goto cleanup;
    }

    /* Load shared libraries */
    for (int i = 0; i < ehdr->e_shnum; ++i) {
        ElfW(Shdr)* shdr = &shdr_base[i];
        if (shdr->sh_type == SHT_DYNAMIC) {
            ElfW(Dyn)* dyn_base = prs_pal_proc_elf_offset(base, shdr->sh_offset);
            const int dyn_count = shdr->sh_size / sizeof(ElfW(Dyn));
            for (int j = 0; j < dyn_count; ++j) {
                ElfW(Dyn)* dyn = &dyn_base[j];
                if (dyn->d_tag == DT_NEEDED) {
                    const char* shlib_name = sym_string_base + dyn->d_un.d_val;
                    if (!strcmp(shlib_name, "libprs.so")) {
                        continue;
                    }
                    void* handle = dlopen(shlib_name, RTLD_LAZY);
                    if (handle) {
                        PRS_PAL_PROC_DEBUG_LOG("Successfully loaded shared library '%s'", shlib_name);
                    } else {
                        PRS_FTRACE("Failed to load shared library '%s'", shlib_name);
                    }
                }
            }
        }
    }

    /* Find relocation section and apply relocations */
    for (int i = 0; i < ehdr->e_shnum; ++i) {
        ElfW(Shdr)* shdr = &shdr_base[i];
        if (shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA) {
            const prs_bool_t is_rel = PRS_BOOL(shdr->sh_type == SHT_REL);
            ElfW(Rel)* rel_base = prs_pal_proc_elf_offset(base, shdr->sh_offset);
            ElfW(Rela)* rela_base = (void*)rel_base;
            const int rel_count = shdr->sh_size / (is_rel ? sizeof(ElfW(Rel)) : sizeof(ElfW(Rela)));
            for (int j = 0; j < rel_count; ++j) {
                ElfW(Rela)* rela = (is_rel ? (void*)&rel_base[j] : &rela_base[j]);
                const ElfW(Xword) sym_table_index = ELF64_R_SYM(rela->r_info);
                const ElfW(Xword) rel_type = ELF64_R_TYPE(rela->r_info);
                ElfW(Sym)* sym = &sym_base[sym_table_index];
                const char* symbol_name = sym_string_base + sym->st_name;
                switch (rel_type) {
                    case 6: /* R_386_GLOB_DAT */
                    case 7: /* R_386_JMP_SLOT */ {
                        void* symbol_addr = dlsym(RTLD_DEFAULT, symbol_name);
                        if (symbol_addr) {
                            ElfW(Addr)* entry = prs_pal_proc_elf_offset(vbase, rela->r_offset);
                            *entry = (ElfW(Addr))symbol_addr;
                            PRS_PAL_PROC_DEBUG_LOG("Resolved symbol '%s' (%p), entry %p virt %p", symbol_name, symbol_addr, entry, rela->r_offset);
                        } else {
                            PRS_FTRACE("Could not resolve symbol '%s'", symbol_name);
                        }
                        break;
                    }
                }
            }
        }
    }

#if defined(DEBUG)
    /* Find the .text section base address */
    ElfW(Shdr)* sh_strtab = &shdr_base[ehdr->e_shstrndx];
    const char* sh_string_base =  prs_pal_proc_elf_offset(base, sh_strtab->sh_offset);
    for (int i = 0; i < ehdr->e_shnum; ++i) {
        ElfW(Shdr)* shdr = &shdr_base[i];
        if (shdr->sh_type == SHT_PROGBITS) {
            const char* name = sh_string_base + shdr->sh_name;
            if (!strcmp(name, ".text")) {
                printf("### add-symbol-file \"%s\" %p\n", params->filename, prs_pal_proc_elf_offset(vbase, shdr->sh_addr));
                break;
            }
        }
    }
#endif

    proc->entry = prs_pal_proc_elf_offset(vbase, ehdr->e_entry);

    return proc;

    cleanup:

    if (proc) {
        if (proc->data) {
            munmap(proc->data, proc->size);
        }
        prs_pal_free(proc);
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

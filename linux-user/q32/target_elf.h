/* SPDX-License-Identifier: GPL-2.0-or-later */

#ifndef Q32_TARGET_ELF_H
#define Q32_TARGET_ELF_H
#define ELF_CLASS ELFCLASS32
#define ELF_MACHINE EM_Q32
#define elf_check_abi(flags) ((flags) == 0)
#define elf_check_type(type) ((type) == ET_EXEC)
#define elf_check_entry(entry) (((entry) & 3) == 0)

#define elf_check_load_segment(phdr)                                 \
    ((phdr)->p_type != PT_LOAD ||                                    \
     ((phdr)->p_filesz <= (phdr)->p_memsz &&                         \
      (phdr)->p_vaddr + (phdr)->p_memsz >= (phdr)->p_vaddr &&        \
      (phdr)->p_offset + (phdr)->p_filesz >= (phdr)->p_offset &&     \
      ((phdr)->p_align <= 1 ||                                       \
       (((phdr)->p_align & ((phdr)->p_align - 1)) == 0 &&            \
        (((phdr)->p_vaddr - (phdr)->p_offset) &                      \
         ((phdr)->p_align - 1)) == 0))))
#define ELF_REJECT_OVERLAPPING_LOAD_SEGMENTS

#define elf_check_entry_segment(entry, phdr)                         \
    ((phdr)->p_type == PT_LOAD && ((phdr)->p_flags & PF_X) &&        \
     (phdr)->p_vaddr + (phdr)->p_memsz >= (phdr)->p_vaddr &&        \
     (entry) >= (phdr)->p_vaddr &&                                  \
     (entry) < (phdr)->p_vaddr + (phdr)->p_memsz)

#define HAVE_ELF_CORE_DUMP 1
typedef struct target_elf_gregset_t {
    abi_uint pc;
    abi_uint regs[31];
} target_elf_gregset_t;
#endif

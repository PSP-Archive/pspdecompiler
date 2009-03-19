#ifndef __PRX_H
#define __PRX_H

#include "types.h"
#include "hash.h"

#define ELF_HEADER_IDENT        16
#define ELF_PRX_TYPE            0xFFA0
#define ELF_MACHINE_MIPS        8
#define ELF_VERSION_CURRENT     1
#define ELF_FLAGS_MACH_ALLEGREX 0x00A20000
#define ELF_FLAGS_ABI_EABI32    0x00003000
#define ELF_FLAGS_MIPS_ARCH2    0x10000000


/* Structure to hold prx header data */
struct prx
{
  uint8  ident[ELF_HEADER_IDENT];
  uint16 type;
  uint16 machine;
  uint32 version;
  uint32 entry;
  uint32 phoff;
  uint32 shoff;
  uint32 flags;
  uint16 ehsize;
  uint16 phentsize;
  uint16 phnum;
  uint16 shentsize;
  uint16 shnum;
  uint16 shstrndx;

  uint32 size;
  const uint8 *data;

  struct elf_section *sections;
  struct hashtable *secbyname;

  struct elf_program *programs;

  struct prx_modinfo *modinfo;


};

#define SHT_NULL            0
#define SHT_PROGBITS        1
#define SHT_SYMTAB          2
#define SHT_STRTAB          3
#define SHT_RELA            4
#define SHT_HASH            5
#define SHT_DYNAMIC         6
#define SHT_NOTE            7
#define SHT_NOBITS          8
#define SHT_REL             9
#define SHT_SHLIB          10
#define SHT_DYNSYM         11
#define SHT_LOPROC 0x70000000
#define SHT_HIPROC 0x7fffffff
#define SHT_LOUSER 0x80000000
#define SHT_HIUSER 0xffffffff

#define SHT_PRXRELOC (SHT_LOPROC | 0xA0)

#define SHF_WRITE               1
#define SHF_ALLOC               2
#define SHF_EXECINSTR           4

/* Structure defining a single elf section */
struct elf_section
{
  uint32 idxname;
  uint32 type;
  uint32 flags;
  uint32 addr;
  uint32 offset;
  uint32 size;
  uint32 link;
  uint32 info;
  uint32 addralign;
  uint32 entsize;

  const uint8 *data;
  const char *name;

};

#define PT_NULL                 0
#define PT_LOAD                 1
#define PT_DYNAMIC              2
#define PT_INTERP               3
#define PT_NOTE                 4
#define PT_SHLIB                5
#define PT_PHDR                 6
#define PT_LOPROC               0x70000000
#define PT_HIPROC               0x7fffffff
#define PT_PRX                  (PT_LOPROC | 0xA1)

#define PF_X                    1
#define PF_W                    2
#define PF_R                    4

struct elf_program
{
  uint32 type;
  uint32 offset;
  uint32 vaddr;
  uint32 paddr;
  uint32 filesz;
  uint32 memsz;
  uint32 flags;
  uint32 align;

  const uint8 *data;
};

#define PRX_MODULE_INFO       ".rodata.sceModuleInfo"

struct prx_modinfo {

  uint16 attributes;
  uint16 version;
  uint32 gp;
  uint32 expvaddr;
  uint32 expvaddrbtm;
  uint32 impvaddr;
  uint32 impvaddrbtm;

  uint32 numimports;
  uint32 numexports;

  struct prx_import *imports;
  struct prx_export *exports;

  const char *name;
};

struct prx_import {

  uint32 nameaddr;
  uint16 flags;
  uint16 version;
  uint16 numstubs;
  uint16 stubsize;
  uint32 nids;
  uint32 funcs;
  uint32 vars;

  const char *name;

};

struct prx_export {
  uint32 nameaddr;
  uint16 version;
  uint16 attributes;
  uint8 ndwords;
  uint8 nvars;
  uint16 nfuncs;
  uint32 funcs;
  const char *name;

};


/* MIPS Reloc Entry Types */
#define R_MIPS_NONE     0
#define R_MIPS_16       1
#define R_MIPS_32       2
#define R_MIPS_REL32    3
#define R_MIPS_26       4
#define R_MIPS_HI16     5
#define R_MIPS_LO16     6
#define R_MIPS_GPREL16  7
#define R_MIPS_LITERAL  8
#define R_MIPS_GOT16    9
#define R_MIPS_PC16     10
#define R_MIPS_CALL16   11
#define R_MIPS_GPREL32  12



#define ELF32_R_SYM(i) ((i)>>8)
#define ELF32_R_TYPE(i) ((uint8)(i&0xFF))

#define ELF32_ST_BIND(i) ((i)>>4)
#define ELF32_ST_TYPE(i) ((i)&0xf)
#define ELF32_ST_INFO(b,t) (((b)<<4)+((t)&0xf))

struct prx *prx_load (const char *path);
void prx_free (struct prx *p);
void prx_print (struct prx *p);


#endif /* __PRX_H */

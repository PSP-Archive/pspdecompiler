#ifndef __PRX_H
#define __PRX_H

#include "types.h"

#define ELF_HEADER_IDENT      16

#define ELF_PRX_TYPE      0xFFA0

/* Define ELF types */
typedef uint32 Elf32_Addr;
typedef uint16 Elf32_Half;
typedef uint32 Elf32_Off;
typedef uint32 Elf32_Word;


/* Structure to hold elf header data */
struct ElfHeader
{
  unsigned char e_ident[ELF_HEADER_IDENT];

  Elf32_Half e_type;
  Elf32_Half e_machine;
  Elf32_Word e_version;
  Elf32_Addr e_entry;
  Elf32_Off  e_phoff;
  Elf32_Off  e_shoff;
  Elf32_Word e_flags;
  Elf32_Half e_ehsize;
  Elf32_Half e_phentsize;
  Elf32_Half e_phnum;
  Elf32_Half e_shentsize;
  Elf32_Half e_shnum;
  Elf32_Half e_shstrndx;

  size_t size;
  const uint8 *raw_data;

  struct ElfSection *sections;
  struct ElfProgram *programs;
};

/* Structure defining a single elf section */
struct ElfSection
{
  Elf32_Word sh_name;
  Elf32_Word sh_type;
  Elf32_Word sh_flags;
  Elf32_Addr sh_addr;
  Elf32_Off sh_offset;
  Elf32_Word sh_size;
  Elf32_Word sh_link;
  Elf32_Word sh_info;
  Elf32_Word sh_addralign;
  Elf32_Word sh_entsize;

  const uint8 *raw_data;
  const char *name;

  /* Pointer to the head of the relocs (if any) */
  struct ElfReloc *pRelocs;
  /* Number of relocs for this section */
  uint32 iRelocCount;
};

struct ElfProgram
{
  Elf32_Word p_type;
  Elf32_Off p_offset;
  Elf32_Addr p_vaddr;
  Elf32_Addr p_paddr;
  Elf32_Word p_filesz;
  Elf32_Word p_memsz;
  Elf32_Word p_flags;
  Elf32_Word p_align;

  const uint8  *raw_data;
};

struct ElfReloc
{
  /* Pointer to the section name */
  const char* secname;
  /* Base address */
  uint32 base;
  /* Type */
  uint32 type;
  /* Symbol (if known) */
  uint32 symbol;
  /* Offset into the file */
  uint32 offset;
  /* New Address for the relocation (to do with what you will) */
  uint32 info;
  uint32 addr;
};

struct ElfSymbol
{
  const char *symname;
  uint32 name;
  uint32 value;
  uint32 size;
  uint32 info;
  uint32 other;
  uint32 shndx;
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

#define SHF_WRITE               1
#define SHF_ALLOC               2
#define SHF_EXECINSTR           4

#define PT_NULL                 0
#define PT_LOAD                 1
#define PT_DYNAMIC              2
#define PT_INTERP               3
#define PT_NOTE                 4
#define PT_SHLIB                5
#define PT_PHDR                 6
#define PT_LOPROC               0x70000000
#define PT_HIPROC               0x7fffffff

#define ELF32_R_SYM(i) ((i)>>8)
#define ELF32_R_TYPE(i) ((uint8)(i&0xFF))

typedef struct {
  Elf32_Addr r_offset;
  Elf32_Word r_info;
} Elf32_Rel;

#define ELF32_ST_BIND(i) ((i)>>4)
#define ELF32_ST_TYPE(i) ((i)&0xf)
#define ELF32_ST_INFO(b,t) (((b)<<4)+((t)&0xf))

#define STT_NOTYPE   0
#define STT_OBJECT   1
#define STT_FUNC     2
#define STT_SECTION  3
#define STT_FILE     4
#define STT_LOPROC  13
#define STT_HIPROC  15

typedef struct {
  Elf32_Word st_name;
  Elf32_Addr st_value;
  Elf32_Word st_size;
  unsigned char st_info;
  unsigned char st_other;
  Elf32_Half st_shndx;
} __attribute__((packed)) Elf32_Sym;

struct ElfHeader *load_prx (const char *path);

#endif /* __PRX_H */


#include <stdlib.h>
#include <string.h>

#include "prx.h"
#include "utils.h"

#define ELF_HEADER_SIZE       52
#define ELF_MACHINE_MIPS       8
#define ELF_VERSION_CURRENT    1
#define ELF_PRX_FLAGS 0x10A23000


static const uint8 *elf_bytes;
static size_t elf_pos, elf_size;


static const uint8 valid_ident[] = {
  0x7F, 'E', 'L', 'F',
  0x01, /* Elf class = ELFCLASS32 */
  0x01, /* Elf data = ELFDATA2LSB */
  0x01, /* Version = EV_CURRENT */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 /* Padding */
};

static
uint32 read_uint32_le (void)
{
  uint32 r;
  r  = elf_bytes[elf_pos++];
  r |= elf_bytes[elf_pos++] << 8;
  r |= elf_bytes[elf_pos++] << 16;
  r |= elf_bytes[elf_pos++] << 24;
  return r;
}

static
uint16 read_uint16_le (void)
{
  uint16 r;
  r  = elf_bytes[elf_pos++];
  r |= elf_bytes[elf_pos++] << 8;
  return r;
}

static
void read_bytes (uint8 *out, size_t size)
{
  memcpy (out, &elf_bytes[elf_pos], size);
  elf_pos += size;
}

static
void set_position (size_t pos)
{
  elf_pos = pos;
}

static
const uint8 *get_pointer_offset (size_t offset)
{
  return &elf_bytes[offset];
}


static
int check_elf_header (struct ElfHeader *elf_header)
{
  Elf32_Word table_size;

  if (memcmp (elf_header->e_ident, valid_ident, sizeof (valid_ident))) {
    error (__FILE__ ": invalid identification for ELF/PRX");
    return 0;
  }

  if (elf_header->e_type != ELF_PRX_TYPE) {
    error (__FILE__ ": not a PRX file");
    return 0;
  }

  if (elf_header->e_machine != ELF_MACHINE_MIPS) {
    error (__FILE__ ": not a valid PRX file (machine is not MIPS)");
    return 0;
  }

  if (elf_header->e_version != ELF_VERSION_CURRENT) {
    error (__FILE__ ": not a valid PRX file (version is not EV_CURRENT)");
    return 0;
  }

  if (elf_header->e_ehsize != ELF_HEADER_SIZE) {
    error (__FILE__ ": wrong ELF header size");
    return 0;
  }

  if ((elf_header->e_flags & ELF_PRX_FLAGS) != ELF_PRX_FLAGS) {
    error (__FILE__ ": wrong ELF flags");
    return 0;
  }

  table_size = elf_header->e_phentsize;
  table_size *= (Elf32_Word) elf_header->e_phnum;
  if (elf_header->e_phoff >= elf_header->size ||
      table_size > elf_header->size ||
      (elf_header->e_phoff + table_size) > elf_header->size) {
    error (__FILE__ ": wrong ELF program header table offset/size");
    return 0;
  }

  table_size = elf_header->e_shentsize;
  table_size *= (Elf32_Word) elf_header->e_shnum;
  if (elf_header->e_shoff >= elf_header->size ||
      table_size > elf_header->size ||
      (elf_header->e_shoff + table_size) > elf_header->size) {
    error (__FILE__ ": wrong ELF section header table offset/size");
    return 0;
  }

  return 1;

}

static
int check_section_header (struct ElfSection *section, Elf32_Word index)
{
  if (section->sh_offset >= elf_size ||
      (section->sh_type != SHT_NOBITS && (section->sh_size > elf_size ||
      (section->sh_offset + section->sh_size) > elf_size))) {
    error (__FILE__ ": wrong section offset/size (section %d)", index);
    return 0;
  }

  return 1;
}

static
int check_program_header (struct ElfProgram *program, Elf32_Word index)
{
  if (program->p_offset >= elf_size ||
      program->p_filesz > elf_size ||
      (program->p_offset + program->p_filesz) > elf_size) {
    error (__FILE__ ": wrong program offset/size (program %d)", index);
    return 0;
  }

  return 1;
}

static
int load_sections (struct ElfHeader *elf_header)
{
  struct ElfSection *sections;
  Elf32_Word idx;

  sections = xmalloc (elf_header->e_shnum * sizeof (struct ElfSection));
  elf_header->sections = sections;

  set_position (elf_header->e_shoff);
  for (idx = 0; idx < elf_header->e_shnum; idx++) {

    sections[idx].sh_name = read_uint32_le ();
    sections[idx].sh_type = read_uint32_le ();
    sections[idx].sh_flags = read_uint32_le ();
    sections[idx].sh_addr = read_uint32_le ();
    sections[idx].sh_offset = read_uint32_le ();
    sections[idx].sh_size = read_uint32_le ();
    sections[idx].sh_link = read_uint32_le ();
    sections[idx].sh_info = read_uint32_le ();
    sections[idx].sh_addralign = read_uint32_le ();
    sections[idx].sh_entsize = read_uint32_le ();

    if (!check_section_header (&sections[idx], idx)) {
      free (sections);
      elf_header->sections = NULL;
      return 0;
    }

    sections[idx].raw_data = get_pointer_offset (sections[idx].sh_offset);
  }

  if (elf_header->e_shstrndx > 0) {
    if (sections[elf_header->e_shstrndx].sh_type == SHT_STRTAB) {
      char *strings = (char *) sections[elf_header->e_shstrndx].raw_data;
      Elf32_Word max_index = sections[elf_header->e_shstrndx].sh_size;
      if (max_index > 0) {

        if (strings[max_index - 1] != '\0') {
          error (__FILE__ ": string table section not terminated with null byte");
          free (sections);
          elf_header->sections = NULL;
          return 0;
        }

        for (idx = 0; idx < elf_header->e_shnum; idx++) {
          if (sections[idx].sh_name < max_index) {
            sections[idx].name = &strings[sections[idx].sh_name];
          } else {
            error (__FILE__ ": invalid section name");
            free (sections);
            elf_header->sections = NULL;
            return 0;
          }
        }
      }
    }
  }
  return 1;
}

static
int load_programs (struct ElfHeader *elf_header)
{
  struct ElfProgram *programs;
  Elf32_Word idx;

  programs = xmalloc (elf_header->e_phnum * sizeof (struct ElfProgram));
  elf_header->programs = programs;

  set_position (elf_header->e_phoff);
  for (idx = 0; idx < elf_header->e_phnum; idx++) {
    programs[idx].p_type = read_uint32_le ();
    programs[idx].p_offset = read_uint32_le ();
    programs[idx].p_vaddr = read_uint32_le ();
    programs[idx].p_paddr = read_uint32_le ();
    programs[idx].p_filesz = read_uint32_le ();
    programs[idx].p_memsz = read_uint32_le ();
    programs[idx].p_flags = read_uint32_le ();
    programs[idx].p_align = read_uint32_le ();

    if (!check_program_header (&programs[idx], idx)) {
      free (programs);
      elf_header->programs = NULL;
      return 0;
    }

    programs[idx].raw_data = get_pointer_offset (programs[idx].p_offset);
  }

  return 1;
}

struct ElfHeader *load_prx (const char *path)
{
  struct ElfHeader *elf_header;
  elf_bytes = read_file (path, &elf_size);
  if (!elf_bytes) return NULL;

  if (elf_size < ELF_HEADER_SIZE) {
    error (__FILE__ ": elf size too short");
    free ((void *) elf_bytes);
    elf_bytes = NULL;
    return NULL;
  }

  elf_header = xmalloc (sizeof (struct ElfHeader));
  elf_header->size = elf_size;
  elf_header->raw_data = elf_bytes;

  read_bytes (elf_header->e_ident, ELF_HEADER_IDENT);
  elf_header->e_type = read_uint16_le ();
  elf_header->e_machine = read_uint16_le ();

  elf_header->e_version = read_uint32_le ();
  elf_header->e_entry = read_uint32_le ();
  elf_header->e_phoff = read_uint32_le ();
  elf_header->e_shoff = read_uint32_le ();
  elf_header->e_flags = read_uint32_le ();
  elf_header->e_ehsize = read_uint16_le ();
  elf_header->e_phentsize = read_uint16_le ();
  elf_header->e_phnum = read_uint16_le ();
  elf_header->e_shentsize = read_uint16_le ();
  elf_header->e_shnum = read_uint16_le ();
  elf_header->e_shstrndx = read_uint16_le ();

  if (!check_elf_header (elf_header)) {
    free ((void *) elf_bytes);
    free (elf_header);
    elf_bytes = NULL;
    return NULL;
  }

  if (!load_sections (elf_header)) {
    free ((void *) elf_bytes);
    free (elf_header);
    elf_bytes = NULL;
    return NULL;
  }

  if (!load_programs (elf_header)) {
    free ((void *) elf_bytes);
    free (elf_header);
    elf_bytes = NULL;
    return NULL;
  }

  return elf_header;
}

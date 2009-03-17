
#include <stdlib.h>
#include <string.h>

#include "prx.h"
#include "hash.h"
#include "utils.h"

#define ELF_HEADER_SIZE              52
#define ELF_SECTION_HEADER_ENT_SIZE  40
#define ELF_PROGRAM_HEADER_ENT_SIZE  32
#define ELF_PRX_FLAGS                (ELF_FLAGS_MIPS_ARCH2 | ELF_FLAGS_MACH_ALLEGREX | ELF_FLAGS_MACH_ALLEGREX)
#define PRX_MODULE_INFO_SIZE


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
const uint8 *get_pointer (size_t offset)
{
  return &elf_bytes[offset];
}


static
int check_section_header (struct elf_section *section, uint32 index)
{
  if (section->offset >= elf_size ||
      (section->type != SHT_NOBITS && (section->size > elf_size ||
      (section->offset + section->size) > elf_size))) {
    error (__FILE__ ": wrong section offset/size (section %d)", index);
    return 0;
  }

  return 1;
}

static
int check_program_header (struct elf_program *program, uint32 index)
{
  if (program->offset >= elf_size ||
      program->filesz > elf_size ||
      (program->offset + program->filesz) > elf_size) {
    error (__FILE__ ": wrong program offset/size (program %d)", index);
    return 0;
  }
  return 1;
}

static
int check_elf_header (struct prx *p)
{
  uint32 table_size;

  if (memcmp (p->ident, valid_ident, sizeof (valid_ident))) {
    error (__FILE__ ": invalid identification for ELF/PRX");
    return 0;
  }

  if (p->type != ELF_PRX_TYPE) {
    error (__FILE__ ": not a PRX file (0x%04X)", p->type);
    return 0;
  }

  if (p->machine != ELF_MACHINE_MIPS) {
    error (__FILE__ ": machine is not MIPS (0x%04X)", p->machine);
    return 0;
  }

  if (p->version != ELF_VERSION_CURRENT) {
    error (__FILE__ ": version is not EV_CURRENT (0x%08X)", p->version);
    return 0;
  }

  if (p->ehsize != ELF_HEADER_SIZE) {
    error (__FILE__ ": wrong ELF header size (%u)", p->ehsize);
    return 0;
  }

  if ((p->flags & ELF_PRX_FLAGS) != ELF_PRX_FLAGS) {
    error (__FILE__ ": wrong ELF flags (0x%08X)", p->flags);
    return 0;
  }

  if (p->phnum && p->phentsize != ELF_PROGRAM_HEADER_ENT_SIZE) {
    error (__FILE__ ": wrong ELF program header entity size (%u)", p->phentsize);
    return 0;
  }

  if (!p->phnum) {
    error (__FILE__ ": PRX has no programs");
    return 0;
  }

  table_size = p->phentsize;
  table_size *= (uint32) p->phnum;
  if (p->phoff >= p->size ||
      table_size > p->size ||
      (p->phoff + table_size) > p->size) {
    error (__FILE__ ": wrong ELF program header table offset/size");
    return 0;
  }

  if (p->shnum && p->shentsize != ELF_SECTION_HEADER_ENT_SIZE) {
    error (__FILE__ ": wrong ELF section header entity size (%u)", p->shentsize);
    return 0;
  }

  table_size = p->shentsize;
  table_size *= (uint32) p->shnum;
  if (p->shoff >= p->size ||
      table_size > p->size ||
      (p->shoff + table_size) > p->size) {
    error (__FILE__ ": wrong ELF section header table offset/size");
    return 0;
  }

  return 1;

}

static
void free_sections (struct prx *p)
{
  if (p->sections)
    free (p->sections);
  p->sections = NULL;
  if (p->secbyname)
    hashtable_destroy (p->secbyname, NULL);
  p->secbyname = NULL;
}

static
void free_programs (struct prx *p)
{
  if (p->programs)
    free (p->programs);
  p->programs = NULL;
}

static
void free_prx (struct prx *p)
{
  free_sections (p);
  free_programs (p);
  if (p->data)
    free ((void *) p->data);
  p->data = NULL;
  free (p);
}


static
int load_sections (struct prx *p)
{
  struct elf_section *sections;
  uint32 idx;

  p->sections = NULL;
  p->secbyname = hashtable_create (64, &hash_string, &hashtable_string_compare);
  if (p->shnum == 0) return 1;

  sections = xmalloc (p->shnum * sizeof (struct elf_section));
  p->sections = sections;

  set_position (p->shoff);
  for (idx = 0; idx < p->shnum; idx++) {

    sections[idx].idxname = read_uint32_le ();
    sections[idx].type = read_uint32_le ();
    sections[idx].flags = read_uint32_le ();
    sections[idx].addr = read_uint32_le ();
    sections[idx].offset = read_uint32_le ();
    sections[idx].size = read_uint32_le ();
    sections[idx].link = read_uint32_le ();
    sections[idx].info = read_uint32_le ();
    sections[idx].addralign = read_uint32_le ();
    sections[idx].entsize = read_uint32_le ();

    if (!check_section_header (&sections[idx], idx))
      return 0;

    sections[idx].data = get_pointer (sections[idx].offset);
  }

  if (p->shstrndx > 0) {
    if (sections[p->shstrndx].type == SHT_STRTAB) {
      char *strings = (char *) sections[p->shstrndx].data;
      uint32 max_index = sections[p->shstrndx].size;
      if (max_index > 0) {

        if (strings[max_index - 1] != '\0') {
          error (__FILE__ ": string table section not terminated with null byte");
          return 0;
        }

        for (idx = 0; idx < p->shnum; idx++) {
          if (sections[idx].idxname < max_index) {
            sections[idx].name = &strings[sections[idx].idxname];
          } else {
            error (__FILE__ ": invalid section name");
            return 0;
          }
        }
        for (idx = 0; idx < p->shnum; idx++) {
          hashtable_insert (p->secbyname, (void *) sections[idx].name, &sections[idx]);
        }
      }
    }
  }

  return 1;
}

static
int load_programs (struct prx *p)
{
  struct elf_program *programs;
  uint32 idx;

  programs = xmalloc (p->phnum * sizeof (struct elf_program));
  p->programs = programs;

  set_position (p->phoff);
  for (idx = 0; idx < p->phnum; idx++) {
    programs[idx].type = read_uint32_le ();
    programs[idx].offset = read_uint32_le ();
    programs[idx].vaddr = read_uint32_le ();
    programs[idx].paddr = read_uint32_le ();
    programs[idx].filesz = read_uint32_le ();
    programs[idx].memsz = read_uint32_le ();
    programs[idx].flags = read_uint32_le ();
    programs[idx].align = read_uint32_le ();

    if (!check_program_header (&programs[idx], idx))
      return 0;

    programs[idx].data = get_pointer (programs[idx].offset);
  }

  return 1;
}

static
int load_module_info (struct prx *p)
{
  struct elf_section *s;
  s = hashtable_search (p->secbyname, PRX_MODULE_INFO, NULL);
  p->modinfo = NULL;
  if (s) {
    p->modinfo = (struct prx_modinfo *) xmalloc (sizeof (struct prx_modinfo));
    return 1;
  }
  return 0;
}


struct prx *load_prx (const char *path)
{
  struct prx *p;
  elf_bytes = read_file (path, &elf_size);
  if (!elf_bytes) return NULL;

  if (elf_size < ELF_HEADER_SIZE) {
    error (__FILE__ ": elf size too short");
    free ((void *) elf_bytes);
    elf_bytes = NULL;
    return NULL;
  }

  p = xmalloc (sizeof (struct prx));
  p->size = elf_size;
  p->data = elf_bytes;

  read_bytes (p->ident, ELF_HEADER_IDENT);
  p->type = read_uint16_le ();
  p->machine = read_uint16_le ();

  p->version = read_uint32_le ();
  p->entry = read_uint32_le ();
  p->phoff = read_uint32_le ();
  p->shoff = read_uint32_le ();
  p->flags = read_uint32_le ();
  p->ehsize = read_uint16_le ();
  p->phentsize = read_uint16_le ();
  p->phnum = read_uint16_le ();
  p->shentsize = read_uint16_le ();
  p->shnum = read_uint16_le ();
  p->shstrndx = read_uint16_le ();

  if (!check_elf_header (p)) {
    free_prx (p);
    elf_bytes = NULL;
    return NULL;
  }

  if (!load_sections (p)) {
    free_prx (p);
    elf_bytes = NULL;
    return NULL;
  }

  if (!load_programs (p)) {
    free_prx (p);
    elf_bytes = NULL;
    return NULL;
  }

  if (!load_module_info (p)) {
    free_prx (p);
    elf_bytes = NULL;
    return NULL;
  }

  return p;
}

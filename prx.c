
#include <stdlib.h>
#include <string.h>

#include "prx.h"
#include "hash.h"
#include "utils.h"

#define ELF_HEADER_SIZE              52
#define ELF_SECTION_HEADER_ENT_SIZE  40
#define ELF_PROGRAM_HEADER_ENT_SIZE  32
#define ELF_PRX_FLAGS                (ELF_FLAGS_MIPS_ARCH2 | ELF_FLAGS_MACH_ALLEGREX | ELF_FLAGS_MACH_ALLEGREX)
#define PRX_MODULE_INFO_SIZE         52


static const uint8 valid_ident[] = {
  0x7F, 'E', 'L', 'F',
  0x01, /* Elf class = ELFCLASS32 */
  0x01, /* Elf data = ELFDATA2LSB */
  0x01, /* Version = EV_CURRENT */
  0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 /* Padding */
};

static
uint32 read_uint32_le (const uint8 *bytes)
{
  uint32 r;
  r  = *bytes++;
  r |= *bytes++ << 8;
  r |= *bytes++ << 16;
  r |= *bytes++ << 24;
  return r;
}

static
uint16 read_uint16_le (const uint8 *bytes)
{
  uint16 r;
  r  = *bytes++;
  r |= *bytes++ << 8;
  return r;
}

static
int check_inside_prx (struct prx *p, uint32 offset, uint32 size)
{
  if (offset >= p->size || size > p->size ||
      size > (p->size - offset)) return 0;
  return 1;
}

static
int check_inside_program0 (struct prx *p, uint32 vaddr, uint32 size)
{
  struct elf_program *program = p->programs;
  if (vaddr < program->vaddr || size > program->filesz) return 0;

  vaddr -= program->vaddr;
  if (vaddr >= program->filesz || (program->filesz - vaddr) < size) return 0;
  return 1;
}

static
int check_string_program0 (struct prx *p, uint32 vaddr)
{
  struct elf_program *program = p->programs;

  if (vaddr < program->vaddr) return 0;

  vaddr -= program->vaddr;
  if (vaddr >= program->filesz) return 0;

  while (vaddr < program->filesz) {
    if (!p->data[program->offset + vaddr]) return 1;
    vaddr++;
  }

  return 0;
}

static
int check_section_header (struct prx *p, uint32 index)
{
  struct elf_section *section = &p->sections[index];

  switch (section->type) {
  case SHT_NOBITS:
    break;
  case SHT_PRXRELOC:
  case SHT_STRTAB:
  case SHT_PROGBITS:
  case SHT_NULL:
    if (!check_inside_prx (p, section->offset, section->size)) {
      error (__FILE__ ": wrong section offset/size (section %d)", index);
      return 0;
    }
    break;
  default:
    error (__FILE__ ": invalid section type 0x$08X (section %d)", section->type, index);
    return 0;
  }

  return 1;
}

static
int check_program_header (struct prx *p, uint32 index)
{
  struct elf_program *program = &p->programs[index];
  if (!check_inside_prx (p, program->offset, program->filesz)) {
    error (__FILE__ ": wrong program offset/size (program %d)", index);
    return 0;
  }

  if ((index == 0) && program->type != PT_LOAD) {
    error (__FILE__ ": first program is not of the type LOAD");
    return 0;
  }

  switch (program->type) {
  case PT_LOAD:
    if (program->filesz > program->memsz) {
      error (__FILE__ ": program file size grater than than memory size (program %d)", index);
      return 0;
    }
    break;
  case PT_PRX:
    if (program->memsz) {
      error (__FILE__ ": program type must not loaded (program %d)", index);
      return 0;
    }
    break;
  default:
    error (__FILE__ ": invalid program type 0x$08X (program %d)", program->type, index);
    return 0;
  }

  return 1;
}

static
int check_module_info (struct prx *p)
{
  struct prx_modinfo *info = p->modinfo;
  uint32 vaddr, offset;

  if (info->name[27]) {
    error (__FILE__ ": module name is not null terminated\n");
    return 0;
  }

  if (info->expvaddr) {
    if (info->expvaddr > info->expvaddrbtm) {
      error (__FILE__ ": exports bottom is above top (0x%08X - 0x%08X)", info->expvaddr, info->expvaddrbtm);
      return 0;
    }
    if (!check_inside_program0 (p, info->expvaddr, info->expvaddrbtm - info->expvaddr)) {
      error (__FILE__ ": exports not inside the first program (0x%08X - 0x%08X)", info->expvaddr, info->expvaddrbtm);
      return 0;
    }
    info->numexports = 0;
    offset = prx_translate (p, info->expvaddr);
    for (vaddr = info->expvaddr; vaddr < info->expvaddrbtm; info->numexports++) {
      uint32 size;
      size = p->data[offset+8];
      if (size < 4) {
        error (__FILE__ ": export size less than 4 words: %d", size);
        return 0;
      }
      vaddr += size << 2;
      offset += size << 2;
    }
    if (vaddr != info->expvaddrbtm) {
      error (__FILE__ ": invalid exports boundary");
      return 0;
    }
  }

  if (info->impvaddr) {
    if (info->impvaddr > info->impvaddrbtm) {
      error (__FILE__ ": imports bottom is above top (0x%08X - 0x%08X)", info->impvaddr, info->impvaddrbtm);
      return 0;
    }
    if (!check_inside_program0 (p, info->impvaddr, info->impvaddrbtm - info->impvaddr)) {
      error (__FILE__ ": imports not inside the first program (0x%08X - 0x%08X)", info->impvaddr, info->impvaddrbtm);
      return 0;
    }
    info->numimports = 0;
    offset = prx_translate (p, info->impvaddr);
    for (vaddr = info->impvaddr; vaddr < info->impvaddrbtm; info->numimports++) {
      uint32 size;
      uint8 nvars;
      size = p->data[offset+8];
      nvars = p->data[offset+9];
      if (size < 5) {
        error (__FILE__ ": import size less than 5 words: %d", size);
        return 0;
      }
      if (nvars && size < 6) {
        error (__FILE__ ": import size less than 6 words: %d", size);
        return 0;
      }
      vaddr += size << 2;
      offset += size << 2;
    }
    if (vaddr != info->impvaddrbtm) {
      error (__FILE__ ": invalid imports boundary");
      return 0;
    }
  }
  return 1;
}

static
int check_module_import (struct prx *p, uint32 index)
{
  struct prx_import *imp = &p->modinfo->imports[index];

  if (!check_string_program0 (p, imp->namevaddr)) {
    error (__FILE__ ": import name not inside first program");
    return 0;
  }

  if (!imp->nfuncs && !imp->nvars) {
    error (__FILE__ ": no functions or variables imported");
    return 0;
  }

  if (!check_inside_program0 (p, imp->funcsvaddr, 8 * imp->nfuncs)) {
    error (__FILE__ ": functions not inside the first program");
    return 0;
  }

  if (!check_inside_program0 (p, imp->nidsvaddr, 4 * imp->nfuncs)) {
    error (__FILE__ ": nids not inside the first program");
    return 0;
  }

  if (imp->nvars) {
    if (!check_inside_program0 (p, imp->varsvaddr, 8 * imp->nvars)) {
      error (__FILE__ ": variables not inside first program");
      return 0;
    }
  }


  return 1;
}

static
int check_module_export (struct prx *p, uint32 index)
{
  struct prx_export *exp = &p->modinfo->exports[index];

  if (!check_string_program0 (p, exp->namevaddr)) {
    error (__FILE__ ": export name not inside first program");
    return 0;
  }

  if (!exp->nfuncs && !exp->nvars) {
    error (__FILE__ ": no functions or variables exported");
    return 0;
  }

  if (!check_inside_program0 (p, exp->expvaddr, 8 * (exp->nfuncs + exp->nvars))) {
    error (__FILE__ ": functions and variables not inside the first program");
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
  if (!check_inside_prx (p, p->phoff, table_size)) {
    error (__FILE__ ": wrong ELF program header table offset/size");
    return 0;
  }

  if (p->shnum && p->shentsize != ELF_SECTION_HEADER_ENT_SIZE) {
    error (__FILE__ ": wrong ELF section header entity size (%u)", p->shentsize);
    return 0;
  }

  table_size = p->shentsize;
  table_size *= (uint32) p->shnum;
  if (!check_inside_prx (p, p->shoff, table_size)) {
    error (__FILE__ ": wrong ELF section header table offset/size");
    return 0;
  }

  return 1;

}

static
int load_sections (struct prx *p)
{
  struct elf_section *sections;
  uint32 idx;
  uint32 offset;

  p->sections = NULL;
  p->secbyname = hashtable_create (64, &hashtable_hash_string, &hashtable_string_compare);
  if (p->shnum == 0) return 1;

  sections = xmalloc (p->shnum * sizeof (struct elf_section));
  p->sections = sections;

  offset = p->shoff;
  for (idx = 0; idx < p->shnum; idx++) {

    sections[idx].idxname = read_uint32_le (&p->data[offset]);
    sections[idx].type = read_uint32_le (&p->data[offset+4]);
    sections[idx].flags = read_uint32_le (&p->data[offset+8]);
    sections[idx].addr = read_uint32_le (&p->data[offset+12]);
    sections[idx].offset = read_uint32_le (&p->data[offset+16]);
    sections[idx].size = read_uint32_le (&p->data[offset+20]);
    sections[idx].link = read_uint32_le (&p->data[offset+24]);
    sections[idx].info = read_uint32_le (&p->data[offset+28]);
    sections[idx].addralign = read_uint32_le (&p->data[offset+32]);
    sections[idx].entsize = read_uint32_le (&p->data[offset+36]);

    sections[idx].data = &p->data[sections[idx].offset];

    if (!check_section_header (p, idx))
      return 0;

    offset += p->shentsize;
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
  uint32 offset;

  programs = xmalloc (p->phnum * sizeof (struct elf_program));
  p->programs = programs;

  offset = p->phoff;
  for (idx = 0; idx < p->phnum; idx++) {
    programs[idx].type = read_uint32_le (&p->data[offset]);
    programs[idx].offset = read_uint32_le (&p->data[offset+4]);
    programs[idx].vaddr = read_uint32_le (&p->data[offset+8]);
    programs[idx].paddr = read_uint32_le (&p->data[offset+12]);
    programs[idx].filesz = read_uint32_le (&p->data[offset+16]);
    programs[idx].memsz = read_uint32_le (&p->data[offset+20]);
    programs[idx].flags = read_uint32_le (&p->data[offset+24]);
    programs[idx].align = read_uint32_le (&p->data[offset+28]);

    programs[idx].data = &p->data[programs[idx].offset];

    if (!check_program_header (p, idx))
      return 0;

    offset += p->phentsize;
  }

  return 1;
}

static
int load_relocs (struct prx *p)
{
  uint32 i, count = 0;
  for (i = 0; i < p->shnum; i++) {
    struct elf_section *section = &p->sections[i];
    if (section->type == SHT_PRXRELOC) {
      count += section->size >> 3;
    }
  }
  p->relocs = NULL;
  if (!count) return 1;

  p->relocnum = count;
  p->relocs = (struct prx_reloc *) xmalloc (count * sizeof (struct prx_reloc));

  count = 0;
  for (i = 0; i < p->shnum; i++) {
    struct elf_section *section = &p->sections[i];
    if (section->type == SHT_PRXRELOC) {
      uint32 j, secsize;
      uint32 offset;
      offset = section->offset;
      secsize = section->size >> 3;
      for (j = 0; j < secsize; j++) {
        p->relocs[count].offset = read_uint32_le (&p->data[offset]);
        p->relocs[count].info = read_uint32_le (&p->data[offset + 4]);
        count++;
        offset += 8;
      }
    }
  }
  return 1;
}

static
int load_module_import (struct prx *p, struct prx_import *imp)
{
  uint32 i, offset;
  if (imp->nfuncs) {
    imp->funcs = (struct prx_function *) xmalloc (imp->nfuncs * sizeof (struct prx_function));
    offset = prx_translate (p, imp->nidsvaddr);
    for (i = 0; i < imp->nfuncs; i++) {
      struct prx_function *f = &imp->funcs[i];
      f->nid = read_uint32_le (&p->data[offset + 4 * i]);
      f->vaddr = imp->funcsvaddr + 8 * i;
      f->name = NULL;
    }
  }

  if (imp->nvars) {
    imp->vars = (struct prx_variable *) xmalloc (imp->nvars * sizeof (struct prx_variable));
    offset = prx_translate (p, imp->varsvaddr);
    for (i = 0; i < imp->nvars; i++) {
      struct prx_variable *v = &imp->vars[i];
      v->nid = read_uint32_le (&p->data[offset + 8 * i + 4]);
      v->vaddr = read_uint32_le (&p->data[offset +  8 * i]);
      v->name = NULL;
    }
 }
  return 1;
}

static
int load_module_imports (struct prx *p)
{
  uint32 i = 0, offset;
  struct prx_modinfo *info = p->modinfo;
  if (!info->impvaddr) return 1;

  info->imports = (struct prx_import *) xmalloc (info->numimports * sizeof (struct prx_import));
  memset (info->imports, 0, info->numimports * sizeof (struct prx_import));

  offset = prx_translate (p, info->impvaddr);
  for (i = 0; i < info->numimports; i++) {
    struct prx_import *imp = &info->imports[i];
    imp->namevaddr = read_uint32_le (&p->data[offset]);
    imp->flags = read_uint32_le (&p->data[offset+4]);
    imp->size = p->data[offset+8];
    imp->nvars = p->data[offset+9];
    imp->nfuncs = read_uint16_le (&p->data[offset+10]);
    imp->nidsvaddr = read_uint32_le (&p->data[offset+12]);
    imp->funcsvaddr = read_uint32_le (&p->data[offset+16]);
    if (imp->nvars) imp->varsvaddr = read_uint32_le (&p->data[offset+20]);

    if (!check_module_import (p, i)) return 0;

    if (imp->namevaddr)
      imp->name = (const char *) &p->data[prx_translate (p, imp->namevaddr)];
    else
      imp->name = NULL;

    if (!load_module_import (p, imp)) return 0;
    offset += imp->size << 2;
  }
  return 1;
}

static
const char *resolve_syslib_nid (uint32 nid)
{
  switch (nid) {
  case 0xd3744be0: return "module_bootstart";
  case 0x2f064fa6: return "module_reboot_before";
  case 0xadf12745: return "module_reboot_phase";
  case 0xd632acdb: return "module_start";
  case 0xcee8593c: return "module_stop";
  case 0xf01d73a7: return "module_info";
  case 0x0f7c276c: return "module_start_thread_parameter";
  case 0xcf0cc697: return "module_stop_thread_parameter";
  }
  return NULL;
}

static
int load_module_export (struct prx *p, struct prx_export *exp)
{
  uint32 i, offset, disp;
  offset = prx_translate (p, exp->expvaddr);
  disp = 4 * (exp->nfuncs + exp->nvars);
  if (exp->nfuncs) {
    exp->funcs = (struct prx_function *) xmalloc (exp->nfuncs * sizeof (struct prx_function));
    for (i = 0; i < exp->nfuncs; i++) {
      struct prx_function *f = &exp->funcs[i];
      f->vaddr = read_uint32_le (&p->data[offset + disp]);
      f->nid = read_uint32_le (&p->data[offset]);
      f->name = NULL;
      offset += 4;
      if (exp->namevaddr == 0) {
        f->name = resolve_syslib_nid (f->nid);
      }
    }
  }

  if (exp->nvars) {
    exp->vars = (struct prx_variable *) xmalloc (exp->nvars * sizeof (struct prx_variable));
    for (i = 0; i < exp->nvars; i++) {
      struct prx_variable *v = &exp->vars[i];
      v->vaddr = read_uint32_le (&p->data[offset + disp]);
      v->nid = read_uint32_le (&p->data[offset]);
      v->name = NULL;
      offset += 4;
      if (exp->namevaddr == 0) {
        v->name = resolve_syslib_nid (v->nid);
      }
    }
  }
  return 1;
}

static
int load_module_exports (struct prx *p)
{
  uint32 i = 0, offset;
  struct prx_modinfo *info = p->modinfo;
  if (!info->expvaddr) return 1;

  info->exports = (struct prx_export *) xmalloc (info->numexports * sizeof (struct prx_export));
  memset (info->exports, 0, info->numexports * sizeof (struct prx_export));

  offset = prx_translate (p, info->expvaddr);
  for (i = 0; i < info->numexports; i++) {
    struct prx_export *exp = &info->exports[i];
    exp->namevaddr = read_uint32_le (&p->data[offset]);
    exp->flags = read_uint32_le (&p->data[offset+4]);
    exp->size = p->data[offset+8];
    exp->nvars = p->data[offset+9];
    exp->nfuncs = read_uint16_le (&p->data[offset+10]);
    exp->expvaddr = read_uint32_le (&p->data[offset+12]);

    if (!check_module_export (p, i)) return 0;

    if (exp->namevaddr)
      exp->name = (const char *) &p->data[prx_translate (p, exp->namevaddr)];
    else
      exp->name = "syslib";

    if (!load_module_export (p, exp)) return 0;
    offset += exp->size << 2;
  }
  return 1;
}

static
int load_module_info (struct prx *p)
{
  struct prx_modinfo *info;
  uint32 offset;
  p->modinfo = NULL;
  if (p->phnum > 0)
    offset = p->programs[0].paddr & 0x7FFFFFFF;
  else {
    error (__FILE__ ": can't find module info for PRX");
    return 0;
  }

  info = (struct prx_modinfo *) xmalloc (sizeof (struct prx_modinfo));
  p->modinfo = info;

  info->attributes = read_uint16_le (&p->data[offset]);
  info->version = read_uint16_le (&p->data[offset+2]);
  info->name = (const char *) &p->data[offset+4];
  info->gp = read_uint32_le (&p->data[offset+32]);
  info->expvaddr = read_uint32_le (&p->data[offset+36]);
  info->expvaddrbtm = read_uint32_le (&p->data[offset+40]);
  info->impvaddr = read_uint32_le (&p->data[offset+44]);
  info->impvaddrbtm = read_uint32_le (&p->data[offset+48]);

  info->imports = NULL;
  info->exports = NULL;

  if (!check_module_info (p)) return 0;

  if (!load_module_imports (p)) return 0;
  if (!load_module_exports (p)) return 0;

  return 1;
}


struct prx *prx_load (const char *path)
{
  struct prx *p;
  uint8 *elf_bytes;
  size_t elf_size;
  elf_bytes = read_file (path, &elf_size);

  if (!elf_bytes) return NULL;

  if (elf_size < ELF_HEADER_SIZE) {
    error (__FILE__ ": elf size too short");
    free ((void *) elf_bytes);
    return NULL;
  }

  p = xmalloc (sizeof (struct prx));
  p->size = elf_size;
  p->data = elf_bytes;

  memcpy (p->ident, p->data, ELF_HEADER_IDENT);
  p->type = read_uint16_le (&p->data[ELF_HEADER_IDENT]);
  p->machine = read_uint16_le (&p->data[ELF_HEADER_IDENT+2]);

  p->version = read_uint32_le (&p->data[ELF_HEADER_IDENT+4]);
  p->entry = read_uint32_le (&p->data[ELF_HEADER_IDENT+8]);
  p->phoff = read_uint32_le (&p->data[ELF_HEADER_IDENT+12]);
  p->shoff = read_uint32_le (&p->data[ELF_HEADER_IDENT+16]);
  p->flags = read_uint32_le (&p->data[ELF_HEADER_IDENT+20]);
  p->ehsize = read_uint16_le (&p->data[ELF_HEADER_IDENT+24]);
  p->phentsize = read_uint16_le (&p->data[ELF_HEADER_IDENT+26]);
  p->phnum = read_uint16_le (&p->data[ELF_HEADER_IDENT+28]);
  p->shentsize = read_uint16_le (&p->data[ELF_HEADER_IDENT+30]);
  p->shnum = read_uint16_le (&p->data[ELF_HEADER_IDENT+32]);
  p->shstrndx = read_uint16_le (&p->data[ELF_HEADER_IDENT+34]);

  if (!check_elf_header (p)) {
    prx_free (p);
    return NULL;
  }

  if (!load_sections (p)) {
    prx_free (p);
    return NULL;
  }

  if (!load_programs (p)) {
    prx_free (p);
    return NULL;
  }

  if (!load_relocs (p)) {
    prx_free (p);
    return NULL;
  }

  if (!load_module_info (p)) {
    prx_free (p);
    return NULL;
  }

  return p;
}

static
void free_sections (struct prx *p)
{
  if (p->sections)
    free (p->sections);
  p->sections = NULL;
  if (p->secbyname)
    hashtable_free (p->secbyname, NULL, NULL);
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
void free_relocs (struct prx *p)
{
  if (p->relocs)
    free (p->relocs);
  p->relocs = NULL;
}

static
void free_module_import (struct prx_import *imp)
{
  if (imp->funcs) free (imp->funcs);
  if (imp->vars) free (imp->vars);
  imp->funcs = NULL;
  imp->vars = NULL;
}

static
void free_module_imports (struct prx *p)
{
  if (!p->modinfo) return;
  if (p->modinfo->imports) {
    uint32 i;
    for (i = 0; i < p->modinfo->numimports; i++)
      free_module_import (&p->modinfo->imports[i]);
    free (p->modinfo->imports);
  }
  p->modinfo->imports = NULL;
}

static
void free_module_export (struct prx_export *exp)
{
  if (exp->funcs) free (exp->funcs);
  if (exp->vars) free (exp->vars);
  exp->funcs = NULL;
  exp->vars = NULL;
}

static
void free_module_exports (struct prx *p)
{
  if (!p->modinfo) return;
  if (p->modinfo->exports) {
    uint32 i;
    for (i = 0; i < p->modinfo->numexports; i++)
      free_module_export (&p->modinfo->exports[i]);
    free (p->modinfo->exports);
  }
  p->modinfo->imports = NULL;
}

static
void free_module_info (struct prx *p)
{
  free_module_imports (p);
  free_module_exports (p);
  if (p->modinfo)
    free (p->modinfo);
  p->modinfo = NULL;
}

void prx_free (struct prx *p)
{
  free_sections (p);
  free_programs (p);
  free_relocs (p);
  free_module_info (p);
  if (p->data)
    free ((void *) p->data);
  p->data = NULL;
  free (p);
}

static
void print_sections (struct prx *p)
{
  uint32 idx;
  struct elf_section *section;
  const char *type = "";

  if (!p->shnum) return;
  report ("\nSection Headers:\n");
  report ("  [Nr]  Name                        Type       Addr     Off      Size     ES Flg Lk Inf Al\n");

  for (idx = 0; idx < p->shnum; idx++) {
    section = &p->sections[idx];
    switch (section->type) {
    case SHT_NOBITS: type = "NOBITS"; break;
    case SHT_PRXRELOC: type = "PRXRELOC"; break;
    case SHT_STRTAB: type = "STRTAB"; break;
    case SHT_PROGBITS: type = "PROGBITS"; break;
    case SHT_NULL: type = "NULL"; break;
    }
    report ("  [%2d] %-28s %-10s %08X %08X %08X %02d %s%s%s %2d %2d  %2d\n",
            idx, section->name, type, section->addr, section->offset, section->size,
            section->entsize, (section->flags & SHF_ALLOC) ? "A" : " ",
            (section->flags & SHF_EXECINSTR) ? "X" : " ", (section->flags & SHF_WRITE) ? "W" : " ",
            section->link, section->info, section->addralign);
  }
}

static
void print_programs (struct prx *p)
{
  uint32 idx;
  struct elf_program *program;
  const char *type = "";

  if (!p->phnum) return;
  report ("\nProgram Headers:\n");
  report ("  Type  Offset     VirtAddr   PhysAddr   FileSiz    MemSiz     Flg Align\n");

  for (idx = 0; idx < p->phnum; idx++) {
    program = &p->programs[idx];
    switch (program->type) {
    case PT_LOAD: type = "LOAD"; break;
    case PT_PRX: type = "PRX"; break;
    }

    report ("  %-5s 0x%08X 0x%08X 0x%08X 0x%08X 0x%08X %s%s%s 0x%02X\n",
            type, program->offset, program->vaddr, program->paddr, program->filesz,
            program->memsz, (program->flags & PF_X) ? "X" : " ", (program->flags & PF_R) ? "R" : " ",
            (program->flags & PF_W) ? "W" : " ", program->align);
  }
}

static
void print_module_imports (struct prx *p)
{
  uint32 idx, i;
  struct prx_modinfo *info = p->modinfo;
  report ("\nImports:\n");
  for (idx = 0; idx < info->numimports; idx++) {
    struct prx_import *imp = &info->imports[idx];
    report ("  %s\n", imp->name);

    report ("     Flags:          0x%08X\n", imp->flags);
    report ("     Size:               %6d\n", imp->size);
    report ("     Num Variables:      %6d\n", imp->nvars);
    report ("     Num Functions:      %6d\n", imp->nfuncs);
    report ("     Nids:           0x%08X\n", imp->nidsvaddr);
    report ("     Functions:      0x%08X\n", imp->funcsvaddr);

    for (i = 0; i < imp->nfuncs; i++) {
      struct prx_function *f = &imp->funcs[i];
      report ("         NID: 0x%08X  VADDR: 0x%08X", f->nid, f->vaddr);
      if (f->name)
        report (" NAME: %s", f->name);
      report ("\n");
    }
    if (imp->nvars) {
      report ("     Variables:      0x%08X\n", imp->varsvaddr);
      for (i = 0; i < imp->nvars; i++) {
        struct prx_variable *v = &imp->vars[i];
        report ("         NID: 0x%08X  VADDR: 0x%08X", v->nid, v->vaddr);
        if (v->name)
          report (" NAME: %s", v->name);
        report ("\n");
      }
    }

    report ("\n");
  }
}

static
void print_module_exports (struct prx *p)
{
  uint32 idx, i;
  struct prx_modinfo *info = p->modinfo;
  report ("\nExports:\n");
  for (idx = 0; idx < info->numexports; idx++) {
    struct prx_export *exp = &info->exports[idx];
    report ("  %s\n", exp->name);

    report ("     Flags:          0x%08X\n", exp->flags);
    report ("     Size:               %6d\n", exp->size);
    report ("     Num Variables:      %6d\n", exp->nvars);
    report ("     Num Functions:      %6d\n", exp->nfuncs);
    report ("     Exports:        0x%08X\n", exp->expvaddr);
    if (exp->nfuncs) {
      report ("     Functions:\n");
      for (i = 0; i < exp->nfuncs; i++) {
        struct prx_function *f = &exp->funcs[i];
        report ("         NID: 0x%08X  VADDR: 0x%08X", f->nid, f->vaddr);
        if (f->name)
          report (" NAME: %s", f->name);
        report ("\n");
      }
    }
    if (exp->nvars) {
      report ("     Variables:\n");
      for (i = 0; i < exp->nvars; i++) {
        struct prx_variable *v = &exp->vars[i];
        report ("         NID: 0x%08X  VADDR: 0x%08X", v->nid, v->vaddr);
        if (v->name)
          report (" NAME: %s", v->name);
        report ("\n");
      }
    }
    report ("\n");
  }
}

static
void print_module_info (struct prx *p)
{
  struct prx_modinfo *info = p->modinfo;
  if (!info) return;

  report ("\nModule info:\n");
  report ("  Name: %31s\n", info->name);
  report ("  Attributes:                    0x%04X\n", info->attributes);
  report ("  Version:                       0x%04X\n", info->version);
  report ("  GP:                        0x%08X\n", info->gp);
  report ("  Library entry:             0x%08X\n", info->expvaddr);
  report ("  Library entry bottom:      0x%08X\n", info->expvaddrbtm);
  report ("  Library stubs:             0x%08X\n", info->impvaddr);
  report ("  Library stubs bottom:      0x%08X\n", info->impvaddrbtm);

  print_module_imports (p);
  print_module_exports (p);
}

void prx_print (struct prx *p)
{
  report ("ELF header:\n");
  report ("  Entry point address:        0x%08X\n", p->entry);
  report ("  Start of program headers:   0x%08X\n", p->phoff);
  report ("  Start of section headers:   0x%08X\n", p->shoff);
  report ("  Number of programs:           %8d\n", p->phnum);
  report ("  Number of sections:           %8d\n", p->shnum);

  print_sections (p);
  print_programs (p);
  print_module_info (p);

  report ("\n");
}

void prx_resolve_nids (struct prx *p, struct nidstable *nids)
{
  uint32 i, j;
  const char *name;
  struct prx_modinfo *info = p->modinfo;
  for (i = 0; i < info->numimports; i++) {
    struct prx_import *imp = &info->imports[i];
    for (j = 0; j < imp->nfuncs; j++) {
      struct prx_function *f = &imp->funcs[j];
      name = nids_find (nids, imp->name, f->nid);
      if (name) f->name = name;
    }
    for (j = 0; j < imp->nvars; j++) {
      struct prx_variable *v = &imp->vars[j];
      name = nids_find (nids, imp->name, v->nid);
      if (name) v->name = name;
    }
  }
  for (i = 0; i < info->numexports; i++) {
    struct prx_export *exp = &info->exports[i];
    for (j = 0; j < exp->nfuncs; j++) {
      struct prx_function *f = &exp->funcs[j];
      name = nids_find (nids, exp->name, f->nid);
      if (name) f->name = name;
    }
    for (j = 0; j < exp->nvars; j++) {
      struct prx_variable *v = &exp->vars[j];
      name = nids_find (nids, exp->name, v->nid);
      if (name) v->name = name;
    }
  }
}

uint32 prx_translate (struct prx *p, uint32 vaddr)
{
  uint32 idx;
  for (idx = 0; idx < p->phnum; idx++) {
    struct elf_program *program = &p->programs[idx];
    if (program->type != PT_LOAD) continue;
    if (vaddr >= program->vaddr &&
        (vaddr - program->vaddr) < program->memsz) {
      vaddr -= program->vaddr;
      if (vaddr < program->filesz)
        return vaddr + program->offset;
      else
        return 0;
    }
  }
  return 0;
}

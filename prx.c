
#include <stdlib.h>
#include <string.h>

#include "prx.h"
#include "nids.h"
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
void write_uint32_le (uint8 *bytes, uint32 val)
{
  bytes[0] = val & 0xFF; val >>= 8;
  bytes[1] = val & 0xFF; val >>= 8;
  bytes[2] = val & 0xFF; val >>= 8;
  bytes[3] = val & 0xFF;
}

static
int inside_prx (struct prx *p, uint32 offset, uint32 size)
{
  if (offset >= p->size || size > p->size ||
      size > (p->size - offset)) return 0;
  return 1;
}

static
int inside_progfile (struct elf_program *program, uint32 vaddr, uint32 size)
{
  if (vaddr < program->vaddr || size > program->filesz) return 0;

  vaddr -= program->vaddr;
  if (vaddr >= program->filesz || (program->filesz - vaddr) < size) return 0;
  return 1;
}

static
int inside_progmem (struct elf_program *program, uint32 vaddr, uint32 size)
{
  if (vaddr < program->vaddr || size > program->memsz) return 0;

  vaddr -= program->vaddr;
  if (vaddr >= program->memsz || (program->memsz - vaddr) < size) return 0;
  return 1;
}


static
int inside_strprogfile (struct elf_program *program, uint32 vaddr)
{
  if (vaddr < program->vaddr) return 0;

  vaddr -= program->vaddr;
  if (vaddr >= program->filesz) return 0;

  while (vaddr < program->filesz) {
    if (!program->data[vaddr]) return 1;
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
    if (section->size) {
      if (!inside_prx (p, section->offset, section->size)) {
        error (__FILE__ ": section is not inside ELF/PRX (section %d)", index);
        return 0;
      }
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
  if (!inside_prx (p, program->offset, program->filesz)) {
    error (__FILE__ ": program is not inside ELF/PRX (program %d)", index);
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
  case PT_PRXRELOC:
  case PT_PRXRELOC2:
    if (program->memsz) {
      error (__FILE__ ": program type must not loaded (program %d)", index);
      return 0;
    }
    break;
  default:
    error (__FILE__ ": invalid program type 0x%08X (program %d)", program->type, index);
    return 0;
  }

  return 1;
}

static
int cmp_relocs (const void *p1, const void *p2)
{
  const struct prx_reloc *r1 = p1;
  const struct prx_reloc *r2 = p2;
  if (r1->target < r2->target) return -1;
  if (r1->target > r2->target) return 1;
  return 0;
}

static
int cmp_relocs_by_addr (const void *p1, const void *p2)
{
  const struct prx_reloc *r1 = p1;
  const struct prx_reloc *r2 = p2;
  if (r1->vaddr < r2->vaddr) return -1;
  if (r1->vaddr > r2->vaddr) return 1;
  return 0;
}

static
int check_apply_relocs (struct prx *p)
{
  struct prx_reloc *r;
  struct elf_program *offsbase;
  struct elf_program *addrbase;
  uint32 index, addend, base, temp;
  uint32 hiaddr, loaddr;


  for (index = 0; index < p->relocnum; index++) {
    r = &p->relocs[index];
    if (r->offsbase >= p->phnum) {
      error (__FILE__ ": invalid offset base for relocation (%d)", r->offsbase);
      return 0;
    }

    if (r->addrbase >= p->phnum) {
      error (__FILE__ ": invalid address base for relocation (%d)", r->offsbase);
      return 0;
    }

    offsbase = &p->programs[r->offsbase];
    addrbase = &p->programs[r->addrbase];

    r->vaddr = r->offset + offsbase->vaddr;
    if (!inside_progfile (offsbase, r->vaddr, 4)) {
      error (__FILE__ ": relocation points to invalid address (0x%08X)", r->vaddr);
      return 0;
    }
  }

  for (index = 0; index < p->relocnum; index++) {
    r = &p->relocs[index];
    offsbase = &p->programs[r->offsbase];
    addrbase = &p->programs[r->addrbase];

    addend = read_uint32_le (&offsbase->data[r->offset]);

    switch (r->type) {
    case R_MIPS_NONE:
      break;
    case R_MIPS_26:
    case R_MIPSX_J26:
    case R_MIPSX_JAL26:
      r->target = (r->offset + offsbase->vaddr) & 0xF0000000;
      r->target = (((addend & 0x3FFFFFF) << 2) | r->target) + addrbase->vaddr;
      addend = (addend & ~0x3FFFFFF) | (r->target >> 2);
      if (!inside_progfile (addrbase, r->target, 8)) {
        error (__FILE__ ": mips26 reference out of range at 0x%08X (0x%08X)", r->vaddr, r->target);
      }
      write_uint32_le ((uint8 *)&offsbase->data[r->offset], addend);
      break;
    case R_MIPS_HI16:
      base = index;
      while (++index < p->relocnum) {
        if (p->relocs[index].type != R_MIPS_HI16) break;
        if (p->relocs[index].offsbase != r->offsbase) {
          error (__FILE__ ": changed offset base");
          return 0;
        }
        if (p->relocs[index].addrbase != r->addrbase) {
          error (__FILE__ ": changed offset base");
          return 0;
        }
        temp = read_uint32_le (&offsbase->data[p->relocs[index].offset]) & 0xFFFF;
        if (temp != (addend & 0xFFFF)) {
          error (__FILE__ ": changed hi");
          return 0;
        }
      }

      if (index == p->relocnum) {
        error (__FILE__ ": hi16 without matching lo16");
        return 0;
      }

      if (p->relocs[index].type != R_MIPS_LO16 ||
          p->relocs[index].offsbase != r->offsbase ||
          p->relocs[index].addrbase != r->addrbase) {
        error (__FILE__ ": hi16 without matching lo16");
        return 0;
      }

      temp = read_uint32_le (&offsbase->data[p->relocs[index].offset]) & 0xFFFF;
      if (temp & 0x8000) temp |= ~0xFFFF;

      r->target = ((addend & 0xFFFF) << 16) + addrbase->vaddr + temp;
      addend = temp & 0xFFFF;
      if (!inside_progmem (addrbase, r->target, 1)) {
        error (__FILE__ ": hi16 reference out of range at 0x%08X (0x%08X)", r->vaddr, r->target);
      }

      loaddr = r->target & 0xFFFF;
      hiaddr = (((r->target >> 16) + 1) >> 1) & 0xFFFF;

      while (base < index) {
        p->relocs[base].target = r->target;
        temp = (read_uint32_le (&offsbase->data[p->relocs[base].offset]) & ~0xFFFF) | hiaddr;
        write_uint32_le ((uint8 *) &offsbase->data[p->relocs[base].offset], temp);
        base++;
      }

      while (index < p->relocnum) {
        temp = read_uint32_le (&offsbase->data[p->relocs[index].offset]);
        if ((temp & 0xFFFF) != addend) break;
        if (p->relocs[index].type != R_MIPS_LO16) break;
        if (p->relocs[index].offsbase != r->offsbase) break;
        if (p->relocs[index].addrbase != r->addrbase) break;

        p->relocs[index].target = r->target;
        p->relocs[index].matched = TRUE;

        temp = (temp & ~0xFFFF) | loaddr;
        write_uint32_le ((uint8 *) &offsbase->data[p->relocs[index].offset], temp);
        index++;
      }
      index--;
      break;
    case R_MIPSX_HI16:
      r->target = ((addend & 0xFFFF) << 16) + addrbase->vaddr + r->addend;
      addend = (addend & ~0xFFFF) | ((((r->target >> 16) + 1) >> 1) & 0xFFFF);
      if (!inside_progmem (addrbase, r->target, 1)) {
        error (__FILE__ ": xhi16 reference out of range at 0x%08X (0x%08X)", r->vaddr, r->target);
      }
      write_uint32_le ((uint8 *)&offsbase->data[r->offset], addend);
      break;

    case R_MIPS_16:
    case R_MIPS_LO16:
      r->target = (addend & 0xFFFF) + addrbase->vaddr;
      addend = (addend & ~0xFFFF) | (r->target & 0xFFFF);
      if (!inside_progmem (addrbase, r->target, 1)) {
        error (__FILE__ ": lo16 reference out of range at 0x%08X (0x%08X)", r->vaddr, r->target);
      }
      write_uint32_le ((uint8 *)&offsbase->data[r->offset], addend);
      break;

    case R_MIPS_32:
      r->target = addend + addrbase->vaddr;
      addend = r->target;
      /*if (!inside_progmem (addrbase, r->target, 1)) {
        error (__FILE__ ": mips32 reference out of range at 0x%08X (0x%08X)", r->vaddr, r->target);
      }*/
      write_uint32_le ((uint8 *)&offsbase->data[r->offset], addend);
      break;

    default:
      error (__FILE__ ": invalid reference type %d", r->type);
      return 0;
    }

  }


  p->relocsbyaddr = xmalloc (p->relocnum * sizeof (struct prx_reloc));
  memcpy (p->relocsbyaddr, p->relocs, p->relocnum * sizeof (struct prx_reloc));

  qsort (p->relocs, p->relocnum, sizeof (struct prx_reloc), &cmp_relocs);
  qsort (p->relocsbyaddr, p->relocnum, sizeof (struct prx_reloc), &cmp_relocs_by_addr);

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
    if (!inside_progfile (p->programs, info->expvaddr, info->expvaddrbtm - info->expvaddr)) {
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
    if (!inside_progfile (p->programs, info->impvaddr, info->impvaddrbtm - info->impvaddr)) {
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

  if (!inside_strprogfile (p->programs, imp->namevaddr)) {
    error (__FILE__ ": import name not inside first program");
    return 0;
  }

  if (!imp->nfuncs && !imp->nvars) {
    error (__FILE__ ": no functions or variables imported");
    return 0;
  }

  if (!inside_progfile (p->programs, imp->funcsvaddr, 8 * imp->nfuncs)) {
    error (__FILE__ ": functions not inside the first program");
    return 0;
  }

  if (!inside_progfile (p->programs, imp->nidsvaddr, 4 * imp->nfuncs)) {
    error (__FILE__ ": nids not inside the first program");
    return 0;
  }

  if (imp->nvars) {
    if (!inside_progfile (p->programs, imp->varsvaddr, 8 * imp->nvars)) {
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

  if (!inside_strprogfile (p->programs, exp->namevaddr)) {
    error (__FILE__ ": export name not inside first program");
    return 0;
  }

  if (!exp->nfuncs && !exp->nvars) {
    error (__FILE__ ": no functions or variables exported");
    return 0;
  }

  if (!inside_progfile (p->programs, exp->expvaddr, 8 * (exp->nfuncs + exp->nvars))) {
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
  if (!inside_prx (p, p->phoff, table_size)) {
    error (__FILE__ ": wrong ELF program header table offset/size");
    return 0;
  }

  if (p->shnum && p->shentsize != ELF_SECTION_HEADER_ENT_SIZE) {
    error (__FILE__ ": wrong ELF section header entity size (%u)", p->shentsize);
    return 0;
  }

  table_size = p->shentsize;
  table_size *= (uint32) p->shnum;
  if (!inside_prx (p, p->shoff, table_size)) {
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
uint32 count_relocs_b (uint32 prgidx, const uint8 *data, uint32 size)
{
  const uint8 *end;
  uint8 part1s, part2s;
  uint32 block1s, block2s;
  uint8 block1[256], block2[256];
  uint32 temp1, temp2, part1, part2;
  uint32 count = 0, nbits;

  end = data + size;
  for (nbits = 1; (1 << nbits) < prgidx; nbits++) {
    if (nbits >= 33) {
      error (__FILE__  ": invalid number of bits for indexes");
      return 0;
    }
  }

  if (read_uint16_le (data) != 0) {
    error (__FILE__  ": invalid header for relocation");
    return 0;
  }

  part1s = data[2];
  part2s = data[3];

  block1s = data[4];
  data += 4;

  if (block1s) {
    memcpy (block1, data, block1s);
    data += block1s;
  }

  block2s = *data;
  if (block2s) {
    memcpy (block2, data, block2s);
    data += block2s;
  }


  count = 0;
  while (data < end) {
    uint32 cmd = read_uint16_le (data);
    temp1 = (cmd << (16 - part1s)) & 0xFFFF;
    temp1 = (temp1 >> (16 -part1s)) & 0xFFFF;

    data = data + 2;
    if (temp1 >= block1s) {
      error (__FILE__ ": invalid index for the first part");
      return 0;
    }
    part1 = block1[temp1];
    if ((part1 & 0x06) == 0x06) {
      error (__FILE__ ": invalid size");
      return 0;
    }

    data += part1 & 0x06;

    if ((part1 & 0x01) == 0) {
      if ((part1 & 0x06) == 2) {
        error (__FILE__ ": invalid size of part1");
        return 0;
      }
    } else {
      temp2 = (cmd << (16 - (part1s + nbits + part2s))) & 0xFFFF;
      temp2 = (temp2 >> (16 - part2s)) & 0xFFFF;
      if (temp2 >= block2s) {
        error (__FILE__ ": invalid index for the second part");
        return 0;
      }

      part2 = block2[temp2];

      switch (part1 & 0x38) {
      case 0x00:
        break;
      case 0x08:
        break;
      case 0x10:
        data += 2;
        break;
      default:
        error (__FILE__ ": invalid addendum size");
        return 0;
      }

      switch (part2) {
      case 1: case 2: case 3:
      case 4: case 5: case 6: case 7:
        count++;
        break;
      case 0:
        break;
      default:
        error (__FILE__ ": invalid relocation type %d", part2);
        return 0;
      }
    }
  }

  return count;
}

static
int load_relocs_b (struct elf_program *programs, struct prx_reloc *out, uint32 prgidx, const uint8 *data, uint32 size)
{
  const uint8 *end;
  uint32 nbits;
  uint8 part1s, part2s;
  uint32 block1s, block2s;
  uint8 block1[256], block2[256];
  uint32 temp1, temp2;
  uint32 part1, part2, lastpart2;
  uint32 addend = 0, offset = 0;
  uint32 offsbase = 0xFFFFFFFF;
  uint32 addrbase;
  uint32 count;

  end = data + size;
  for (nbits = 1; (1 << nbits) < prgidx; nbits++) {
  }

  part1s = data[2];
  part2s = data[3];

  block1s = data[4];
  data += 4;

  if (block1s) {
    memcpy (block1, data, block1s);
    data += block1s;
  }

  block2s = *data;
  if (block2s) {
    memcpy (block2, data, block2s);
    data += block2s;
  }


  count = 0;
  lastpart2 = block2s;
  while (data < end) {
    uint32 cmd = read_uint16_le (data);
    temp1 = (cmd << (16 - part1s)) & 0xFFFF;
    temp1 = (temp1 >> (16 -part1s)) & 0xFFFF;

    data += 2;
    part1= block1[temp1];

    if ((part1 & 0x01) == 0) {
      offsbase = (cmd << (16 - part1s - nbits)) & 0xFFFF;
      offsbase = (offsbase >> (16 - nbits)) & 0xFFFF;
      if (!(offsbase < prgidx)) {
        error (__FILE__ ": invalid offset base");
        return 0;
      }

      offset = cmd >> (part1s + nbits);
      if ((part1 & 0x06) == 0) continue;
      offset = read_uint32_le (data);
      data += 4;
    } else {
      temp2 = (cmd << (16 - (part1s + nbits + part2s))) & 0xFFFF;
      temp2 = (temp2 >> (16 - part2s)) & 0xFFFF;

      addrbase = (cmd << (16 - part1s - nbits)) & 0xFFFF;
      addrbase = (addrbase >> (16 - nbits)) & 0xFFFF;
      if (!(addrbase < prgidx)) {
        error (__FILE__ ": invalid address base");
        return 0;
      }
      part2 = block2[temp2];

      switch (part1 & 0x06) {
      case 0:
        if (cmd & 0x8000) {
          cmd |= ~0xFFFF;
          cmd >>= part1s + part2s + nbits;
          cmd |= ~0xFFFF;
        } else {
          cmd >>= part1s + part2s + nbits;
        }
        offset += cmd;
        break;
      case 2:
        if (cmd & 0x8000) cmd |= ~0xFFFF;
        cmd = (cmd >> (part1s + part2s + nbits)) << 16;
        cmd |= read_uint16_le (data);
        offset += cmd;
        data += 2;
        break;
      case 4:
        offset = read_uint32_le (data);
        data += 4;
        break;
      }

      if (!(offset < programs[offsbase].filesz)) {
        error (__FILE__ ": invalid relocation offset");
        return 0;
      }

      switch (part1 & 0x38) {
      case 0x00:
        addend = 0;
        break;
      case 0x08:
        if ((lastpart2 ^ 0x04) != 0) {
          addend = 0;
        }
        break;
      case 0x10:
        addend = read_uint16_le (data);
        data += 2;
        break;
      }

      lastpart2 = part2;

      out[count].addrbase = addrbase;
      out[count].offsbase = offsbase;
      out[count].offset = offset;
      out[count].extra = 0;

      switch (part2) {
      case 2:
        out[count++].type = R_MIPS_32;
        break;
      case 0:
        break;
      case 3:
        out[count++].type = R_MIPS_26;
        break;
      case 4:
        if (addend & 0x8000) addend |= ~0xFFFF;
        out[count].addend = addend;
        out[count++].type = R_MIPSX_HI16;
        break;
      case 1:
      case 5:
        out[count++].type = R_MIPS_LO16;
        break;
      case 6:
        out[count++].type = R_MIPSX_J26;
        break;
      case 7:
        out[count++].type = R_MIPSX_JAL26;
        break;
      }
    }
  }

  return count;
}

static
int load_relocs (struct prx *p)
{
  uint32 i, ret, count = 0;

  for (i = 0; i < p->shnum; i++) {
    struct elf_section *section = &p->sections[i];
    if (section->type == SHT_PRXRELOC) {
      count += section->size >> 3;
    }
  }
  for (i = 0; i < p->phnum; i++) {
    struct elf_program *program = &p->programs[i];
    if (program->type == PT_PRXRELOC) {
      count += program->filesz >> 3;
    } else if (program->type == PT_PRXRELOC2) {
      ret = count_relocs_b (i, program->data, program->filesz);
      if (!ret) return 0;
      count += ret;
    }
  }

  p->relocs = NULL;
  if (!count) {
    error (__FILE__ ": no relocation found");
    return 0;
  }

  p->relocnum = count;
  p->relocs = (struct prx_reloc *) xmalloc (count * sizeof (struct prx_reloc));
  memset (p->relocs, 0, count * sizeof (struct prx_reloc));

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
        p->relocs[count].type = p->data[offset + 4];
        p->relocs[count].offsbase = p->data[offset + 5];
        p->relocs[count].addrbase = p->data[offset + 6];
        p->relocs[count].extra = p->data[offset + 7];

        count++;
        offset += 8;
      }
    }
  }

  for (i = 0; i < p->phnum; i++) {
    struct elf_program *program = &p->programs[i];
    if (program->type == PT_PRXRELOC) {
      uint32 j, progsize;
      uint32 offset;
      offset = program->offset;
      progsize = program->filesz >> 3;
      for (j = 0; j < progsize; j++) {
        p->relocs[count].offset = read_uint32_le (&p->data[offset]);
        p->relocs[count].type = p->data[offset + 4];
        p->relocs[count].offsbase = p->data[offset + 5];
        p->relocs[count].addrbase = p->data[offset + 6];
        p->relocs[count].extra = p->data[offset + 7];

        count++;
        offset += 8;
      }
    } else if (program->type == PT_PRXRELOC2) {
      ret = load_relocs_b (p->programs, &p->relocs[count], i, program->data, program->filesz);
      if (!ret) {
        return 0;
      }
      count += ret;
    }
  }

  if (!check_apply_relocs (p)) return 0;

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
  memset (p, 0, sizeof (struct prx));
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

  if (p->relocsbyaddr)
    free (p->relocsbyaddr);
  p->relocsbyaddr = NULL;
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
    case PT_PRXRELOC: type = "REL"; break;
    case PT_PRXRELOC2: type = "REL2"; break;
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

void print_relocs (struct prx *p)
{
  uint32 i;
  report ("\nRelocs:\n");
  for (i = 0; i < p->relocnum; i++) {
    const char *type = "unk";

    switch (p->relocs[i].type) {
    case R_MIPSX_HI16:  type = "xhi16"; break;
    case R_MIPSX_J26:   type = "xj26"; break;
    case R_MIPSX_JAL26: type = "xjal26"; break;
    case R_MIPS_16:     type = "mips16"; break;
    case R_MIPS_26:     type = "mips26"; break;
    case R_MIPS_32:     type = "mips32"; break;
    case R_MIPS_HI16:   type = "hi16"; break;
    case R_MIPS_LO16:   type = "lo16"; break;
    case R_MIPS_NONE:   type = "none"; break;
    }
    report ("  Type: %8s Vaddr: 0x%08X Target: 0x%08X Addend: 0x%08X\n",
        type, p->relocs[i].vaddr, p->relocs[i].target, p->relocs[i].addend);
  }
}

void prx_print (struct prx *p, int prtrelocs)
{
  report ("ELF header:\n");
  report ("  Entry point address:        0x%08X\n", p->entry);
  report ("  Start of program headers:   0x%08X\n", p->phoff);
  report ("  Start of section headers:   0x%08X\n", p->shoff);
  report ("  Number of programs:           %8d\n", p->phnum);
  report ("  Number of sections:           %8d\n", p->shnum);

  print_sections (p);
  print_programs (p);
  if (prtrelocs)
    print_relocs (p);
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


uint32 prx_findreloc (struct prx *p, uint32 target)
{
  uint32 first, last, i;

  first = 0;
  last = p->relocnum - 1;
  while (first < last) {
    i = (first + last) / 2;
    if (p->relocs[i].target < target) {
      first = i + 1;
    } else {
      last = i;
    }
  }

  return first;
}

uint32 prx_findrelocbyaddr (struct prx *p, uint32 vaddr)
{
  uint32 first, last, i;

  first = 0;
  last = p->relocnum - 1;
  while (first < last) {
    i = (first + last) / 2;
    if (p->relocsbyaddr[i].vaddr < vaddr) {
      first = i + 1;
    } else {
      last = i;
    }
  }

  return first;
}

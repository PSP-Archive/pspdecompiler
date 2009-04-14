
#include <string.h>

#include "code.h"
#include "utils.h"


static
void new_subroutine (struct code *c, struct location *loc, struct prx_function *imp, struct prx_function *exp)
{
  struct subroutine *sub = loc->sub;
  if (!sub) {
    sub = fixedpool_alloc (c->subspool);
    memset (sub, 0, sizeof (struct subroutine));
    sub->location = loc;
    loc->sub = sub;
  }
  if (imp) sub->import = imp;
  if (exp) sub->export = exp;

  if (sub->import && sub->export) {
    sub->haserror = TRUE;
    error (__FILE__ ": location 0x%08X is both import and export", loc->address);
  }
}

static
void analyse_relocs (struct code *c)
{
  uint32 i, tgt;

  i = prx_findreloc (c->file, c->baddr);
  for (; i < c->file->relocnum; i++) {
    struct location *loc;
    struct prx_reloc *rel = &c->file->relocs[i];

    tgt = (rel->target - c->baddr) >> 2;
    if (tgt >= c->numopc) continue;

    if (rel->target & 0x03) {
      error (__FILE__ ": relocation not word aligned 0x%08X", rel->target);
      continue;
    }

    loc = &c->base[tgt];

    if (rel->type == R_MIPSX_JAL26) {
      new_subroutine (c, loc, NULL, NULL);
    } else if (rel->type == R_MIPS_26) {
      struct location *calledfrom;
      uint32 ctgt;

      if (rel->vaddr < c->baddr) continue;
      ctgt = (rel->vaddr - c->baddr) >> 2;
      if (ctgt >= c->numopc) continue;

      calledfrom = &c->base[ctgt];
      if (calledfrom->insn->insn == I_JAL) {
        new_subroutine (c, loc, NULL, NULL);
      }
    } else if (rel->type == R_MIPS_32) {
      if (!loc->switchtarget)
        new_subroutine (c, loc, NULL, NULL);
    } else if (rel->type == R_MIPS_HI16 || rel->type == R_MIPSX_HI16) {
      /* TODO */
      if (!loc->switchtarget)
        new_subroutine (c, loc, NULL, NULL);
    }
  }
}

static
void mark_reachable (struct code *c, struct location *loc)
{
  while (loc) {
    if (loc->reachable == 1) break;
    loc->reachable = 1;

    if (!loc->insn) return;

    if (loc->insn->flags & INSN_JUMP) {
      if (loc != c->end) loc[1].reachable = 2;

      if (loc->cswitch) {
        element el;
        el = list_head (loc->cswitch->references);
        while (el) {
          mark_reachable (c, element_getvalue (el));
          el = element_next (el);
        }
        return;
      }

      if (loc->insn->flags & (INSN_LINK | INSN_WRITE_GPR_D)) {
        if (loc->target)
          mark_reachable (c, loc->target);

        if ((loc->address + 8) > c->end->address)
          return;
        loc += 2;
      } else
        loc = loc->target;
    } else if (loc->insn->flags & INSN_BRANCH) {
      if (loc != c->end) {
        if (!(loc->insn->flags & INSN_BRANCHLIKELY) || (loc->branchtype != BRANCH_NEVER))
          loc[1].reachable = 2;
      }

      if (loc->branchtype == BRANCH_ALWAYS) {
        if (loc->insn->flags & INSN_LINK) {
          mark_reachable (c, loc->target);
          if ((loc->address + 8) > c->end->address)
            return;
          loc += 2;
        } else
          loc = loc->target;
      } else {
        if (loc->branchtype == BRANCH_NORMAL) {
          mark_reachable (c, loc->target);
        }

        if ((loc->address + 8) > c->end->address)
          return;
        loc += 2;
      }
    } else {
      if (loc++ == c->end) return;
    }
  };
}

static
void find_hidden_subroutines (struct code *c)
{
  struct subroutine *cursub;
  uint32 i;

  for (i = 0; i < c->numopc; i++) {
    struct location *loc = &c->base[i];
    if (loc->sub)
      mark_reachable (c, loc);
  }

  for (i = 0; i < c->numopc; i++) {
    struct location *loc = &c->base[i];
    if (loc->sub) {
      cursub = loc->sub;
    }

    if (loc->target && (loc->branchtype != BRANCH_NEVER)) {
      struct location *aux = loc->target;
      if (!loc->target->sub) {
        do {
          if (aux->sub) {
            if (aux->sub != cursub) {
              if (!loc->reachable) {
                report (__FILE__ ": hidden subroutine at 0x%08X discovered because of 0x%08X (cursub = 0x%08X, sub = 0x%08X)\n",
                    loc->target->address, loc->address, cursub->location->address, aux->sub->location->address);
                new_subroutine (c, loc->target, NULL, NULL);
                mark_reachable (c, loc->target);
              } else {
                loc->error = ERROR_ILLEGAL_JUMP;
                loc->target->error = ERROR_ILLEGAL_JUMP;
                error (__FILE__ ": jump to a subroutine internal location at 0x%08X\n", loc->address);
              }
            }
            break;
          }
        } while (aux-- != c->base);
      }
    }
  }
}


static
void analyse_subroutine (struct code *c, struct subroutine *sub)
{
  struct location *loc;
  loc = sub->location;
  do {
    if (!loc->reachable) continue;

    if (loc->error) {
      switch (loc->error) {
      case ERROR_INVALID_OPCODE:
        error (__FILE__ ": invalid opcode 0x%08X at 0x%08X", loc->opc, loc->address);
        break;
      case ERROR_TARGET_OUTSIDE_FILE:
        error (__FILE__ ": branch/jump outside file at 0x%08X\n", loc->address);
        break;
      case ERROR_DELAY_SLOT:
        error (__FILE__ ": branch/jump inside delay slot at 0x%08X", loc->address);
        break;
      }

      sub->haserror = 1;
      return;
    }


    if (loc->target && (loc->branchtype != BRANCH_NEVER)) {
      if (!loc->target->references)
        loc->target->references = list_alloc (c->lstpool);
      list_inserttail (loc->target->references, loc);

      if (loc->target->sub != loc->sub) {
        if (!(loc->insn->flags & (INSN_LINK | INSN_WRITE_GPR_D))) {
          report (__FILE__ ": jumped call at 0x%08X\n", loc->address);
        }
      }
    }
  } while (loc++ != sub->end);
}


void extract_subroutines (struct code *c)
{
  uint32 i, j, tgt;
  struct subroutine *prevsub = NULL;
  element el;

  c->subroutines = list_alloc (c->lstpool);
  analyse_relocs (c);

  for (i = 0; i < c->file->modinfo->numexports; i++) {
    struct prx_export *exp;

    exp = &c->file->modinfo->exports[i];
    for (j = 0; j < exp->nfuncs; j++) {
      struct location *loc;

      tgt = (exp->funcs[j].vaddr - c->baddr) >> 2;
      if (exp->funcs[j].vaddr < c->baddr ||
          tgt >= c->numopc) {
        error (__FILE__ ": invalid exported function");
        continue;
      }

      loc = &c->base[tgt];
      new_subroutine (c, loc, NULL, &exp->funcs[j]);
    }
  }

  for (i = 0; i < c->file->modinfo->numimports; i++) {
    struct prx_import *imp;

    imp = &c->file->modinfo->imports[i];
    for (j = 0; j < imp->nfuncs; j++) {
      struct location *loc;

      tgt = (imp->funcs[j].vaddr - c->baddr) >> 2;
      if (imp->funcs[j].vaddr < c->baddr ||
          tgt >= c->numopc) {
        error (__FILE__ ": invalid imported function");
        continue;
      }

      loc = &c->base[tgt];
      new_subroutine (c, loc, &imp->funcs[j], NULL);
    }
  }

  if (!c->base->sub) {
    error (__FILE__ ": creating artificial subroutine at address 0x%08X", c->baddr);
    new_subroutine (c, c->base, NULL, NULL);
  }

  find_hidden_subroutines (c);

  for (i = 0; i < c->numopc; i++) {
    if (c->base[i].sub) {
      list_inserttail (c->subroutines, c->base[i].sub);
      if (prevsub) {
        prevsub->end = &c->base[i - 1];
      }
      prevsub = c->base[i].sub;
    } else {
      c->base[i].sub = prevsub;
    }
  }
  if (prevsub) {
    prevsub->end = &c->base[i - 1];
  }



  el = list_head (c->subroutines);
  while (el) {
    analyse_subroutine (c, element_getvalue (el));
    el = element_next (el);
  }
}


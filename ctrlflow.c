
#include "code.h"
#include "utils.h"

int extract_cfg (struct code *c, struct subroutine *sub)
{
  struct location *begin, *end;
  begin = sub->location;
  do {
    end = begin;
    do {
      if (end->references && end != begin) break;
      if (end->insn->flags & INSN_JUMP) {

      } else if (end->insn->flags & INSN_BRANCH) {

      }
    } while (end++ != sub->end);
    end--;

  } while (begin++ != sub->end);
  return 0;
}

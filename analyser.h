#ifndef __ANALYSER_H
#define __ANALYSER_H

#include "allegrex.h"

enum allegrex_jumptype {
  JTYPE_NONE,
  JTYPE_BRANCH,
  JTYPE_BRANCHLIKELY,
  JTYPE_BRANCHANDLINK,
  JTYPE_BRANCHANDLINKLIKELY,
  JTYPE_JUMP,
  JTYPE_JUMPANDLINK,
  JTYPE_JUMPREGISTER,
  JTYPE_JUMPANDLINKREGISTER
};

struct code_position {
  uint32 opc;
  enum allegrex_itype itype;
  enum allegrex_jumptype jumptype;
  struct code_position *target;
  struct code_position *jumprefs;
  struct code_position *callrefs;
  struct code_position *next;
};

#endif /* __ANALYSER_H

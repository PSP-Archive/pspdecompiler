#ifndef __ANALYSER_H
#define __ANALYSER_H


#define JTYPE_BRANCH 0x01
#define JTYPE_LIKELY 0x02
#define JTYPE_LINK   0x04

struct code_position {
  uint32 opcode;

  int jumptype;
  struct code_position *target;
  struct code_position *next_jump;
  struct code_position *next_call;
};

#endif /* __ANALYSER_H

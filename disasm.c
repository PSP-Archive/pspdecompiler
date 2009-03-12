#include <stdio.h>
#include <ctype.h>

/* Format codes
 * %d - Rd
 * %t - Rt
 * %s - Rs
 * %i - 16bit signed immediate
 * %I - 16bit unsigned immediate (always printed in hex)
 * %o - 16bit signed offset (rt base)
 * %O - 16bit signed offset (PC relative)
 * %j - 26bit absolute offset
 * %J - Register jump
 * %a - SA
 * %0 - Cop0 register
 * %1 - Cop1 register
 * %2? - Cop2 register (? is (s, d))
 * %p - General cop (i.e. numbered) register
 * %n? - ins/ext size, ? (e, i)
 * %r - Debug register
 * %k - Cache function
 * %D - Fd
 * %T - Ft
 * %S - Fs
 * %x? - Vt (? is (s/scalar, p/pair, t/triple, q/quad, m/matrix pair, n/matrix triple, o/matrix quad)
 * %y? - Vs
 * %z? - Vd
 * %X? - Vo (? is (s, q))
 * %Y - VFPU offset
 * %Z? - VFPU condition code/name (? is (c, n))
 * %v? - VFPU immediate, ? (3, 5, 8, k, i, h, r, p? (? is (0, 1, 2, 3, 4, 5, 6, 7)))
 * %c - code (for break)
 * %C - code (for syscall)
 * %? - Indicates vmmul special exception
 */

#define RT(op) ((op >> 16) & 0x1F)
#define RS(op) ((op >> 21) & 0x1F)
#define RD(op) ((op >> 11) & 0x1F)
#define FT(op) ((op >> 16) & 0x1F)
#define FS(op) ((op >> 11) & 0x1F)
#define FD(op) ((op >> 6) & 0x1F)
#define SA(op) ((op >> 6)  & 0x1F)
#define IMM(op) ((signed short) (op & 0xFFFF))
#define IMMU(op) ((unsigned short) (op & 0xFFFF))
#define JUMP(op, pc) ((pc & 0xF0000000) | ((op & 0x3FFFFFF) << 2))
#define CODE(op) ((op >> 6) & 0xFFFFF)
#define SIZE(op) ((op >> 11) & 0x1F)
#define POS(op)  ((op >> 6) & 0x1F)
#define VO(op)   (((op & 3) << 5) | ((op >> 16) & 0x1F))
#define VCC(op)  ((op >> 18) & 7)
#define VD(op)   (op & 0x7F)
#define VS(op)   ((op >> 8) & 0x7F)
#define VT(op)   ((op >> 16) & 0x7F)

/* [hlide] new #defines */
#define VED(op)  (op & 0xFF)
#define VES(op)  ((op >> 8) & 0xFF)
#define VCN(op)  (op & 0x0F)
#define VI3(op)  ((op >> 16) & 0x07)
#define VI5(op)  ((op >> 16) & 0x1F)
#define VI8(op)  ((op >> 16) & 0xFF)

/* VFPU 16-bit floating-point format. */
#define VFPU_FLOAT16_EXP_MAX    0x1f
#define VFPU_SH_FLOAT16_SIGN    15
#define VFPU_MASK_FLOAT16_SIGN  0x1
#define VFPU_SH_FLOAT16_EXP     10
#define VFPU_MASK_FLOAT16_EXP   0x1f
#define VFPU_SH_FLOAT16_FRAC    0
#define VFPU_MASK_FLOAT16_FRAC  0x3ff

/* VFPU prefix instruction operands.  The *_SH_* values really specify where
   the bitfield begins, as VFPU prefix instructions have four operands
   encoded within the immediate field. */
#define VFPU_SH_PFX_NEG         16
#define VFPU_MASK_PFX_NEG       0x1     /* Negation. */
#define VFPU_SH_PFX_CST         12
#define VFPU_MASK_PFX_CST       0x1     /* Constant. */
#define VFPU_SH_PFX_ABS_CSTHI   8
#define VFPU_MASK_PFX_ABS_CSTHI 0x1     /* Abs/Constant (bit 2). */
#define VFPU_SH_PFX_SWZ_CSTLO   0
#define VFPU_MASK_PFX_SWZ_CSTLO 0x3     /* Swizzle/Constant (bits 0-1). */
#define VFPU_SH_PFX_MASK        8
#define VFPU_MASK_PFX_MASK      0x1     /* Mask. */
#define VFPU_SH_PFX_SAT         0
#define VFPU_MASK_PFX_SAT       0x3     /* Saturation. */

/* Special handling of the vrot instructions. */
#define VFPU_MASK_OP_SIZE       0x8080  /* Masks the operand size (pair, triple, quad). */
#define VFPU_OP_SIZE_PAIR       0x80
#define VFPU_OP_SIZE_TRIPLE     0x8000
#define VFPU_OP_SIZE_QUAD       0x8080
/* Note that these are within the rotators field, and not the full opcode. */
#define VFPU_SH_ROT_HI          2
#define VFPU_MASK_ROT_HI        0x3
#define VFPU_SH_ROT_LO          0
#define VFPU_MASK_ROT_LO        0x3
#define VFPU_SH_ROT_NEG         4       /* Negation. */
#define VFPU_MASK_ROT_NEG       0x1


struct Instruction
{
  const char *name;
  unsigned int opcode;
  unsigned int mask;
  const char *fmt;
  int addrtype;
  int type;
};

#define INSTR_TYPE_PSP    1
#define INSTR_TYPE_B      2
#define INSTR_TYPE_JUMP   4
#define INSTR_TYPE_JAL    8

#define INSTR_TYPE_BRANCH (INSTR_TYPE_B | INSTR_TYPE_JUMP | INSTR_TYPE_JAL)

#define ADDR_TYPE_NONE 0
#define ADDR_TYPE_16   1
#define ADDR_TYPE_26   2
#define ADDR_TYPE_REG  3

static struct Instruction g_inst[] =
{
  /* Macro instructions */
  { "nop",        0x00000000, 0xFFFFFFFF, "",       ADDR_TYPE_NONE, 0 },
  { "li",         0x24000000, 0xFFE00000, "%t, %i", ADDR_TYPE_NONE, 0 },
  { "li",         0x34000000, 0xFFE00000, "%t, %I", ADDR_TYPE_NONE, 0 },
  { "move",       0x00000021, 0xFC1F07FF, "%d, %s", ADDR_TYPE_NONE, 0 },
  { "move",       0x00000025, 0xFC1F07FF, "%d, %s", ADDR_TYPE_NONE, 0 },
  { "b",          0x10000000, 0xFFFF0000, "%O",     ADDR_TYPE_16,   INSTR_TYPE_B },
  { "b",          0x04010000, 0xFFFF0000, "%O",     ADDR_TYPE_16,   INSTR_TYPE_B },
  { "bal",        0x04110000, 0xFFFF0000, "%O",     ADDR_TYPE_16,   INSTR_TYPE_JAL },
  { "bnez",       0x14000000, 0xFC1F0000, "%s, %O", ADDR_TYPE_16,   INSTR_TYPE_B },
  { "bnezl",      0x54000000, 0xFC1F0000, "%s, %O", ADDR_TYPE_16,   INSTR_TYPE_B },
  { "beqz",       0x10000000, 0xFC1F0000, "%s, %O", ADDR_TYPE_16,   INSTR_TYPE_B },
  { "beqzl",      0x50000000, 0xFC1F0000, "%s, %O", ADDR_TYPE_16,   INSTR_TYPE_B },
  { "neg",        0x00000022, 0xFFE007FF, "%d, %t", ADDR_TYPE_NONE, 0 },
  { "negu",       0x00000023, 0xFFE007FF, "%d, %t", ADDR_TYPE_NONE, 0 },
  { "not",        0x00000027, 0xFC1F07FF, "%d, %s", ADDR_TYPE_NONE, 0 },
  { "jalr",       0x0000F809, 0xFC1FFFFF, "%J",     ADDR_TYPE_REG,  INSTR_TYPE_JAL },

  /* MIPS instructions */
  { "add",         0x00000020, 0xFC0007FF, "%d, %s, %t", ADDR_TYPE_NONE, 0 },
  { "addi",        0x20000000, 0xFC000000, "%t, %s, %i", ADDR_TYPE_NONE, 0 },
  { "addiu",       0x24000000, 0xFC000000, "%t, %s, %i", ADDR_TYPE_NONE, 0 },
  { "addu",        0x00000021, 0xFC0007FF, "%d, %s, %t", ADDR_TYPE_NONE, 0 },
  { "and",         0x00000024, 0xFC0007FF, "%d, %s, %t", ADDR_TYPE_NONE, 0 },
  { "andi",        0x30000000, 0xFC000000, "%t, %s, %I", ADDR_TYPE_NONE, 0 },
  { "beq",         0x10000000, 0xFC000000, "%s, %t, %O", ADDR_TYPE_16,   INSTR_TYPE_B },
  { "beql",        0x50000000, 0xFC000000, "%s, %t, %O", ADDR_TYPE_16,   INSTR_TYPE_B },
  { "bgez",        0x04010000, 0xFC1F0000, "%s, %O",     ADDR_TYPE_16,   INSTR_TYPE_B },
  { "bgezal",      0x04110000, 0xFC1F0000, "%s, %O",     ADDR_TYPE_16,   INSTR_TYPE_JAL },
  { "bgezl",       0x04030000, 0xFC1F0000, "%s, %O",     ADDR_TYPE_16,   INSTR_TYPE_B },
  { "bgtz",        0x1C000000, 0xFC1F0000, "%s, %O",     ADDR_TYPE_16,   INSTR_TYPE_B },
  { "bgtzl",       0x5C000000, 0xFC1F0000, "%s, %O",     ADDR_TYPE_16,   INSTR_TYPE_B },
  { "bitrev",      0x7C000520, 0xFFE007FF, "%d, %t",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "blez",        0x18000000, 0xFC1F0000, "%s, %O",     ADDR_TYPE_16,   INSTR_TYPE_B },
  { "blezl",       0x58000000, 0xFC1F0000, "%s, %O",     ADDR_TYPE_16,   INSTR_TYPE_B },
  { "bltz",        0x04000000, 0xFC1F0000, "%s, %O",     ADDR_TYPE_16,   INSTR_TYPE_B },
  { "bltzl",       0x04020000, 0xFC1F0000, "%s, %O",     ADDR_TYPE_16,   INSTR_TYPE_B },
  { "bltzal",      0x04100000, 0xFC1F0000, "%s, %O",     ADDR_TYPE_16,   INSTR_TYPE_JAL },
  { "bltzall",     0x04120000, 0xFC1F0000, "%s, %O",     ADDR_TYPE_16,   INSTR_TYPE_JAL },
  { "bne",         0x14000000, 0xFC000000, "%s, %t, %O", ADDR_TYPE_16,   INSTR_TYPE_B },
  { "bnel",        0x54000000, 0xFC000000, "%s, %t, %O", ADDR_TYPE_16,   INSTR_TYPE_B },
  { "break",       0x0000000D, 0xFC00003F, "%c",         ADDR_TYPE_NONE, 0 },
  { "cache",       0xbc000000, 0xfc000000, "%k, %o",     ADDR_TYPE_NONE, 0 },
  { "cfc0",        0x40400000, 0xFFE007FF, "%t, %p",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "clo",         0x00000017, 0xFC1F07FF, "%d, %s",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "clz",         0x00000016, 0xFC1F07FF, "%d, %s",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "ctc0",        0x40C00000, 0xFFE007FF, "%t, %p",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "max",         0x0000002C, 0xFC0007FF, "%d, %s, %t", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "min",         0x0000002D, 0xFC0007FF, "%d, %s, %t", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "dbreak",      0x7000003F, 0xFFFFFFFF, "",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "div",         0x0000001A, 0xFC00FFFF, "%s, %t",     ADDR_TYPE_NONE, 0 },
  { "divu",        0x0000001B, 0xFC00FFFF, "%s, %t",     ADDR_TYPE_NONE, 0 },
  { "dret",        0x7000003E, 0xFFFFFFFF, "",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "eret",        0x42000018, 0xFFFFFFFF, "",           ADDR_TYPE_NONE, 0 },
  { "ext",         0x7C000000, 0xFC00003F, "%t, %s, %a, %ne", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "ins",         0x7C000004, 0xFC00003F, "%t, %s, %a, %ni", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "j",           0x08000000, 0xFC000000, "%j",         ADDR_TYPE_26,   INSTR_TYPE_JUMP },
  { "jr",          0x00000008, 0xFC1FFFFF, "%J",         ADDR_TYPE_REG,  INSTR_TYPE_JUMP },
  { "jalr",        0x00000009, 0xFC1F07FF, "%J, %d",     ADDR_TYPE_REG,  INSTR_TYPE_JAL },
  { "jal",         0x0C000000, 0xFC000000, "%j",         ADDR_TYPE_26,   INSTR_TYPE_JAL },
  { "lb",          0x80000000, 0xFC000000, "%t, %o",     ADDR_TYPE_NONE, 0 },
  { "lbu",         0x90000000, 0xFC000000, "%t, %o",     ADDR_TYPE_NONE, 0 },
  { "lh",          0x84000000, 0xFC000000, "%t, %o",     ADDR_TYPE_NONE, 0 },
  { "lhu",         0x94000000, 0xFC000000, "%t, %o",     ADDR_TYPE_NONE, 0 },
  { "ll",          0xC0000000, 0xFC000000, "%t, %O",     ADDR_TYPE_NONE, 0 },
  { "lui",         0x3C000000, 0xFFE00000, "%t, %I",     ADDR_TYPE_NONE, 0 },
  { "lw",          0x8C000000, 0xFC000000, "%t, %o",     ADDR_TYPE_NONE, 0 },
  { "lwl",         0x88000000, 0xFC000000, "%t, %o",     ADDR_TYPE_NONE, 0 },
  { "lwr",         0x98000000, 0xFC000000, "%t, %o",     ADDR_TYPE_NONE, 0 },
  { "madd",        0x0000001C, 0xFC00FFFF, "%s, %t",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "maddu",       0x0000001D, 0xFC00FFFF, "%s, %t",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "mfc0",        0x40000000, 0xFFE007FF, "%t, %0",     ADDR_TYPE_NONE, 0 },
  { "mfdr",        0x7000003D, 0xFFE007FF, "%t, %r",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "mfhi",        0x00000010, 0xFFFF07FF, "%d",         ADDR_TYPE_NONE, 0 },
  { "mfic",        0x70000024, 0xFFE007FF, "%t, %p",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "mflo",        0x00000012, 0xFFFF07FF, "%d",         ADDR_TYPE_NONE, 0 },
  { "movn",        0x0000000B, 0xFC0007FF, "%d, %s, %t", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "movz",        0x0000000A, 0xFC0007FF, "%d, %s, %t", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "msub",        0x0000002e, 0xfc00ffff, "%d, %t",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "msubu",       0x0000002f, 0xfc00ffff, "%d, %t",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "mtc0",        0x40800000, 0xFFE007FF, "%t, %0",     ADDR_TYPE_NONE, 0 },
  { "mtdr",        0x7080003D, 0xFFE007FF, "%t, %r",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "mtic",        0x70000026, 0xFFE007FF, "%t, %p",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "halt",        0x70000000, 0xFFFFFFFF, "",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "mthi",        0x00000011, 0xFC1FFFFF, "%s",         ADDR_TYPE_NONE, 0 },
  { "mtlo",        0x00000013, 0xFC1FFFFF, "%s",         ADDR_TYPE_NONE, 0 },
  { "mult",        0x00000018, 0xFC00FFFF, "%s, %t",     ADDR_TYPE_NONE, 0 },
  { "multu",       0x00000019, 0xFC0007FF, "%s, %t",     ADDR_TYPE_NONE, 0 },
  { "nor",         0x00000027, 0xFC0007FF, "%d, %s, %t", ADDR_TYPE_NONE, 0 },
  { "or",          0x00000025, 0xFC0007FF, "%d, %s, %t", ADDR_TYPE_NONE, 0 },
  { "ori",         0x34000000, 0xFC000000, "%t, %s, %I", ADDR_TYPE_NONE, 0 },
  { "rotr",        0x00200002, 0xFFE0003F, "%d, %t, %a", ADDR_TYPE_NONE, 0 },
  { "rotv",        0x00000046, 0xFC0007FF, "%d, %t, %s", ADDR_TYPE_NONE, 0 },
  { "seb",         0x7C000420, 0xFFE007FF, "%d, %t",     ADDR_TYPE_NONE, 0 },
  { "seh",         0x7C000620, 0xFFE007FF, "%d, %t",     ADDR_TYPE_NONE, 0 },
  { "sb",          0xA0000000, 0xFC000000, "%t, %o",     ADDR_TYPE_NONE, 0 },
  { "sh",          0xA4000000, 0xFC000000, "%t, %o",     ADDR_TYPE_NONE, 0 },
  { "sllv",        0x00000004, 0xFC0007FF, "%d, %t, %s", ADDR_TYPE_NONE, 0 },
  { "sll",         0x00000000, 0xFFE0003F, "%d, %t, %a", ADDR_TYPE_NONE, 0 },
  { "slt",         0x0000002A, 0xFC0007FF, "%d, %s, %t", ADDR_TYPE_NONE, 0 },
  { "slti",        0x28000000, 0xFC000000, "%t, %s, %i", ADDR_TYPE_NONE, 0 },
  { "sltiu",       0x2C000000, 0xFC000000, "%t, %s, %i", ADDR_TYPE_NONE, 0 },
  { "sltu",        0x0000002B, 0xFC0007FF, "%d, %s, %t", ADDR_TYPE_NONE, 0 },
  { "sra",         0x00000003, 0xFFE0003F, "%d, %t, %a", ADDR_TYPE_NONE, 0 },
  { "srav",        0x00000007, 0xFC0007FF, "%d, %t, %s", ADDR_TYPE_NONE, 0 },
  { "srlv",        0x00000006, 0xFC0007FF, "%d, %t, %s", ADDR_TYPE_NONE, 0 },
  { "srl",         0x00000002, 0xFFE0003F, "%d, %t, %a", ADDR_TYPE_NONE, 0 },
  { "sw",          0xAC000000, 0xFC000000, "%t, %o",     ADDR_TYPE_NONE, 0 },
  { "swl",         0xA8000000, 0xFC000000, "%t, %o",     ADDR_TYPE_NONE, 0 },
  { "swr",         0xB8000000, 0xFC000000, "%t, %o",     ADDR_TYPE_NONE, 0 },
  { "sub",         0x00000022, 0xFC0007FF, "%d, %s, %t", ADDR_TYPE_NONE, 0 },
  { "subu",        0x00000023, 0xFC0007FF, "%d, %s, %t", ADDR_TYPE_NONE, 0 },
  { "sync",        0x0000000F, 0xFFFFFFFF, "",           ADDR_TYPE_NONE, 0 },
  { "syscall",     0x0000000C, 0xFC00003F, "%C",         ADDR_TYPE_NONE, 0 },
  { "xor",         0x00000026, 0xFC0007FF, "%d, %s, %t", ADDR_TYPE_NONE, 0 },
  { "xori",        0x38000000, 0xFC000000, "%t, %s, %I", ADDR_TYPE_NONE, 0 },
  { "wsbh",        0x7C0000A0, 0xFFE007FF, "%d, %t",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "wsbw",        0x7C0000E0, 0xFFE007FF, "%d, %t",     ADDR_TYPE_NONE, INSTR_TYPE_PSP },

  /* FPU instructions */
  {"abs.s",        0x46000005, 0xFFFF003F, "%D, %S",     ADDR_TYPE_NONE, 0 },
  {"add.s",        0x46000000, 0xFFE0003F, "%D, %S, %T", ADDR_TYPE_NONE, 0 },
  {"bc1f",         0x45000000, 0xFFFF0000, "%O",         ADDR_TYPE_16,   INSTR_TYPE_B },
  {"bc1fl",        0x45020000, 0xFFFF0000, "%O",         ADDR_TYPE_16,   INSTR_TYPE_B },
  {"bc1t",         0x45010000, 0xFFFF0000, "%O",         ADDR_TYPE_16,   INSTR_TYPE_B },
  {"bc1tl",        0x45030000, 0xFFFF0000, "%O",         ADDR_TYPE_16,   INSTR_TYPE_B },
  {"c.f.s",        0x46000030, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.un.s",       0x46000031, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.eq.s",       0x46000032, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.ueq.s",      0x46000033, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.olt.s",      0x46000034, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.ult.s",      0x46000035, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.ole.s",      0x46000036, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.ule.s",      0x46000037, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.sf.s",       0x46000038, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.ngle.s",     0x46000039, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.seq.s",      0x4600003A, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.ngl.s",      0x4600003B, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.lt.s",       0x4600003C, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.nge.s",      0x4600003D, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.le.s",       0x4600003E, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"c.ngt.s",      0x4600003F, 0xFFE007FF, "%S, %T",     ADDR_TYPE_NONE, 0 },
  {"ceil.w.s",     0x4600000E, 0xFFFF003F, "%D, %S",     ADDR_TYPE_NONE, 0 },
  {"cfc1",         0x44400000, 0xFFE007FF, "%t, %p",     ADDR_TYPE_NONE, 0 },
  {"ctc1",         0x44c00000, 0xFFE007FF, "%t, %p",     ADDR_TYPE_NONE, 0 },
  {"cvt.s.w",      0x46800020, 0xFFFF003F, "%D, %S",     ADDR_TYPE_NONE, 0 },
  {"cvt.w.s",      0x46000024, 0xFFFF003F, "%D, %S",     ADDR_TYPE_NONE, 0 },
  {"div.s",        0x46000003, 0xFFE0003F, "%D, %S, %T", ADDR_TYPE_NONE, 0 },
  {"floor.w.s",    0x4600000F, 0xFFFF003F, "%D, %S",     ADDR_TYPE_NONE, 0 },
  {"lwc1",         0xc4000000, 0xFC000000, "%T, %o",     ADDR_TYPE_NONE, 0 },
  {"mfc1",         0x44000000, 0xFFE007FF, "%t, %1",     ADDR_TYPE_NONE, 0 },
  {"mov.s",        0x46000006, 0xFFFF003F, "%D, %S",     ADDR_TYPE_NONE, 0 },
  {"mtc1",         0x44800000, 0xFFE007FF, "%t, %1",     ADDR_TYPE_NONE, 0 },
  {"mul.s",        0x46000002, 0xFFE0003F, "%D, %S, %T", ADDR_TYPE_NONE, 0 },
  {"neg.s",        0x46000007, 0xFFFF003F, "%D, %S",     ADDR_TYPE_NONE, 0 },
  {"round.w.s",    0x4600000C, 0xFFFF003F, "%D, %S",     ADDR_TYPE_NONE, 0 },
  {"sqrt.s",       0x46000004, 0xFFFF003F, "%D, %S",     ADDR_TYPE_NONE, 0 },
  {"sub.s",        0x46000001, 0xFFE0003F, "%D, %S, %T", ADDR_TYPE_NONE, 0 },
  {"swc1",         0xe4000000, 0xFC000000, "%T, %o",     ADDR_TYPE_NONE, 0 },
  {"trunc.w.s",    0x4600000D, 0xFFFF003F, "%D, %S",     ADDR_TYPE_NONE, 0 },

        /* VPU instructions */
  { "bvf",         0x49000000, 0xFFE30000, "%Zc, %O",       ADDR_TYPE_16, INSTR_TYPE_PSP | INSTR_TYPE_B }, /* [hlide] %Z -> %Zc */
  { "bvfl",        0x49020000, 0xFFE30000, "%Zc, %O",       ADDR_TYPE_16, INSTR_TYPE_PSP | INSTR_TYPE_B }, /* [hlide] %Z -> %Zc */
  { "bvt",         0x49010000, 0xFFE30000, "%Zc, %O",       ADDR_TYPE_16, INSTR_TYPE_PSP | INSTR_TYPE_B }, /* [hlide] %Z -> %Zc */
  { "bvtl",        0x49030000, 0xFFE30000, "%Zc, %O",       ADDR_TYPE_16, INSTR_TYPE_PSP | INSTR_TYPE_B }, /* [hlide] %Z -> %Zc */
  { "lv.q",        0xD8000000, 0xFC000002, "%Xq, %Y",       ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "lv.s",        0xC8000000, 0xFC000000, "%Xs, %Y",       ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "lvl.q",       0xD4000000, 0xFC000002, "%Xq, %Y",       ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "lvr.q",       0xD4000002, 0xFC000002, "%Xq, %Y",       ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "mfv",         0x48600000, 0xFFE0FF80, "%t, %zs",       ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%t, %zs" */
  { "mfvc",        0x48600000, 0xFFE0FF00, "%t, %2d",       ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%t, %2d" */
  { "mtv",         0x48E00000, 0xFFE0FF80, "%t, %zs",       ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%t, %zs" */
  { "mtvc",        0x48E00000, 0xFFE0FF00, "%t, %2d",       ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%t, %2d" */
  { "sv.q",        0xF8000000, 0xFC000002, "%Xq, %Y",       ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "sv.s",        0xE8000000, 0xFC000000, "%Xs, %Y",       ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "svl.q",       0xF4000000, 0xFC000002, "%Xq, %Y",       ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "svr.q",       0xF4000002, 0xFC000002, "%Xq, %Y",       ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vabs.p",      0xD0010080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vabs.q",      0xD0018080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vabs.s",      0xD0010000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vabs.t",      0xD0018000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vadd.p",      0x60000080, 0xFF808080, "%zp, %yp, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vadd.q",      0x60008080, 0xFF808080, "%zq, %yq, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vadd.s",      0x60000000, 0xFF808080, "%zs, %ys, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %yz -> %ys */
  { "vadd.t",      0x60008000, 0xFF808080, "%zt, %yt, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vasin.p",     0xD0170080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vasin.q",     0xD0178080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vasin.s",     0xD0170000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vasin.t",     0xD0178000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vavg.p",      0xD0470080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vavg.q",      0xD0478080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vavg.t",      0xD0478000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vbfy1.p",     0xD0420080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vbfy1.q",     0xD0428080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vbfy2.q",     0xD0438080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vcmovf.p",    0xD2A80080, 0xFFF88080, "%zp, %yp, %v3", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zp, %yp, %v3" */
  { "vcmovf.q",    0xD2A88080, 0xFFF88080, "%zq, %yq, %v3", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zq, %yq, %v3" */
  { "vcmovf.s",    0xD2A80000, 0xFFF88080, "%zs, %ys, %v3", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zs, %ys, %v3" */
  { "vcmovf.t",    0xD2A88000, 0xFFF88080, "%zt, %yt, %v3", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zt, %yt, %v3" */
  { "vcmovt.p",    0xD2A00080, 0xFFF88080, "%zp, %yp, %v3", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zp, %yp, %v3" */
  { "vcmovt.q",    0xD2A08080, 0xFFF88080, "%zq, %yq, %v3", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zq, %yq, %v3" */
  { "vcmovt.s",    0xD2A00000, 0xFFF88080, "%zs, %ys, %v3", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zs, %ys, %v3" */
  { "vcmovt.t",    0xD2A08000, 0xFFF88080, "%zt, %yt, %v3", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zt, %yt, %v3" */
  { "vcmp.p",      0x6C000080, 0xFFFFFFF0, "%Zn",           ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%Zn" */
  { "vcmp.p",      0x6C000080, 0xFFFF80F0, "%Zn, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%Zn, %xp" */
  { "vcmp.p",      0x6C000080, 0xFF8080F0, "%Zn, %yp, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%Zn, %zp, %xp" */
  { "vcmp.q",      0x6C008080, 0xFFFFFFF0, "%Zn",           ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%Zn" */
  { "vcmp.q",      0x6C008080, 0xFFFF80F0, "%Zn, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%Zn, %yq" */
  { "vcmp.q",      0x6C008080, 0xFF8080F0, "%Zn, %yq, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%Zn, %yq, %xq" */
  { "vcmp.s",      0x6C000000, 0xFFFFFFF0, "%Zn",           ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%Zn" */
  { "vcmp.s",      0x6C000000, 0xFFFF80F0, "%Zn, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%Zn, %ys" */
  { "vcmp.s",      0x6C000000, 0xFF8080F0, "%Zn, %ys, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%Zn, %ys, %xs" */
  { "vcmp.t",      0x6C008000, 0xFFFFFFF0, "%Zn",           ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zp" */
  { "vcmp.t",      0x6C008000, 0xFFFF80F0, "%Zn, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%Zn, %yt" */
  { "vcmp.t",      0x6C008000, 0xFF8080F0, "%Zn, %yt, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%Zn, %yt, %xt" */
  { "vcos.p",      0xD0130080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vcos.q",      0xD0138080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vcos.s",      0xD0130000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vcos.t",      0xD0138000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vcrs.t",      0x66808000, 0xFF808080, "%zt, %yt, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vcrsp.t",     0xF2808000, 0xFF808080, "%zt, %yt, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vcst.p",      0xD0600080, 0xFFE0FF80, "%zp, %vk",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] "%zp, %yp, %xp" -> "%zp, %vk" */
  { "vcst.q",      0xD0608080, 0xFFE0FF80, "%zq, %vk",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] "%zq, %yq, %xq" -> "%zq, %vk" */
  { "vcst.s",      0xD0600000, 0xFFE0FF80, "%zs, %vk",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] "%zs, %ys, %xs" -> "%zs, %vk" */
  { "vcst.t",      0xD0608000, 0xFFE0FF80, "%zt, %vk",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] "%zt, %yt, %xt" -> "%zt, %vk" */
  { "vdet.p",      0x67000080, 0xFF808080, "%zs, %yp, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vdiv.p",      0x63800080, 0xFF808080, "%zp, %yp, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vdiv.q",      0x63808080, 0xFF808080, "%zq, %yq, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vdiv.s",      0x63800000, 0xFF808080, "%zs, %ys, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %yz -> %ys */
  { "vdiv.t",      0x63808000, 0xFF808080, "%zt, %yt, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vdot.p",      0x64800080, 0xFF808080, "%zs, %yp, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vdot.q",      0x64808080, 0xFF808080, "%zs, %yq, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vdot.t",      0x64808000, 0xFF808080, "%zs, %yt, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vexp2.p",     0xD0140080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vexp2.q",     0xD0148080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vexp2.s",     0xD0140000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vexp2.t",     0xD0148000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vf2h.p",      0xD0320080, 0xFFFF8080, "%zs, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zp -> %zs */
  { "vf2h.q",      0xD0328080, 0xFFFF8080, "%zp, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zq -> %zp */
  { "vf2id.p",     0xD2600080, 0xFFE08080, "%zp, %yp, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zp, %yp, %v5" */
  { "vf2id.q",     0xD2608080, 0xFFE08080, "%zq, %yq, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zq, %yq, %v5" */
  { "vf2id.s",     0xD2600000, 0xFFE08080, "%zs, %ys, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zs, %ys, %v5" */
  { "vf2id.t",     0xD2608000, 0xFFE08080, "%zt, %yt, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zt, %yt, %v5" */
  { "vf2in.p",     0xD2000080, 0xFFE08080, "%zp, %yp, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zp, %yp, %v5" */
  { "vf2in.q",     0xD2008080, 0xFFE08080, "%zq, %yq, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zq, %yq, %v5" */
  { "vf2in.s",     0xD2000000, 0xFFE08080, "%zs, %ys, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zs, %ys, %v5" */
  { "vf2in.t",     0xD2008000, 0xFFE08080, "%zt, %yt, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zt, %yt, %v5" */
  { "vf2iu.p",     0xD2400080, 0xFFE08080, "%zp, %yp, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zp, %yp, %v5" */
  { "vf2iu.q",     0xD2408080, 0xFFE08080, "%zq, %yq, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zq, %yq, %v5" */
  { "vf2iu.s",     0xD2400000, 0xFFE08080, "%zs, %ys, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zs, %ys, %v5" */
  { "vf2iu.t",     0xD2408000, 0xFFE08080, "%zt, %yt, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zt, %yt, %v5" */
  { "vf2iz.p",     0xD2200080, 0xFFE08080, "%zp, %yp, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zp, %yp, %v5" */
  { "vf2iz.q",     0xD2208080, 0xFFE08080, "%zq, %yq, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zq, %yq, %v5" */
  { "vf2iz.s",     0xD2200000, 0xFFE08080, "%zs, %ys, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zs, %ys, %v5" */
  { "vf2iz.t",     0xD2208000, 0xFFE08080, "%zt, %yt, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zt, %yt, %v5" */
  { "vfad.p",      0xD0460080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vfad.q",      0xD0468080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vfad.t",      0xD0468000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vfim.s",      0xDF800000, 0xFF800000, "%xs, %vh",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%xs, %vh" */
  { "vflush",      0xFFFF040D, 0xFFFFFFFF, "",              ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vh2f.p",      0xD0330080, 0xFFFF8080, "%zq, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zp -> %zq */
  { "vh2f.s",      0xD0330000, 0xFFFF8080, "%zp, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zs -> %zp */
  { "vhdp.p",      0x66000080, 0xFF808080, "%zs, %yp, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zs, %yp, %xp" */
  { "vhdp.q",      0x66008080, 0xFF808080, "%zs, %yq, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zs, %yq, %xq" */
  { "vhdp.t",      0x66008000, 0xFF808080, "%zs, %yt, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zs, %yt, %xt" */
  { "vhtfm2.p",    0xF0800000, 0xFF808080, "%zp, %ym, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zp, %ym, %xp" */
  { "vhtfm3.t",    0xF1000080, 0xFF808080, "%zt, %yn, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zt, %yn, %xt" */
  { "vhtfm4.q",    0xF1808000, 0xFF808080, "%zq, %yo, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zq, %yo, %xq" */
  { "vi2c.q",      0xD03D8080, 0xFFFF8080, "%zs, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zs, %yq" */
  { "vi2f.p",      0xD2800080, 0xFFE08080, "%zp, %yp, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zp, %yp, %v5" */
  { "vi2f.q",      0xD2808080, 0xFFE08080, "%zq, %yq, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zq, %yq, %v5" */
  { "vi2f.s",      0xD2800000, 0xFFE08080, "%zs, %ys, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zs, %ys, %v5" */
  { "vi2f.t",      0xD2808000, 0xFFE08080, "%zt, %yt, %v5", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zt, %yt, %v5" */
  { "vi2s.p",      0xD03F0080, 0xFFFF8080, "%zs, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zs, %yp" */
  { "vi2s.q",      0xD03F8080, 0xFFFF8080, "%zp, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zp, %yq" */
  { "vi2uc.q",     0xD03C8080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zp -> %zq */
  { "vi2us.p",     0xD03E0080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zp -> %zq */
  { "vi2us.q",     0xD03E8080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zp -> %zq */
  { "vidt.p",      0xD0030080, 0xFFFFFF80, "%zp",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vidt.q",      0xD0038080, 0xFFFFFF80, "%zq",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "viim.s",      0xDF000000, 0xFF800000, "%xs, %vi",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%xs, %vi" */
  { "vlgb.s",      0xD0370000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vlog2.p",     0xD0150080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vlog2.q",     0xD0158080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vlog2.s",     0xD0150000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vlog2.t",     0xD0158000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmax.p",      0x6D800080, 0xFF808080, "%zp, %yp, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmax.q",      0x6D808080, 0xFF808080, "%zq, %yq, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmax.s",      0x6D800000, 0xFF808080, "%zs, %ys, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmax.t",      0x6D808000, 0xFF808080, "%zt, %yt, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmfvc",       0xD0500000, 0xFFFF0080, "%zs, %2s",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zs, %2s" */
  { "vmidt.p",     0xF3830080, 0xFFFFFF80, "%zm",           ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zp -> %zm */
  { "vmidt.q",     0xF3838080, 0xFFFFFF80, "%zo",           ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zq -> %zo */
  { "vmidt.t",     0xF3838000, 0xFFFFFF80, "%zn",           ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zt -> %zn */
  { "vmin.p",      0x6D000080, 0xFF808080, "%zp, %yp, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmin.q",      0x6D008080, 0xFF808080, "%zq, %yq, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmin.s",      0x6D000000, 0xFF808080, "%zs, %ys, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmin.t",      0x6D008000, 0xFF808080, "%zt, %yt, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmmov.p",     0xF3800080, 0xFFFF8080, "%zm, %ym",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zm, %ym" */
  { "vmmov.q",     0xF3808080, 0xFFFF8080, "%zo, %yo",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmmov.t",     0xF3808000, 0xFFFF8080, "%zn, %yn",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zn, %yn" */
  { "vmmul.p",     0xF0000080, 0xFF808080, "%?%zm, %ym, %xm", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%?%zm, %ym, %xm" */
  { "vmmul.q",     0xF0008080, 0xFF808080, "%?%zo, %yo, %xo", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmmul.t",     0xF0008000, 0xFF808080, "%?%zn, %yn, %xn", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%?%zn, %yn, %xn" */
  { "vmone.p",     0xF3870080, 0xFFFFFF80, "%zp",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmone.q",     0xF3878080, 0xFFFFFF80, "%zq",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmone.t",     0xF3878000, 0xFFFFFF80, "%zt",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmov.p",      0xD0000080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmov.q",      0xD0008080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmov.s",      0xD0000000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmov.t",      0xD0008000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmscl.p",     0xF2000080, 0xFF808080, "%zm, %ym, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zp, %yp, %xp -> %zm, %ym, %xs */
  { "vmscl.q",     0xF2008080, 0xFF808080, "%zo, %yo, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zq, %yq, %xp -> %zo, %yo, %xs */
  { "vmscl.t",     0xF2008000, 0xFF808080, "%zn, %yn, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zt, %yt, %xp -> %zn, %yn, %xs */
  { "vmtvc",       0xD0510000, 0xFFFF8000, "%2d, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%2d, %ys" */
  { "vmul.p",      0x64000080, 0xFF808080, "%zp, %yp, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmul.q",      0x64008080, 0xFF808080, "%zq, %yq, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmul.s",      0x64000000, 0xFF808080, "%zs, %ys, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmul.t",      0x64008000, 0xFF808080, "%zt, %yt, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vmzero.p",    0xF3860080, 0xFFFFFF80, "%zm",           ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zp -> %zm */
  { "vmzero.q",    0xF3868080, 0xFFFFFF80, "%zo",           ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zq -> %zo */
  { "vmzero.t",    0xF3868000, 0xFFFFFF80, "%zn",           ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zt -> %zn */
  { "vneg.p",      0xD0020080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vneg.q",      0xD0028080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vneg.s",      0xD0020000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vneg.t",      0xD0028000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vnop",        0xFFFF0000, 0xFFFFFFFF, "",              ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vnrcp.p",     0xD0180080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vnrcp.q",     0xD0188080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vnrcp.s",     0xD0180000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vnrcp.t",     0xD0188000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vnsin.p",     0xD01A0080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vnsin.q",     0xD01A8080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vnsin.s",     0xD01A0000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vnsin.t",     0xD01A8000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vocp.p",      0xD0440080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vocp.q",      0xD0448080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vocp.s",      0xD0440000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vocp.t",      0xD0448000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vone.p",      0xD0070080, 0xFFFFFF80, "%zp",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vone.q",      0xD0078080, 0xFFFFFF80, "%zq",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vone.s",      0xD0070000, 0xFFFFFF80, "%zs",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vone.t",      0xD0078000, 0xFFFFFF80, "%zt",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vpfxd",       0xDE000000, 0xFF000000, "[%vp4, %vp5, %vp6, %vp7]", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "[%vp4, %vp5, %vp6, %vp7]" */
  { "vpfxs",       0xDC000000, 0xFF000000, "[%vp0, %vp1, %vp2, %vp3]", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "[%vp0, %vp1, %vp2, %vp3]" */
  { "vpfxt",       0xDD000000, 0xFF000000, "[%vp0, %vp1, %vp2, %vp3]", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "[%vp0, %vp1, %vp2, %vp3]" */
  { "vqmul.q",     0xF2808080, 0xFF808080, "%zq, %yq, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zq, %yq, %xq" */
  { "vrcp.p",      0xD0100080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrcp.q",      0xD0108080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrcp.s",      0xD0100000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrcp.t",      0xD0108000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrexp2.p",    0xD01C0080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrexp2.q",    0xD01C8080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrexp2.s",    0xD01C0000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrexp2.t",    0xD01C8000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrndf1.p",    0xD0220080, 0xFFFFFF80, "%zp",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrndf1.q",    0xD0228080, 0xFFFFFF80, "%zq",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrndf1.s",    0xD0220000, 0xFFFFFF80, "%zs",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrndf1.t",    0xD0228000, 0xFFFFFF80, "%zt",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrndf2.p",    0xD0230080, 0xFFFFFF80, "%zp",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrndf2.q",    0xD0238080, 0xFFFFFF80, "%zq",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrndf2.s",    0xD0230000, 0xFFFFFF80, "%zs",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrndf2.t",    0xD0238000, 0xFFFFFF80, "%zt",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrndi.p",     0xD0210080, 0xFFFFFF80, "%zp",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrndi.q",     0xD0218080, 0xFFFFFF80, "%zq",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrndi.s",     0xD0210000, 0xFFFFFF80, "%zs",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrndi.t",     0xD0218000, 0xFFFFFF80, "%zt",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrnds.s",     0xD0200000, 0xFFFF80FF, "%ys",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrot.p",      0xF3A00080, 0xFFE08080, "%zp, %ys, %vr", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zp, %ys, %vr" */
  { "vrot.q",      0xF3A08080, 0xFFE08080, "%zq, %ys, %vr", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zq, %ys, %vr" */
  { "vrot.t",      0xF3A08000, 0xFFE08080, "%zt, %ys, %vr", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zt, %ys, %vr" */
  { "vrsq.p",      0xD0110080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrsq.q",      0xD0118080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrsq.s",      0xD0110000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vrsq.t",      0xD0118000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vs2i.p",      0xD03B0080, 0xFFFF8080, "%zq, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zp -> %zq */
  { "vs2i.s",      0xD03B0000, 0xFFFF8080, "%zp, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zs -> %zp */
  { "vsat0.p",     0xD0040080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsat0.q",     0xD0048080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsat0.s",     0xD0040000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsat0.t",     0xD0048000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsat1.p",     0xD0050080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsat1.q",     0xD0058080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsat1.s",     0xD0050000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsat1.t",     0xD0058000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsbn.s",      0x61000000, 0xFF808080, "%zs, %ys, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsbz.s",      0xD0360000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vscl.p",      0x65000080, 0xFF808080, "%zp, %yp, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %xp -> %xs */
  { "vscl.q",      0x65008080, 0xFF808080, "%zq, %yq, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %xq -> %xs */
  { "vscl.t",      0x65008000, 0xFF808080, "%zt, %yt, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %xt -> %xs */
  { "vscmp.p",     0x6E800080, 0xFF808080, "%zp, %yp, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vscmp.q",     0x6E808080, 0xFF808080, "%zq, %yq, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vscmp.s",     0x6E800000, 0xFF808080, "%zs, %ys, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vscmp.t",     0x6E808000, 0xFF808080, "%zt, %yt, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsge.p",      0x6F000080, 0xFF808080, "%zp, %yp, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsge.q",      0x6F008080, 0xFF808080, "%zq, %yq, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsge.s",      0x6F000000, 0xFF808080, "%zs, %ys, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsge.t",      0x6F008000, 0xFF808080, "%zt, %yt, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsgn.p",      0xD04A0080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsgn.q",      0xD04A8080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsgn.s",      0xD04A0000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsgn.t",      0xD04A8000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsin.p",      0xD0120080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsin.q",      0xD0128080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsin.s",      0xD0120000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsin.t",      0xD0128000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vslt.p",      0x6F800080, 0xFF808080, "%zp, %yp, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vslt.q",      0x6F808080, 0xFF808080, "%zq, %yq, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vslt.s",      0x6F800000, 0xFF808080, "%zs, %ys, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vslt.t",      0x6F808000, 0xFF808080, "%zt, %yt, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsocp.p",     0xD0450080, 0xFFFF8080, "%zq, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zp -> %zq */
  { "vsocp.s",     0xD0450000, 0xFFFF8080, "%zp, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zs -> %zp */
  { "vsqrt.p",     0xD0160080, 0xFFFF8080, "%zp, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsqrt.q",     0xD0168080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsqrt.s",     0xD0160000, 0xFFFF8080, "%zs, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsqrt.t",     0xD0168000, 0xFFFF8080, "%zt, %yt",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsrt1.q",     0xD0408080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsrt2.q",     0xD0418080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsrt3.q",     0xD0488080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsrt4.q",     0xD0498080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsub.p",      0x60800080, 0xFF808080, "%zp, %yp, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsub.q",      0x60808080, 0xFF808080, "%zq, %yq, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsub.s",      0x60800000, 0xFF808080, "%zs, %ys, %xs", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsub.t",      0x60808000, 0xFF808080, "%zt, %yt, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsync",       0xFFFF0320, 0xFFFFFFFF, "",              ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vsync",       0xFFFF0000, 0xFFFF0000, "%I",            ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vt4444.q",    0xD0598080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zq -> %zp */
  { "vt5551.q",    0xD05A8080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zq -> %zp */
  { "vt5650.q",    0xD05B8080, 0xFFFF8080, "%zq, %yq",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] %zq -> %zp */
  { "vtfm2.p",     0xF0800080, 0xFF808080, "%zp, %ym, %xp", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zp, %ym, %xp" */
  { "vtfm3.t",     0xF1008000, 0xFF808080, "%zt, %yn, %xt", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zt, %yn, %xt" */
  { "vtfm4.q",     0xF1808080, 0xFF808080, "%zq, %yo, %xq", ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zq, %yo, %xq" */
  { "vus2i.p",     0xD03A0080, 0xFFFF8080, "%zq, %yp",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zq, %yp" */
  { "vus2i.s",     0xD03A0000, 0xFFFF8080, "%zp, %ys",      ADDR_TYPE_NONE, INSTR_TYPE_PSP }, /* [hlide] added "%zp, %ys" */
  { "vwb.q",       0xF8000002, 0xFC000002, "%Xq, %Y",       ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vwbn.s",      0xD3000000, 0xFF008080, "%zs, %xs, %I",  ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vzero.p",     0xD0060080, 0xFFFFFF80, "%zp",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vzero.q",     0xD0068080, 0xFFFFFF80, "%zq",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vzero.s",     0xD0060000, 0xFFFFFF80, "%zs",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "vzero.t",     0xD0068000, 0xFFFFFF80, "%zt",           ADDR_TYPE_NONE, INSTR_TYPE_PSP },
  { "mfvme",       0x68000000, 0xFC000000, "%t, %i",        ADDR_TYPE_NONE, 0 },
  { "mtvme",       0xb0000000, 0xFC000000, "%t, %i",        ADDR_TYPE_NONE, 0 },
};

static char buffer[1024];
static const char *gpr_names[] =
{
  "zr", "at", "v0", "v1", "a0", "a1", "a2", "a3",
  "t0", "t1", "t2", "t3", "t4", "t5", "t6", "t7",
  "s0", "s1", "s2", "s3", "s4", "s5", "s6", "s7",
  "t8", "t9", "k0", "k1", "gp", "sp", "fp", "ra"
};

static const char *cop0_regs[] =
{
  "COP0_0", "COP0_1", "COP0_2", "COP0_3", "COP0_4", "COP0_5", "COP0_6", "COP0_7",
  "BadVaddr", "Count", "COP0_10", "Compare", "Status", "Cause", "EPC", "PrID",
  "Config", "COP0_17", "COP0_18", "COP0_19", "COP0_20", "COP0_21", "COP0_22", "COP0_23",
  "COP0_24", "EBase", "COP0_26", "COP0_27", "TagLo", "TagHi", "ErrorPC", "COP0_31"
};


static const char *debug_regs[] =
{
  "DRCNTL", "DEPC", "DDATA0", "DDATA1", "IBC", "DBC", "Debug06", "Debug07",
  "IBA", "IBAM", "Debug10", "Debug11", "DBA", "DBAM", "DBD", "DBDM",
  "Debug16", "Debug17", "Debug18", "Debug19", "Debug20", "Debug21", "Debug22", "Debug23",
  "Debug24", "Debug25", "Debug26", "Debug27", "Debug28", "Debug29", "Debug30", "Debug31"
};

static const char *vfpu_cond_names[] =
{
  "FL",  "EQ",  "LT",  "LE",
  "TR",  "NE",  "GE",  "GT",
  "EZ",  "EN",  "EI",  "ES",
  "NZ",  "NN",  "NI",  "NS"
};

static const char *vfpu_constants[] = {
  "VFPU_CONST0",
  "VFPU_HUGE",
  "VFPU_SQRT2",
  "VFPU_SQRT1_2",
  "VFPU_2_SQRTPI",
  "VFPU_2_PI",
  "VFPU_1_PI",
  "VFPU_PI_4",
  "VFPU_PI_2",
  "VFPU_PI",
  "VFPU_E",
  "VFPU_LOG2E",
  "VFPU_LOG10E",
  "VFPU_LN2",
  "VFPU_LN10",
  "VFPU_2PI",
  "VFPU_PI_6",
  "VFPU_LOG10TWO",
  "VFPU_LOG2TEN",
  "VFPU_SQRT3_2",
  "VFPU_CONST20",
  "VFPU_CONST21",
  "VFPU_CONST22",
  "VFPU_CONST23",
  "VFPU_CONST24",
  "VFPU_CONST25",
  "VFPU_CONST26",
  "VFPU_CONST27",
  "VFPU_CONST28",
  "VFPU_CONST29",
  "VFPU_CONST30",
  "VFPU_CONST31",
};

static const char *pfx_cst_names[] =
{
  "0",  "1",  "2",  "1/2",  "3",  "1/3",  "1/4",  "1/6"
};

static const char *pfx_swz_names[] =
{
  "x",  "y",  "z",  "w"
};

static const char *pfx_sat_names[] =
{
  "",  "[0:1]",  "",  "[-1:1]"
};

/* [hlide] added vfpu_extra_regs */
static const char *vfpu_extra_regs[] =
{
  "VFPU_PFXS",
  "VFPU_PFXT",
  "VFPU_PFXD",
  "VFPU_CC ",
  "VFPU_INF4",
  NULL,
  NULL,
  "VFPU_REV",
  "VFPU_RCX0",
  "VFPU_RCX1",
  "VFPU_RCX2",
  "VFPU_RCX3",
  "VFPU_RCX4",
  "VFPU_RCX5",
  "VFPU_RCX6",
  "VFPU_RCX7"
};

static
int print_vfpu_single (int reg, char *output)
{
  return sprintf (output, "S%d%d%d", (reg >> 2) & 7, reg & 3, (reg >> 5) & 3);
}

static
int print_vfpu_reg (int reg, int offset, char one, char two, char *output)
{
  if ((reg >> 5) & 1) {
    return sprintf (output, "%c%d%d%d", two, (reg >> 2) & 7, offset, reg & 3);
  } else {
    return sprintf (output, "%c%d%d%d", one, (reg >> 2) & 7, reg & 3, offset);
  }
}

static
int print_vfpu_quad (int reg, char *output)
{
  return print_vfpu_reg (reg, 0, 'C', 'R', output);
}

static
int print_vfpu_pair (int reg, char *output)
{
  if ((reg >> 6) & 1) {
    return print_vfpu_reg (reg, 2, 'C', 'R', output);
  } else {
    return print_vfpu_reg (reg, 0, 'C', 'R', output);
  }
}

static
int print_vfpu_triple (int reg, char *output)
{
  if ((reg >> 6) & 1) {
    return print_vfpu_reg (reg, 1, 'C', 'R', output);
  } else {
    return print_vfpu_reg (reg, 0, 'C', 'R', output);
  }
}

static
int print_vfpu_mpair (int reg, char *output)
{
  if ((reg >> 6) & 1) {
    return print_vfpu_reg (reg, 2, 'M', 'E', output);
  } else {
    return print_vfpu_reg (reg, 0, 'M', 'E', output);
  }
}

static
int print_vfpu_mtriple (int reg, char *output)
{
  if ((reg >> 6) & 1) {
    return print_vfpu_reg (reg, 1, 'M', 'E', output);
  } else {
    return print_vfpu_reg (reg, 0, 'M', 'E', output);
  }
}

static
int print_vfpu_matrix (int reg, char *output)
{
  return print_vfpu_reg (reg, 0, 'M', 'E', output);
}

static
int print_vfpu_register (int reg, char type, char *output)
{
  switch (type) {
  case 's': return print_vfpu_single (reg, output);
  case 'q': return print_vfpu_quad (reg, output);
  case 'p': return print_vfpu_pair (reg, output);
  case 't': return print_vfpu_triple (reg, output);
  case 'm': return print_vfpu_mpair (reg, output);
  case 'n': return print_vfpu_mtriple (reg, output);
  case 'o': return print_vfpu_matrix (reg, output);
  };

  return 0;
}

static
int print_vfpu_halffloat (int l, char *output)
{
  unsigned short float16 = l & 0xFFFF;
  unsigned int sign = (float16 >> VFPU_SH_FLOAT16_SIGN) & VFPU_MASK_FLOAT16_SIGN;
  int exponent = (float16 >> VFPU_SH_FLOAT16_EXP) & VFPU_MASK_FLOAT16_EXP;
  unsigned int fraction = float16 & VFPU_MASK_FLOAT16_FRAC;
  char signchar = '+' + ((sign == 1) * 2);
  int len;
  /* Convert a VFPU 16-bit floating-point number to IEEE754. */
  union float2int
  {
    unsigned int i;
    float f;
  } float2int;

  if (exponent == VFPU_FLOAT16_EXP_MAX) {
    if (fraction == 0)
      len = sprintf (output, "%cInf", signchar);
    else
      len = sprintf (output, "%cNaN", signchar);
  } else if (exponent == 0 && fraction == 0) {
    len = sprintf (output, "%c0", signchar);
  } else {
    if (exponent == 0) {
      do {
        fraction <<= 1;
        exponent--;
      } while (!(fraction & (VFPU_MASK_FLOAT16_FRAC + 1)));

      fraction &= VFPU_MASK_FLOAT16_FRAC;
    }

    /* Convert to 32-bit single-precision IEEE754. */
    float2int.i = sign << 31;
    float2int.i |= (exponent + 112) << 23;
    float2int.i |= fraction << 13;
    len = sprintf (output, "%g", float2int.f);
  }

  return len;
}

/* [hlide] added print_vfpu_prefix */
static
int print_vfpu_prefix (int l, unsigned int pos, char *output)
{
  int len = 0;

  switch (pos)
  {
  case '0':
  case '1':
  case '2':
  case '3':
    {
      unsigned int base = '0';
      unsigned int negation = (l >> (pos - (base - VFPU_SH_PFX_NEG))) & VFPU_MASK_PFX_NEG;
      unsigned int constant = (l >> (pos - (base - VFPU_SH_PFX_CST))) & VFPU_MASK_PFX_CST;
      unsigned int abs_consthi = (l >> (pos - (base - VFPU_SH_PFX_ABS_CSTHI))) & VFPU_MASK_PFX_ABS_CSTHI;
      unsigned int swz_constlo = (l >> ((pos - base) * 2)) & VFPU_MASK_PFX_SWZ_CSTLO;

      if (negation)
        len = sprintf (output, "-");
      if (constant) {
        len += sprintf (output + len, "%s", pfx_cst_names[(abs_consthi << 2) | swz_constlo]);
      } else {
        if (abs_consthi)
          len += sprintf (output + len, "|%s|", pfx_swz_names[swz_constlo]);
        else
          len += sprintf (output + len, "%s", pfx_swz_names[swz_constlo]);
      }
    }
    break;

  case '4':
  case '5':
  case '6':
  case '7':
    {
      unsigned int base = '4';
      unsigned int mask = (l >> (pos - (base - VFPU_SH_PFX_MASK))) & VFPU_MASK_PFX_MASK;
      unsigned int saturation = (l >> ((pos - base) * 2)) & VFPU_MASK_PFX_SAT;

      if (mask)
        len += sprintf (output, "m");
      else
        len += sprintf (output, "%s", pfx_sat_names[saturation]);
    }
    break;
  }

  return len;
}


static
int print_vfpu_rotator (int l, char *output)
{
  int len;

  const char *elements[4];

  unsigned int opcode = l & VFPU_MASK_OP_SIZE;
  unsigned int rotators = (l >> 16) & 0x1f;
  unsigned int opsize, rothi, rotlo, negation, i;

  /* Determine the operand size so we'll know how many elements to output. */
  if (opcode == VFPU_OP_SIZE_PAIR)
    opsize = 2;
  else if (opcode == VFPU_OP_SIZE_TRIPLE)
    opsize = 3;
  else
    opsize = (opcode == VFPU_OP_SIZE_QUAD) * 4;     /* Sanity check. */

  rothi = (rotators >> VFPU_SH_ROT_HI) & VFPU_MASK_ROT_HI;
  rotlo = (rotators >> VFPU_SH_ROT_LO) & VFPU_MASK_ROT_LO;
  negation = (rotators >> VFPU_SH_ROT_NEG) & VFPU_MASK_ROT_NEG;

  if (rothi == rotlo) {
    if (negation) {
      elements[0] = "-s";
      elements[1] = "-s";
      elements[2] = "-s";
      elements[3] = "-s";
    } else {
      elements[0] = "s";
      elements[1] = "s";
      elements[2] = "s";
      elements[3] = "s";
    }
  } else {
    elements[0] = "0";
    elements[1] = "0";
    elements[2] = "0";
    elements[3] = "0";
  }
  if (negation)
    elements[rothi] = "-s";
  else
    elements[rothi] = "s";
  elements[rotlo] = "c";

  len = sprintf (output, "[");
  for (i = 0;;) {
    len += sprintf (output + len, "%s", elements[i++]);
    if (i >= opsize)
      break;
    sprintf (output + len, " ,");
  }

  len += sprintf (output + len, "]");

  return len;
}

/* [hlide] added print_cop2 */
static
int print_cop2 (int reg, char *output)
{
  int len;

  if ((reg >= 128) && (reg < 128+16) && (vfpu_extra_regs[reg - 128])) {
    len = sprintf (output, "%s", vfpu_extra_regs[reg - 128]);
  } else {
    len = sprintf (output, "VFPU_COP2_%d", reg);
  }

  return len;
}


static
void decode_instruction (struct Instruction *insn, unsigned int opcode, unsigned int PC)
{
  int i = 0, len = 0, vmmul = 0;
  unsigned int data = opcode;
  len += sprintf (buffer, "\t0x%08X: 0x%08X '", PC, opcode);
  for (i = 0; i < 4; i++) {
    char c = (char) (data & 0xFF);
    if (isprint (c)) { len += sprintf (&buffer[len], "%c", c); }
    else { len += sprintf (&buffer[len], "."); }
    data >>= 8;
  }
  sprintf (&buffer[len], "' - %s                  ", insn->name);
  len = 44;

  i = 0;
  while (1) {
    char c = insn->fmt[i++];
    if (c == '\0') break;
    if (c == '%') {
      c = insn->fmt[i++];
      switch (c) {
      case 'd':
        len += sprintf (&buffer[len], "$%s", gpr_names[RD (opcode)]);
        break;
      case 'D':
        len += sprintf (&buffer[len], "$fpr%02d", FD (opcode));
        break;
      case 't':
        len += sprintf (&buffer[len], "$%s", gpr_names[RT (opcode)]);
        break;
      case 'T':
        len += sprintf (&buffer[len], "$fpr%02d", FT (opcode));
        break;
      case 's':
        len += sprintf (&buffer[len], "$%s", gpr_names[RS (opcode)]);
        break;
      case 'S':
        len += sprintf (&buffer[len], "$fpr%02d", FS (opcode));
        break;
      case 'J':
        len += sprintf (&buffer[len], "$%s", gpr_names[RS (opcode)]);
        break;
      case 'i':
        len += sprintf (&buffer[len], "%d", IMM (opcode));
        break;
      case 'I':
        len += sprintf (&buffer[len], "0x%04X", IMMU (opcode));
        break;
      case 'j':
        len += sprintf (&buffer[len], "0x%08X", JUMP (opcode, PC));
        break;
      case 'O':
        len += sprintf (&buffer[len], "0x%08X", PC + 4 + 4 * ((int) IMM (opcode)));
        break;
      case 'o':
        len += sprintf (&buffer[len], "%d($%s)", IMM (opcode), gpr_names[RS (opcode)]);
        break;
      case 'c':
        len += sprintf (&buffer[len], "0x%05X", CODE (opcode));
        break;
      case 'C':
        len += sprintf (&buffer[len], "0x%05X", CODE (opcode));
        break;
      case 'k':
        len += sprintf (&buffer[len], "0x%X", RT (opcode));
        break;
      case 'p':
        len += sprintf (&buffer[len], "$%d", RD (opcode));
        break;
      case 'a':
        len += sprintf (&buffer[len], "%d", SA (opcode));
        break;
      case 'r':
        len += sprintf (&buffer[len], "%s", debug_regs[RD (opcode)]);
        break;
      case '0':
        len += sprintf (&buffer[len], "%s", cop0_regs[RD (opcode)]);
        break;
      case '1':
        len += sprintf (&buffer[len], "$fcr%d", RD (opcode));
        break;
      case '2':
        c = insn->fmt[i++];
        if (c == 'd') {
          len += print_cop2 (VED (opcode), &buffer[len]);
        } else { /* 's'*/
          len += print_cop2 (VES (opcode), &buffer[len]);
        }
        break;
      case 'z':
        c = insn->fmt[i++];
        len += print_vfpu_register (VD (opcode), c, &buffer[len]);
        break;
      case 'Z':
        c = insn->fmt[i++];
        if (c == 'c') {
          len += sprintf (&buffer[len], "%d", VCC (opcode));
        } else { /* 'n' */
          len += sprintf (&buffer[len], "%s", vfpu_cond_names[VCN (opcode)]);
        }
        break;
      case 'n':
        c = insn->fmt[i++];
        if (c == 'e') {
          len += sprintf (&buffer[len], "%d", RD (opcode) + 1);
        } else { /* 'i' */
          len += sprintf (&buffer[len], "%d", RD (opcode) - SA (opcode) + 1);
        }
        break;
      case 'X':
        c = insn->fmt[i++];
        len += print_vfpu_register (VO (opcode), c, &buffer[len]);
        break;
      case 'x':
        c = insn->fmt[i++];
        len += print_vfpu_register (VT (opcode), c, &buffer[len]);
        break;
      case 'Y':
        len += sprintf (&buffer[len], "%d($%s)", IMM (opcode) & ~3, gpr_names[RS (opcode)]);
        break;
      case 'y':
        {
          int reg = VS (opcode);
          if (vmmul) { if (reg & 0x20) { reg &= 0x5F; } else { reg |= 0x20; } }
          c = insn->fmt[i++];
          len += print_vfpu_register (reg, c, &buffer[len]);
        }
        break;
      case 'v':
        c = insn->fmt[i++];
        switch (c) {
        case '3' : len += sprintf (&buffer[len], "%d", VI3 (opcode)); break;
        case '5' : len += sprintf (&buffer[len], "%d", VI5 (opcode)); break;
        case '8' : len += sprintf (&buffer[len], "%d", VI8 (opcode)); break;
        case 'k' : len += sprintf (&buffer[len], "%s", vfpu_constants [VI5 (opcode)]); break;
        case 'i' : len += sprintf (&buffer[len], "%d", IMM (opcode)); break;
        case 'h' : len += print_vfpu_halffloat (opcode, &buffer[len]); break;
        case 'r' : len += print_vfpu_rotator (opcode, &buffer[len]); break;
        case 'p' : c = insn->fmt[i++]; len += print_vfpu_prefix (opcode, c, &buffer[len]); break;
        }
        break;
      case '?':
        vmmul = 1;
        break;
      }
    } else {
      len += sprintf (&buffer[len], "%c", c);
    }
  }
  buffer[len] = '\0';
}

char *allegrex_disassemble (unsigned int opcode, unsigned int PC)
{
  int i;

  for (i = 0; i < sizeof (g_inst) / sizeof (struct Instruction); i++) {
    if ((g_inst[i].mask & opcode) == g_inst[i].opcode) {
      decode_instruction (&g_inst[i], opcode, PC);
      break;
    }
  }
  return (char *) buffer;
}

#ifdef TEST_DISASSEMBLE

#include <stdlib.h>
int main (int argc, char **argv)
{
  int i;
  for (i = 0; i < sizeof (g_inst) / sizeof (struct Instruction); i++) {
    unsigned int opcode = rand ();
    opcode = (opcode & (~g_inst[i].mask)) | g_inst[i].opcode;
    printf ("%s\n", disassemble (opcode, 4 * i));
  }
  return 0;
}

#endif /* TEST_DISASSEMBLE */

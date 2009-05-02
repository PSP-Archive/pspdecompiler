        .section .text

        .set    noreorder
        .set    noat


        .global cfg_branches
        .ent    cfg_branches

cfg_branches:
        nop
cfg_branches_start:
        bne     $a0, $0, 1f
        nop

        bgez    $a0, 2f
        nop

1:
        bgtz    $a0, 3f
        nop

2:
        blez    $a0, 4f
        nop

3:
        bltz    $a0, 5f
        nop

4:
        beq     $a0, $at, 6f
        nop

5:
        bne     $a0, $at, 7f
        nop

6:
        j       cfg_branches_start
        nop

7:
        jr      $ra
        nop

        .end    cfg_branches


        .global cfg_branches_likely
        .ent    cfg_branches_likely

cfg_branches_likely:
        beql    $a0, $a1, 1f
        nop

        bgezl   $a1, 1f
        nop

        bgtzl   $a2, 1f
        nop

        blezl   $a3, 1f
        nop

        bltzl   $a0, 1f
        nop

        bnel    $a0, $a2, 1f
        nop

        nop
1:

        jr      $ra
        nop

        .end    cfg_branches_likely


        .global cfg_branches_noswap
        .ent    cfg_branches_noswap

cfg_branches_noswap:
        beq     $a0, $a1, 1f
        move    $a1, $0
        lui     $v0, 0
1:
        bltzal  $a0, cfg_branches_noswap
2:      move    $a1, $31
        bgez    $a0, 2b
        nop

        bltzal  $a0, cfg_branches_noswap
        move    $a1, $31

        bltzall $a0, cfg_branches_noswap
3:      nop
        bgez    $a0, 3b
        nop

        jr      $ra
        nop

        .end    cfg_branches_noswap



        .global cfg_branches_never
        .ent    cfg_branches_never

cfg_branches_never:
        bgtz    $0, 1f
        nop

        bgtzl   $0, 1f
        nop

        bltz    $0, 1f
        nop

        bltzl   $0, 1f
        nop

        bne     $0, $0, 1f
        nop

        bne     $a0, $a0, 1f
        nop

        bnel    $0, $0, 1f
        nop

        bnel    $a0, $a0, 1f
        nop

        bltzal  $0, cfg_branches_never
        nop

        bltzall  $0, cfg_branches_never
        nop

1:
        jr      $ra
        nop

        .end    cfg_branches_never

        .global cfg_branches_always
        .ent    cfg_branches_always

cfg_branches_always:

        bgez    $0, 7f
        nop

1:
        bgezl   $0, 8f
        nop

2:
        blez    $0, 1b
        nop

3:
        blezl   $0, 2b
        nop

4:
        beq     $0, $0, 3b
        nop

5:
        beq     $a0, $a0, 4b
        nop

6:
        beql    $0, $0, 5b
        nop

7:
        beql    $a0, $a0, 6b
        nop

8:
        jr      $ra
        nop

        .end    cfg_branches_always

        .global cfg_branchlink
        .ent    cfg_branchlink

cfg_branchlink:
        bgezal  $a0, cfg_jumptobegin
        nop
        bltzal  $a0, cfg_jumptobegin
        nop
        bltzall $a0, cfg_jumptobegin
        nop

        jr      $ra
        nop

        .end    cfg_branchlink

        .global cfg_strangerefs
        .ent    cfg_strangerefs

cfg_strangerefs:
        beql    $a0, $a1, 2f
1:
        nop
        beq     $a0, $a1, 2f
        nop
        j       1b
        nop

2:
        bltzall $a0, cfg_jumptobegin
3:
        nop
        beq     $a0, $a1, 3b
        nop

        jr      $ra
        nop

        .end    cfg_strangerefs

        .global cfg_jumptobegin
        .ent    cfg_jumptobegin

cfg_jumptobegin:
        nop
        beq     $a0, $a1, cfg_jumptobegin
        nop

        jr      $ra
        nop

        .end    cfg_jumptobegin

        .global cfg_callnoswap
        .ent    cfg_callnoswap

cfg_callnoswap:
        jal     cfg_callnoswap
        move    $a0, $31

        jr      $ra
        nop

        .end    cfg_callnoswap


        .global cfg_switch
        .ent    cfg_switch

cfg_switch:
        jal     cfg_switch_innercallback
        nop
        slti    $t0, $a1, 7
        sll     $a0, $a1, 2
        blez    $t0, switch1_default
        lui     $v0, %hi(switch1)
        addu    $v0, $a0
        lw      $v0, %lo(switch1)($v0)
        jr      $v0
        nop
switch1_case0:
switch1_case1:
switch1_case6:
switch1_case3:
        jr      $ra
        nop

switch1_case4:
switch1_case2:
        j       switch1_case0
        move    $a0, $v0


switch1_case5:
        jal     cfg_branches_always
        nop

switch1_default:

        jr      $ra
        nop

cfg_switch_innercallback:
        jal     mycallback
        nop
        lui     $v0, %hi(inner_callback)
        lw      $v0, %lo(inner_callback)($v0)
        jr      $v0
        nop

mycallback:
        jr      $ra
        nop

        .end    cfg_switch


/*
int double_break (void)
{
  int i, j = 400009, t = 0;
  for (i = 0; i < 100; i++) {
    if (j & 1) {
      j = 3 * j + 1;
    } else {
      j >>= 1;
    }

    if (j == 50) {
      j++;
      if (i > 10) break;
    } else {
      t++;
      if (i == 98) break;
    }
    t += 2;
  }
  return t;
}
*/
        .global graph_double_break
        .ent    graph_double_break
graph_double_break:
        move    $4,$0
        li      $2,393216
        ori     $3,$2,0x1a89
        move    $5,$0
        li      $9,3
        li      $7,50
        li      $8,98
        li      $10,51
        li      $6,100
1:
        andi    $2,$3,0x1
        beql    $2,$0,2f
        sra     $3,$3,1

        mult    $3,$9
        mflo    $2
        addiu   $3,$2,1
2:
        bne     $3,$7,3f
        slt     $2,$4,11

        beq     $2,$0,5f
        move    $3,$10

        j       4f
        addiu   $4,$4,1

3:
        beq     $4,$8,5f
        addiu   $5,$5,1

        addiu   $4,$4,1
4:
        bne     $4,$6,1b
        addiu   $5,$5,2

5:
        j       $ra
        move    $2,$5

        .end    graph_double_break

        .section .rodata
switch1:
        .word   switch1_case0
        .word   switch1_case1
        .word   switch1_case2
        .word   switch1_case3
        .word   switch1_case4
        .word   switch1_case5
        .word   switch1_case6

inner_callback:
        .word   mycallback
        .word   mycallback


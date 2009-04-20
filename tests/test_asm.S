        .section .text

        .set    noreorder
        .set    noat


        .global testcfg_branches
        .ent    testcfg_branches

testcfg_branches:
        nop
opa:
        bne     $a0, $0, 0f
        nop

        bgez    $0, 1f        /* Always */
        nop
0:
        bgez    $a0, 1f
        nop

        bgezl   $0, 2f        /* Always */
        nop

1:
        bgezl   $a0, 2f
        nop

        bgtz    $a0, 2f
        nop

        bgtzl   $a0, 2f
        nop

        blez    $0, 3f        /* Always */
        nop
2:
        blez    $a0, 3f
        nop

        blezl   $0, 4f        /* Always */
        nop
3:
        blezl   $a0, 4f
        nop

        bltz    $a0, 4f
        nop

        bltzl   $a0, 4f
        nop


        beq     $0, $0, 5f   /* Always */
        nop

4:

        beq     $a0, $at, 5f
        nop

        beq     $a0, $a0, 6f /* Always */
        nop

5:
        beql    $0, $0, 7f  /* Always */
        nop

6:
        beql    $a0, $at, 7f
        nop

        beql    $a0, $a0, 8f /* Always */
        nop

7:

        bne     $a0, $at, 8f
        nop

        bnel    $a0, $at, 8f
        nop

        j       0b
        nop
8:

        jr      $ra
        nop

        .end    testcfg_branches

        .global testcfg_branches_never
        .ent    testcfg_branches_never

testcfg_branches_never:
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

        bltzal  $0, testcfg_branches_never
        nop

        bltzall  $0, testcfg_branches_never
        nop

1:
        jr      $ra
        nop

        .end    testcfg_branches_never

        .global testcfg_branches_always
        .ent    testcfg_branches_always

testcfg_branches_always:

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

        .end    testcfg_branches_always

        .global asmfunc
        .ent    asmfunc

asmfunc:
        jal     asmfunc_inner
        andi    $a0, $a0, 1

        slti    $t0, $a1, 7
        sll     $a0, $a1, 2
        blezl   $t0, asmfunc_default
        lui     $v0, %hi(asmfunc_switch)
        addu    $v0, $a0
        lw      $v0, %lo(asmfunc_switch)($v0)
        jr      $v0
        nop
asmfunc_case0:
asmfunc_case1:
asmfunc_case6:
asmfunc_case3:
        jr      $ra
        nop

asmfunc_case4:
asmfunc_case2:
        j       asmfunc_case0
        move    $a0, $v0


asmfunc_case5:
        jal     testcfg_branches_always
        nop

asmfunc_default:

        jr      $ra
        nop

asmfunc_inner:
        jal     mycallback
        nop
        lui     $v0, %hi(inner_callback)
        lw      $v0, %lo(inner_callback)($v0)
        jr      $v0
        nop

mycallback:
        jr      $ra
        nop

        .end    asmfunc


        .section .rodata
asmfunc_switch:
        .word   asmfunc_case0
        .word   asmfunc_case1
        .word   asmfunc_case2
        .word   asmfunc_case3
        .word   asmfunc_case4
        .word   asmfunc_case5
        .word   asmfunc_case6

inner_callback:
        .word   mycallback
        .word   mycallback


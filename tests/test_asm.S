        .section .text

        .set    noreorder
        .set    noat


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
        jal     sceKernelSleepThread
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


        .section .text

        .set    noreorder
        .set    noat


testcall_inner:
        mult    $a0, $a0
        jr      $ra
        mflo    $v0

        .global testcall
        .ent    testcall
testcall:
        jal     testcall_inner
        andi    $t0, $a0, 1
        lui     $v0, %hi(testcall_callback)
        bnez    $t0, 1f
        lw      $v0, %lo(testcall_callback)($v0)
        addiu   $a0, $a0, 2
1:
        jr      $v0
        nop

        .end    testcall


        .section .rodata
testcall_callback:
        .word   testcall_inner

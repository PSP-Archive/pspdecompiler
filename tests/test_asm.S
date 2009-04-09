        .section .text

        .set    noreorder
        .set    noat


testcc:
        addiu   $sp, $sp, -16
        sw      $ra, 4($sp)
        lw      $ra, 4($sp)
        jr      $ra
        addiu   $sp, $sp, 16

        .end    testcc


        .section .rodata

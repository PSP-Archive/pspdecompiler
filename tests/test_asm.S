        .section .text

        .set    noreorder
        .set    noat


        .global printf        
        .global dumpctrl1
        .ent    dumpctrl1
dumpctrl1:
        addiu   $sp, $sp, -136
        cfc1    $t0, $0
        sw      $t0, 0($sp)
        cfc1    $t0, $1
        sw      $t0, 4($sp)
        cfc1    $t0, $2
        sw      $t0, 8($sp)
        cfc1    $t0, $3
        sw      $t0, 12($sp)
        cfc1    $t0, $4
        sw      $t0, 16($sp)
        cfc1    $t0, $5
        sw      $t0, 20($sp)
        cfc1    $t0, $6
        sw      $t0, 24($sp)
        cfc1    $t0, $7
        sw      $t0, 28($sp)
        cfc1    $t0, $8
        sw      $t0, 32($sp)
        cfc1    $t0, $9
        sw      $t0, 36($sp)
        cfc1    $t0, $10
        sw      $t0, 40($sp)
        cfc1    $t0, $1
        sw      $t0, 44($sp)
        cfc1    $t0, $12
        sw      $t0, 48($sp)
        cfc1    $t0, $13
        sw      $t0, 52($sp)
        cfc1    $t0, $14
        sw      $t0, 56($sp)
        cfc1    $t0, $15
        sw      $t0, 60($sp)
        cfc1    $t0, $16
        sw      $t0, 64($sp)
        cfc1    $t0, $17
        sw      $t0, 68($sp)
        cfc1    $t0, $18
        sw      $t0, 72($sp)
        cfc1    $t0, $19
        sw      $t0, 76($sp)
        cfc1    $t0, $20
        sw      $t0, 80($sp)
        cfc1    $t0, $21
        sw      $t0, 84($sp)
        cfc1    $t0, $22
        sw      $t0, 88($sp)
        cfc1    $t0, $23
        sw      $t0, 92($sp)
        cfc1    $t0, $24
        sw      $t0, 96($sp)
        cfc1    $t0, $25
        sw      $t0, 100($sp)
        cfc1    $t0, $26
        sw      $t0, 104($sp)
        cfc1    $t0, $27
        sw      $t0, 108($sp)
        cfc1    $t0, $28
        sw      $t0, 112($sp)
        cfc1    $t0, $29
        sw      $t0, 116($sp)
        cfc1    $t0, $30
        sw      $t0, 120($sp)
        cfc1    $t0, $31
        sw      $t0, 124($sp)
        sw      $ra, 128($sp)

        lui     $a0, %hi($MSG2)
        jal     printf
        addiu   $a0, %lo($MSG2)

        jal     print_regs
        move    $a0, $sp
        
        lw      $ra, 128($sp)
        jr      $ra
        addiu   $sp, $sp, 136

        .end    dumpctrl1


print_regs:
        addiu   $sp, $sp, -16
        sw      $s2, 0($sp)
        sw      $ra, 4($sp)
        sw      $s0, 8($sp)
        sw      $s1, 12($sp)

        move    $s2, $a0

        move    $s0, $0
loop2:
        move    $s1, $0
loop1:
        sll     $t0, $s0, 2
        addu    $a1, $t0, $s1
        sll     $t2, $a1, 2
        addu    $t0, $s2, $t2
        lw      $a2, 0($t0)
        lui     $a0, %hi($MSG1)
        jal     printf
        addiu   $a0, %lo($MSG1)
        li      $t0, 3
        bne     $s1, $t0, loop1
        addiu   $s1, $s1, 1

        lui     $a0, %hi($MSG3)
        jal     printf
        addiu   $a0, %lo($MSG3) 

        li      $t0, 7
        bne     $s0, $t0, loop2
        addiu   $s0, $s0, 1

        lw      $s2, 0($sp)
        lw      $ra, 4($sp)
        lw      $s0, 8($sp)
        lw      $s1, 12($sp)
        jr      $ra
        addiu   $sp, $sp, 16



        .global dumpstatus1
        .ent    dumpstatus1
dumpstatus1:
        addiu   $sp, $sp, -136
        mfc1    $t0, $0
        sw      $t0, 0($sp)
        mfc1    $t0, $1
        sw      $t0, 4($sp)
        mfc1    $t0, $2
        sw      $t0, 8($sp)
        mfc1    $t0, $3
        sw      $t0, 12($sp)
        mfc1    $t0, $4
        sw      $t0, 16($sp)
        mfc1    $t0, $5
        sw      $t0, 20($sp)
        mfc1    $t0, $6
        sw      $t0, 24($sp)
        mfc1    $t0, $7
        sw      $t0, 28($sp)
        mfc1    $t0, $8
        sw      $t0, 32($sp)
        mfc1    $t0, $9
        sw      $t0, 36($sp)
        mfc1    $t0, $10
        sw      $t0, 40($sp)
        mfc1    $t0, $1
        sw      $t0, 44($sp)
        mfc1    $t0, $12
        sw      $t0, 48($sp)
        mfc1    $t0, $13
        sw      $t0, 52($sp)
        mfc1    $t0, $14
        sw      $t0, 56($sp)
        mfc1    $t0, $15
        sw      $t0, 60($sp)
        mfc1    $t0, $16
        sw      $t0, 64($sp)
        mfc1    $t0, $17
        sw      $t0, 68($sp)
        mfc1    $t0, $18
        sw      $t0, 72($sp)
        mfc1    $t0, $19
        sw      $t0, 76($sp)
        mfc1    $t0, $20
        sw      $t0, 80($sp)
        mfc1    $t0, $21
        sw      $t0, 84($sp)
        mfc1    $t0, $22
        sw      $t0, 88($sp)
        mfc1    $t0, $23
        sw      $t0, 92($sp)
        mfc1    $t0, $24
        sw      $t0, 96($sp)
        mfc1    $t0, $25
        sw      $t0, 100($sp)
        mfc1    $t0, $26
        sw      $t0, 104($sp)
        mfc1    $t0, $27
        sw      $t0, 108($sp)
        mfc1    $t0, $28
        sw      $t0, 112($sp)
        mfc1    $t0, $29
        sw      $t0, 116($sp)
        mfc1    $t0, $30
        sw      $t0, 120($sp)
        mfc1    $t0, $31
        sw      $t0, 124($sp)
        sw      $ra, 128($sp)

        lui     $a0, %hi($MSG4)
        jal     printf
        addiu   $a0, %lo($MSG4)

        jal     print_regs
        move    $a0, $sp

        lw      $ra, 128($sp)
        jr      $ra
        addiu   $sp, $sp, 136

        .end    dumpstatus1

        .global testcc
        .ent    testcc

testcc:
        addiu   $sp, $sp, -16
        sw      $ra, 4($sp)
        jal     dumpctrl1
        nop
        jal     dumpstatus1
        nop

        .word   0x460D6132
        //c.lt.s  $f12, $f13
        nop
        bc1f    OK
        move    $v0, $0
        li      $v0, 1
OK:
        sw      $v0, 0($sp)
        jal     dumpctrl1
        nop
        jal     dumpstatus1
        nop

        lw      $ra, 4($sp)
        lw      $v0, 0($sp)
        jr      $ra
        addiu   $sp, $sp, 16

        .end    testcc


        .section .rodata

$MSG1:
	.ascii  "%02d: 0x%08X \000"

$MSG2:
	.ascii  "FPU Control registers:\012\000"

$MSG3:
        .ascii "\012\000"

$MSG4:
	.ascii  "FPU Status registers:\012\000"

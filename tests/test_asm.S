        .section .text

        .set    noreorder
        .set    noat


        .global testcfg_branches
        .ent    testcfg_branches

testcfg_branches:
        nop
testcfg_branches_start:
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
        j       testcfg_branches_start
        nop

7:
        jr      $ra
        nop

        .end    testcfg_branches


        .global testcfg_branches_likely
        .ent    testcfg_branches_likely

testcfg_branches_likely:
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

        .end    testcfg_branches_likely



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

        .global testcfg_branchlink
        .ent    testcfg_branchlink

testcfg_branchlink:
        bgezal  $a0, testcfg_jumptobegin
        nop
        bltzal  $a0, testcfg_jumptobegin
        nop
        bltzall $a0, testcfg_jumptobegin
        nop

        jr      $ra
        nop

        .end    testcfg_branchlink

        .global testcfg_strangerefs
        .ent    testcfg_strangerefs

testcfg_strangerefs:
        beql    $a0, $a1, 2f
1:
        nop
        beq     $a0, $a1, 2f
        nop
        j       1b
        nop

2:
        bltzall $a0, testcfg_jumptobegin
3:
        nop
        beq     $a0, $a1, 3b
        nop

        jr      $ra
        nop

        .end    testcfg_strangerefs

        .global testcfg_jumptobegin
        .ent    testcfg_jumptobegin

testcfg_jumptobegin:
        nop
        beq     $a0, $a1, testcfg_jumptobegin
        nop

        jr      $ra
        nop

        .end    testcfg_jumptobegin


        .global testswitch
        .ent    testswitch

testswitch:
        jal     testswitch_innercallback
        nop
        slti    $t0, $a1, 7
        sll     $a0, $a1, 2
        blezl   $t0, switch1_default
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
        jal     testcfg_branches_always
        nop

switch1_default:

        jr      $ra
        nop

testswitch_innercallback:
        jal     mycallback
        nop
        lui     $v0, %hi(inner_callback)
        lw      $v0, %lo(inner_callback)($v0)
        jr      $v0
        nop

mycallback:
        jr      $ra
        nop

        .end    testswitch


	.global	forfunc
	.ent	forfunc
forfunc:
	.frame	$fp,32,$31		# vars= 24, regs= 1/0, args= 0, gp= 0
	.mask	0x40000000,-4
	.fmask	0x00000000,0
	.set	noreorder
	.set	nomacro
	
	addiu	$sp,$sp,-32
	sw	$fp,28($sp)
	move	$fp,$sp
	sw	$4,8($fp)
	sw	$5,12($fp)
	sw	$0,4($fp)
	j	$L2
	nop

$L8:
	lw	$2,4($fp)
	andi	$3,$2,0x3
	li	$2,1			# 0x1
	bne	$3,$2,$L3
	nop

	sw	$0,0($fp)
	j	$L4
	nop

$L7:
	lw	$3,4($fp)
	lw	$2,0($fp)
	mult	$3,$2
	mflo	$4
	lw	$3,8($fp)
	lw	$2,12($fp)
	addu	$2,$3,$2
	slt	$2,$4,$2
	bne	$2,$0,$L5
	nop

	lw	$2,4($fp)
	sw	$2,16($fp)
	j	$L6
	nop

$L5:
	lw	$2,0($fp)
	addiu	$2,$2,1
	sw	$2,0($fp)
$L4:
	lw	$2,0($fp)
	lw	$3,12($fp)
	slt	$2,$2,$3
	bne	$2,$0,$L7
	nop

$L3:
	lw	$2,4($fp)
	addiu	$2,$2,1
	sw	$2,4($fp)
$L2:
	lw	$2,4($fp)
	lw	$3,8($fp)
	slt	$2,$2,$3
	bne	$2,$0,$L8
	nop

	lw	$2,8($fp)
	sw	$2,16($fp)
$L6:
	lw	$2,16($fp)
	move	$sp,$fp
	lw	$fp,28($sp)
	addiu	$sp,$sp,32
	j	$31
	nop

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



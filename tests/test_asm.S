        .section .text

        .set    noreorder
        .set    noat


        .global testfunc
        .ent    testfunc

testfunc:
	lui     $at,%hi(var1)
        lw      $v0, %lo(var1)($at)
        lui     $at, %hi(var2)
        jr      $ra
        lw      $v1, %lo(var2)($at)

var1:
	.word   0


var2:
        .word   0

        .end    testfunc

        .section .data

var3:
	.word   var1

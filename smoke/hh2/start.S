! HH2 binary header: execution starts at byte zero, load address at byte 12.
.section .bootup,"ax"
.global _start
_start:
  bra .Lbody
  nop
  .space 8
  .long 0x8cff0000

.Lbody:
  mov.l .Lentry,r0
  jmp @r0
  nop
  .balign 4
.Lentry:
  .long _test_main

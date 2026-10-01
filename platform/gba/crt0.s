@ GBA cartridge header and startup. tools/gbafix.py fills in the header
@ checksum (and the logo, when given one) after linking.
    .section .crt0, "ax"
    .arm
    .global _start
_start:
    b       reset
    .fill   156, 1, 0           @ logo
    .ascii  "NOKIA3210\0\0\0"   @ title
    .ascii  "N32E"              @ game code
    .ascii  "00"                @ maker code
    .byte   0x96                @ fixed value
    .byte   0                   @ main unit code
    .byte   0                   @ device type
    .fill   7, 1, 0
    .byte   0                   @ software version
    .byte   0                   @ header checksum
    .fill   2, 1, 0

reset:
    @ IRQ mode stack, then system mode stack.
    mov     r0, #0x12
    msr     cpsr_c, r0
    ldr     sp, =__sp_irq
    mov     r0, #0x1f
    msr     cpsr_c, r0
    ldr     sp, =__sp_usr

    @ Copy .data from ROM to IWRAM.
    ldr     r0, =__data_lma
    ldr     r1, =__data_start
    ldr     r2, =__data_end
1:  cmp     r1, r2
    ldrlo   r3, [r0], #4
    strlo   r3, [r1], #4
    blo     1b

    @ Zero .bss.
    ldr     r1, =__bss_start
    ldr     r2, =__bss_end
    mov     r3, #0
2:  cmp     r1, r2
    strlo   r3, [r1], #4
    blo     2b

    ldr     r0, =main
    mov     lr, pc
    bx      r0
3:  b       3b
    .pool

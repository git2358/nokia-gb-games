/* Cartridge ROM at 0x08000000, variables in the 32 KiB internal work RAM. */
OUTPUT_FORMAT("elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)

MEMORY
{
    rom   (rx)  : ORIGIN = 0x08000000, LENGTH = 32M
    iwram (rwx) : ORIGIN = 0x03000000, LENGTH = 32K
}

__sp_irq = 0x03007fa0;
__sp_usr = 0x03007f00;

SECTIONS
{
    .text :
    {
        KEEP(*(.crt0))
        *(.text .text.*)
        *(.rodata .rodata.*)
        . = ALIGN(4);
    } > rom

    __data_lma = LOADADDR(.data);
    .data :
    {
        __data_start = .;
        *(.data .data.*)
        /* Code that runs faster from here than from the cartridge. */
        *(.iwram .iwram.*)
        . = ALIGN(4);
        __data_end = .;
    } > iwram AT > rom

    .bss (NOLOAD) :
    {
        __bss_start = .;
        *(.bss .bss.*)
        *(COMMON)
        . = ALIGN(4);
        __bss_end = .;
    } > iwram

    /DISCARD/ : { *(.ARM.exidx* .ARM.attributes .comment) }
}

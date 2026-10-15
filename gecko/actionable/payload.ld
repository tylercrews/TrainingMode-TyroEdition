ENTRY(blob_start)
SECTIONS {
    . = 0;
    .payload : {
        KEEP(*(.entry))
        *(.text*) *(.rodata*) *(.got2) *(.data*) *(.bss*) *(COMMON)
        BYTE(0)
        . = ALIGN(4);
    }
    /DISCARD/ : { *(.comment) *(.eh_frame*) *(.note*) }
}

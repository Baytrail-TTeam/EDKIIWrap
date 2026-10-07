ENTRY(payload_entry)
SECTIONS {
 . = 0;
 .text : { KEEP(*(.text.entry)) *(.text*) }
 .rodata : { *(.rodata*) }
 .data : { *(.data*) }
 .bss : { *(.bss*) *(COMMON) }
 /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.note*) }
}
ASSERT(SIZEOF(.bss)==0, "raw payload must have no uninitialized data")
ASSERT(payload_entry==0, "raw payload entry must be first byte")

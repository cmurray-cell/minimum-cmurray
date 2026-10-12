OUTPUT_ARCH(arm)
ENTRY(__process_user_start)

SECTIONS
{
  . = 0x00400000;
  .text : { *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss (NOLOAD) : { *(.bss*) *(COMMON) }
}

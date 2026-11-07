OUTPUT_FORMAT("elf32-sh")
OUTPUT_ARCH(sh)
ENTRY(_start)
SECTIONS {
  . = 0x8cff0000;
  .text : { KEEP(*(.bootup)) *(.text*) }
  .rodata : { *(.rodata*) }
  .data : { *(.data*) }
  .bss (NOLOAD) : { *(.bss*) *(COMMON) }
  .hollyhock_name 0 (INFO) : { KEEP(*(.hollyhock_name)) }
  .hollyhock_description 0 (INFO) : { KEEP(*(.hollyhock_description)) }
  .hollyhock_author 0 (INFO) : { KEEP(*(.hollyhock_author)) }
  .hollyhock_version 0 (INFO) : { KEEP(*(.hollyhock_version)) }
}

APP_NAME ?= launcher-test
PREFIX = sh4a_nofpueb-elf-
CFLAGS = -std=c23 -Os -ffreestanding -fno-builtin -fno-unwind-tables -fno-asynchronous-unwind-tables -Wall -Wextra -Werror
all: $(APP_NAME).hhk $(APP_NAME).bin
$(APP_NAME).hhk: main.c start.S linker.ld
	$(PREFIX)gcc $(CFLAGS) -nostdlib -T linker.ld -o $@ start.S main.c
$(APP_NAME).bin: $(APP_NAME).hhk
	$(PREFIX)objcopy -O binary -R .hollyhock_name -R .hollyhock_description -R .hollyhock_author -R .hollyhock_version $< $@
clean:
	rm -f $(APP_NAME).hhk $(APP_NAME).bin
.PHONY: all clean

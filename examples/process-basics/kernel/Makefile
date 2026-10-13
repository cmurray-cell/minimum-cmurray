ARM_PREFIX ?= arm-none-eabi-
CC := $(ARM_PREFIX)gcc

ROOT := $(abspath ../../..)
BUILD := build
PROGRAM := process-basics-kernel
ELF := $(BUILD)/$(PROGRAM).elf
SOURCES_C := $(wildcard *.c)
SOURCES_S := $(wildcard *.S)
OBJECTS := $(patsubst %.c,$(BUILD)/%.o,$(SOURCES_C)) \
	$(patsubst %.S,$(BUILD)/%.o,$(SOURCES_S))
DEPENDENCIES := $(OBJECTS:.o=.d)
KERNEL_LINKER := $(ROOT)/kernel/linker/kernel.ld

ARCH_FLAGS := -mcpu=cortex-a9 -marm -mfloat-abi=soft
CPPFLAGS := -Iinclude -I../include
CFLAGS := $(ARCH_FLAGS) -O2 -ffreestanding -fno-builtin -fdata-sections \
	-ffunction-sections -fno-stack-protector -std=c11 -Wall -Wextra -Werror -MMD -MP
ASFLAGS := $(ARCH_FLAGS) -ffreestanding -MMD -MP
LDFLAGS := $(ARCH_FLAGS) -nostdlib -Wl,-T,$(KERNEL_LINKER) \
	-Wl,--gc-sections -Wl,-u,minemu_bootstrap -Wl,-Map,$(BUILD)/$(PROGRAM).map
LIBGCC := $(shell $(CC) $(ARCH_FLAGS) -print-libgcc-file-name)

.PHONY: all clean

all: $(ELF)

$(ELF): $(OBJECTS) $(KERNEL_LINKER)
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBGCC) -o $@

$(BUILD)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/%.o: %.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(DEPENDENCIES)

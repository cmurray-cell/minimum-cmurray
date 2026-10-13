ARM_PREFIX ?= arm-none-eabi-
CC := $(ARM_PREFIX)gcc

PROCESS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/..)
BUILD := build
ELF := $(BUILD)/$(PROGRAM).elf
OBJECTS := $(BUILD)/main.o $(BUILD)/startup.o
DEPENDENCIES := $(OBJECTS:.o=.d)

ARCH_FLAGS := -mcpu=cortex-a9 -marm -mfloat-abi=soft
BKPT ?= 1
CPPFLAGS := -I$(PROCESS_DIR)/include
CPPFLAGS += -DPROCESS_EXAMPLE_ENABLE_BKPT=$(BKPT)
CFLAGS := $(ARCH_FLAGS) -O2 -ffreestanding -fno-builtin -fdata-sections \
	-ffunction-sections -fno-stack-protector -std=c11 -Wall -Wextra -Werror -MMD -MP
LDFLAGS := $(ARCH_FLAGS) -nostdlib -Wl,-T,$(PROCESS_DIR)/user/user.ld \
	-Wl,--gc-sections -Wl,-Map,$(BUILD)/$(PROGRAM).map
LIBGCC := $(shell $(CC) $(ARCH_FLAGS) -print-libgcc-file-name)

.PHONY: all clean

all: $(ELF)

$(ELF): $(OBJECTS) $(PROCESS_DIR)/user/user.ld
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(OBJECTS) $(LIBGCC) -o $@

$(BUILD)/main.o: main.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/startup.o: $(PROCESS_DIR)/user/startup.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(DEPENDENCIES)

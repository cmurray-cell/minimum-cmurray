ARM_PREFIX ?= arm-none-eabi-
CC := $(ARM_PREFIX)gcc
AR := $(ARM_PREFIX)ar

BUILD := build
KERNEL := $(BUILD)/minimum-kernel.elf
RUNTIME := $(BUILD)/libminemu_kernel.a

CORE_OBJECTS := \
        $(BUILD)/core/main.o \
        $(BUILD)/core/irq.o

RUNTIME_OBJECTS := \
        $(BUILD)/runtime/block.o \
        $(BUILD)/runtime/exception.o \
        $(BUILD)/runtime/memory.o \
        $(BUILD)/runtime/trace.o \
        $(BUILD)/startup/boot.o \
        $(BUILD)/startup/vectors.o

OBJECTS := $(CORE_OBJECTS) $(RUNTIME_OBJECTS)
DEPENDENCIES := $(OBJECTS:.o=.d)

ARCH_FLAGS := -mcpu=cortex-a9 -marm -mfloat-abi=soft

CPPFLAGS := -Iinclude

CFLAGS := $(ARCH_FLAGS) \
        -ffreestanding \
        -fno-builtin \
        -fdata-sections \
        -ffunction-sections \
        -fno-stack-protector \
        -std=c11 \
        -Wall \
        -Wextra \
        -Werror \
        -MMD \
        -MP

ASFLAGS := $(ARCH_FLAGS) \
        -ffreestanding \
        -MMD \
        -MP

LDFLAGS := $(ARCH_FLAGS) \
        -nostdlib \
        -Wl,-T,linker/kernel.ld \
        -Wl,--gc-sections \
        -Wl,-u,minemu_bootstrap \
        -Wl,-Map,$(BUILD)/minimum-kernel.map

LIBGCC := $(shell $(CC) $(ARCH_FLAGS) -print-libgcc-file-name)

.PHONY: all runtime clean

all: $(KERNEL)

runtime: $(RUNTIME)

$(RUNTIME): $(RUNTIME_OBJECTS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(KERNEL): $(CORE_OBJECTS) $(RUNTIME) linker/kernel.ld
	@mkdir -p $(dir $@)
	$(CC) $(LDFLAGS) $(CORE_OBJECTS) $(RUNTIME) $(LIBGCC) -o $@

$(BUILD)/core/%.o: src/core/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/core/%.o: src/core/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@

$(BUILD)/runtime/%.o: src/runtime/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

$(BUILD)/startup/%.o: src/startup/%.S
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(ASFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(DEPENDENCIES)
ARM_PREFIX ?= arm-none-eabi-
CC := $(ARM_PREFIX)gcc
AR := $(ARM_PREFIX)ar

BUILD := build
LIBRARY := $(BUILD)/libminimum_user.a
SOURCES := $(wildcard src/*.c)
OBJECTS := $(patsubst src/%.c,$(BUILD)/%.o,$(SOURCES))
DEPENDENCIES := $(OBJECTS:.o=.d)

ARCH_FLAGS := -mcpu=cortex-a9 -marm -mfloat-abi=soft
CPPFLAGS := -Iinclude -I../../kernel/include
CFLAGS := $(ARCH_FLAGS) -O2 -ffreestanding -fno-builtin -fdata-sections \
	-ffunction-sections -fno-stack-protector -std=c11 -Wall -Wextra -Werror -MMD -MP

.PHONY: all clean

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
	@mkdir -p $(dir $@)
	$(AR) rcs $@ $^

$(BUILD)/%.o: src/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CPPFLAGS) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(BUILD)

-include $(DEPENDENCIES)

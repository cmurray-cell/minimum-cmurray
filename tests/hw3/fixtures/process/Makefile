PROGRAM := hw3-process
MINEMU ?= minemu
ROOT := ../../../..
IMAGE := build/hw3-process.img

include $(ROOT)/user/common/common.mk

.PHONY: kernel image target

all: image

kernel:
	$(MAKE) -C $(ROOT)/kernel all

target:
	$(MAKE) -C target all

image: $(IMAGE)

$(IMAGE): image.toml $(ELF) target kernel
	@mkdir -p $(dir $@)
	$(MINEMU) image image.toml --output $@

PROGRAM := hw3-yield-fifo
MINEMU ?= minemu
ROOT := ../../../..
IMAGE := build/hw3-yield-fifo.img

include $(ROOT)/user/common/common.mk

.PHONY: kernel image

all: image

kernel:
	$(MAKE) -C $(ROOT)/kernel all

image: $(IMAGE)

$(IMAGE): image.toml $(ELF) kernel
	@mkdir -p $(dir $@)
	$(MINEMU) image image.toml --output $@

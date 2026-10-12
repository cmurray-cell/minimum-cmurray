MINEMU ?= minemu

IMAGE := build/process-basics.img

.PHONY: all kernel users image test clean

all: image

kernel:
	$(MAKE) -C kernel all

users:
	$(MAKE) -C user/parent all
	$(MAKE) -C user/target all

image: kernel users
	@mkdir -p $(dir $(IMAGE))
	$(MINEMU) image image.toml --output $(IMAGE)

test: image
	$(MINEMU) test test.toml

clean:
	$(MAKE) -C kernel clean
	$(MAKE) -C user/parent clean
	$(MAKE) -C user/target clean
	rm -rf build

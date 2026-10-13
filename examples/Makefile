KERNEL_EXAMPLES := \
	mmio-basics \
	svc-context-switch \
	irq-context-switch

.PHONY: all kernel-examples user-mode-hello process-basics process-basics-test \
	clean $(KERNEL_EXAMPLES)

all: kernel-examples

kernel-examples: $(KERNEL_EXAMPLES)

$(KERNEL_EXAMPLES):
	$(MAKE) -C $@ all

user-mode-hello:
	$(MAKE) -C user-mode-hello image

process-basics:
	$(MAKE) -C process-basics MINEMU="$(MINEMU)" image

process-basics-test:
	$(MAKE) -C process-basics MINEMU="$(MINEMU)" test

clean:
	@for example in $(KERNEL_EXAMPLES); do $(MAKE) -C $$example clean; done
	$(MAKE) -C user-mode-hello clean
	$(MAKE) -C process-basics clean

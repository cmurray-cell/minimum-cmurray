MINEMU ?= minemu

.PHONY: all prepare-process prepare-yield-fifo prepare-threads clean

all: prepare-process prepare-yield-fifo prepare-threads

prepare-process:
	$(MAKE) -C fixtures/process MINEMU="$(MINEMU)" all

prepare-yield-fifo:
	$(MAKE) -C fixtures/yield-fifo MINEMU="$(MINEMU)" all

prepare-threads:
	$(MAKE) -C fixtures/threads MINEMU="$(MINEMU)" all

prepare-%:
	@:

clean:
	$(MAKE) -C fixtures/process clean
	$(MAKE) -C fixtures/process/target clean
	$(MAKE) -C fixtures/yield-fifo clean
	$(MAKE) -C fixtures/threads clean

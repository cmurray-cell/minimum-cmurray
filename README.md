# minimum Template

This repository is the student starting point for the `minimum` teaching
operating system. It contains a starter A32 kernel, user-mode build support,
small examples, a supplied Boot ROM, and the manifest used to package a bootable
system image for `minemu`.

## Quickstart

### Prerequisites

For current Platform ABI v2 work, use a current development-container build or
install these tools locally. The published Assignment 1 image documented below
is a historical Platform ABI v1 environment.

- GNU Make and standard Unix build tools.
- `just` for the parameterized public-test workflow.
- The GNU Arm Embedded toolchain, including `arm-none-eabi-gcc`,
  `arm-none-eabi-ar`, `arm-none-eabi-objcopy`, `arm-none-eabi-readelf`, and
  `arm-none-eabi-nm`.
- `minemu` version `0.2.3` on `PATH`. Verify it with `minemu --version`.

The emulator and platform documentation are maintained in the
[minemu repository](https://github.com/rchtsang/minemu). Start with the
[normative platform ABI v2](https://github.com/rchtsang/minemu/blob/main/docs/platform/abi-v2.md)
or use the [documentation index](https://github.com/rchtsang/minemu/blob/main/docs/README.md)
to find component specifications and user guides.

### Build And Package

From the repository root, build the kernel, user program, examples, and supplied
Boot ROM check:

```sh
make
```

Packaging is a separate step. Build the system-ROM image after the source build:

```sh
make image
```

This creates `image/build/minimum.img`. If `minemu` is not on `PATH`, provide
its executable explicitly:

```sh
make image MINEMU=/path/to/minemu
```

### Public Tests

The HW1 public tests build the current image and exercise it as a black box:

```sh
just test-all hw1
```

Use the same `MINEMU` override when the executable is not on `PATH`:

```sh
MINEMU=/path/to/minemu just test-all hw1
```

The untouched starter is expected to fail these tests because UART output,
interrupt-driven input, and the shell are student work. See
[`tests/README.md`](tests/README.md) for parameterized test commands and optional
student tests.

### Run

Boot the packaged image with the supplied 64-KiB Boot ROM:

```sh
minemu run image/build/minimum.img \
  --boot-rom bootloader/bootloader.bin
```

Later assignments can attach independent filesystem and swap media:

```sh
minemu run image/build/minimum.img \
  --boot-rom bootloader/bootloader.bin \
  --block0-media filesystem.img \
  --block1-media swap.img
```

`--block-media` and `-m` remain compatibility aliases for unit 0.

The TUI opens with emulation paused at the reset vector. These controls are
enough for the initial workflow:

| Input | Effect |
|---|---|
| `Space`, then `s` | Start or pause execution. |
| `:start` | Start continuous execution. |
| `:stop` | Pause execution. |
| `i` | Enter console insert mode and send keys to the selected UART. |
| `Esc` | Return to normal mode. |
| `Space`, then `i` | Open inspect view and pause before taking snapshots. |
| `Tab` | Cycle the focused inspect pane's subview. |
| `?` | Open the complete in-application help table. |
| `:q` | Shut down the emulator and quit. |

See the full [TUI guide](https://github.com/rchtsang/minemu/blob/main/docs/dev/tui.md)
for navigation, memory inspection, searches, commands, and UART selection.

## Where To Work

```text
bootloader/  Supplied reset firmware and canonical 64-KiB Boot ROM
examples/    Self-contained kernel and user-mode examples
kernel/      Starter kernel, platform headers, and linker script
user/        User support library, common build rules, and starter program
image/       Image manifest and generated packaged image
tests/       Public black-box manifests and optional student tests
```

- Start kernel work in `kernel/src/core/` and use interfaces from
  `kernel/include/minemu/`.
- Preserve the supplied vector table and exception-mode stack initialization.
  Assignment 1 replaces the weak IRQ hooks by adapting the supplied
  `irq-context-switch` example.
- The Assignment 1 versions of `minemu/platform.h`, `minemu/trap.h`,
  `minemu/irq.h`, and the
  `minemu_irq_dispatch` boundary are fixed for Assignment 1. Console, buffer,
  line-reader, and shell interfaces remain student-defined.
- `minemu/block.h` provides the supplied serialized synchronous block interface;
  by course convention, unit 0 is filesystem/general media and unit 1 is swap.
  The hardware treats both as identical raw-sector units.
- `minemu/debug.h` provides `breakpoint(TAG)` for programmer-inserted debugging
  pauses. `TAG` must be a compile-time constant from `0` through `65535`:

  ```c
  #include "minemu/debug.h"

  breakpoint(0x1234);
  ```

  The emulator pauses with PC on the `bkpt` instruction so you can inspect the
  machine. Manually resuming execution advances past that breakpoint once and
  continues at the following instruction.
- Add new C or assembly sources under `kernel/src/` and list their objects in
  `kernel/Makefile`; the starter intentionally does not prescribe a subsystem
  layout.
- Add self-contained examples under `examples/`.
- Add independently linked user programs under `user/prog/` using the existing
  directory-local Makefile pattern.
- Select the kernel and user modules included in the image by editing
  `image/minimum.toml`.

Generated files remain in a `build/` directory beneath the component that owns
them. The repository root does not contain build output.

## Useful Targets

```sh
make kernel
make kernel-examples
make user
make image
just test-all hw1
make clean
```

Individual examples and programs can also be built directly:

```sh
make -C examples/mmio-basics
make -C examples/svc-context-switch
make -C examples/irq-context-switch
make -C user/prog/minimum-user
```

The user program links `user/lib/build/libminimum_user.a` and `libgcc`
statically. Newlib and newlib-nano are not part of the platform.

The user-mode program and SVC example are supplied for later assignments. They
are not Assignment 1 implementation work.

### User-Mode Hello Example

The complete later-assignment example packages `user-hello`, loads its module
segments into user-accessible RAM, creates a protected user stack, enters A32
USR mode, and handles its custom one-byte UART syscall:

```sh
make user-mode-hello-image
make -C examples/user-mode-hello test
```

All example-only source, headers, packaging, and tests are under
`examples/user-mode-hello/`; none are part of the canonical kernel or user
program trees. The example deliberately uses a small custom syscall. Assignment
2 generalizes the same SVC path into its required `ioctl` interface.

## Bootloader Maintenance

Normal student work uses the checked-in `bootloader/bootloader.bin`; `make`
automatically rebuilds and compares it as a consistency check. Students should
not replace the canonical firmware. Manual firmware build, comparison, and
update instructions are documented separately in the [bootloader maintainer
guide](bootloader/README.md).

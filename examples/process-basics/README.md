# HW3 Process Basics

This is a self-contained, inspectable Task 1 process example. It packages two
user ELF modules and runs this deterministic sequence:

1. `process-parent` calls `fork()`.
2. The parent calls exact-child `wait(child, &status)` and blocks. No yield is
   needed to run the child.
3. The child resumes from `fork()` with return value zero and calls
   `exec("process-target")`.
4. `process-target` emits trace `0x50524301` and exits with status 37.
5. The awakened parent verifies both the returned PID and status, emits trace
   `0x50524302`, prints `process-basics: success`, and executes a breakpoint
   in user mode.

Build and test from the repository root:

```sh
make process-basics-image
make process-basics-test
```

The default build executes the marked `bkpt #0`. The test requires a Minemu
version with software-breakpoint handling and asserts that the paused CPU is in
USR mode. `BKPT=0` is available only as a debugging compatibility override.

## What To Read

- `kernel/include/process.h`: PCB, TCB, and the required process/thread states.
- `kernel/process.c`: FIFO queue primitives, `fork`, `exec`, exact-child
  `wait`, process-wide `_exit`, and the marked `sched_yield` student stub.
- `kernel/address_space.c`: eager module loading, eager fork cloning, mapping,
  activation, range checks, and cross-address-space copies.
- `kernel/syscall.c`: A32 SVC validation and the existing HW2 `ioctl` request
  dispatch. Process calls are added after, rather than replacing, `ioctl`.
- `kernel/context.S`: trap-frame save/restore and first user entry.
- `kernel/boot.S`, `kernel/vectors.S`, and `kernel/runtime.c`: the small local
  bootstrap/runtime that keeps this example independent of kernel HW3 changes.
- `user/startup.c`: user startup plus `ioctl`, process, and yield wrappers.
- `user/pthread_wrapper_example.c`: an unlinked user-only pthread
  wrapper/trampoline sketch with integration TODOs. There are deliberately no
  pthread create/join/exit implementations in this example kernel.
- `image.toml` and `test.toml`: packaged modules and deterministic assertions.

`wait(pid, NULL)` is accepted: a zero status address skips both validation and
copyout. The demo passes a real status pointer so it can verify the child's
exit value. A live child can have only one waiter; another wait for that child
returns `-EINVAL` without changing either thread or child state.

## HW2 Integration Order

Integrate file by file; do not replace your working HW2 syscall path wholesale.

1. Copy the syscall numbers and user declarations you need from
   `include/process_abi.h` into your shared ABI header. Keep every existing HW2
   `ioctl` number, file descriptor, request value, and variadic type unchanged.
2. Add the process wrappers and `__process_user_start` pattern from
   `user/startup.c` to your user library/startup. Preserve your existing
   `ioctl()` wrapper.
3. Add the eager address-space interface from `kernel/include/address_space.h`,
   then port `kernel/address_space.c` to your allocator or fixed backing store.
   Verify module load, activate, copy, clone, and destroy before scheduling.
4. Add the state enums and PCB/TCB fields from `kernel/include/process.h`.
   Initialize every slot to `PROCESS_FREE`/`FREE` during kernel startup.
5. Add `queue_push`, `queue_pop`, and `queue_remove_process` from
   `kernel/process.c`. Keep the queue FIFO and only enqueue `READY` threads.
6. Port `process_init`, frame initialization, and the context assembly. Start
   one packaged module in USR mode before adding process syscalls.
7. Add `fork`, then `exec`, then exact-child `wait`, testing each separately.
   Finally add process-wide `_exit` and deferred address-space destruction.
8. In your existing SVC dispatcher, leave the HW2 `ioctl` branch intact and
   route only the new syscall numbers to `process_handle_syscall`.
9. Leave `sys_sched_yield` as the marked TODO until implementing the scheduling
   task yourself. Do not infer Task 2 kernel pthread behavior from the
   user-only wrapper sketch.
10. Package both user modules with `image.toml`, run the test, and confirm the
    trace order is exactly target then parent-success.

The example uses bounded static storage to keep allocation policy visible. A
production kernel should separate page ownership, process lifetime, and kernel
stack allocation according to its own memory manager.

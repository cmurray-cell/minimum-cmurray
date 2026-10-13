#ifndef PROCESS_EXAMPLE_PROCESS_H
#define PROCESS_EXAMPLE_PROCESS_H

#include <stdint.h>

#include "address_space.h"
#include "example_kernel.h"

#define EXAMPLE_MAX_PROCESSES 4U
#define EXAMPLE_MAX_THREADS_PER_PROCESS 4U
#define EXAMPLE_KERNEL_STACK_SIZE 4096U

enum example_process_state {
    PROCESS_FREE,
    LIVE,
    ZOMBIE,
};

enum example_thread_state {
    FREE,
    RUNNING,
    READY,
    BLOCKED,
    EXITED,
};

enum example_block_reason {
    BLOCK_NONE,
    BLOCK_WAIT,
};

struct example_process;

/* TCB: Task 1 uses one thread, but keeps the HW3 thread state model explicit. */
struct example_thread {
    enum example_thread_state state;
    uint32_t tid;
    enum example_block_reason block_reason;
    struct example_process *process;
    struct example_trap_frame *frame;
    uint8_t kernel_stack[EXAMPLE_KERNEL_STACK_SIZE] __attribute__((aligned(8)));
};

/* PCB: owns the address space, child/wait relationship, and all of its TCBs. */
struct example_process {
    enum example_process_state state;
    int32_t pid;
    int exit_status;
    struct example_process *parent;
    struct example_thread *waiter;
    uint32_t wait_status_address;
    struct example_address_space *space;
    struct example_thread threads[EXAMPLE_MAX_THREADS_PER_PROCESS];
};

bool process_init(const char *module_name, size_t name_length);
void process_start(void) __attribute__((noreturn));
struct example_trap_frame *process_handle_syscall(
    struct example_trap_frame *frame, uint32_t syscall_number);
struct example_address_space *process_current_space(void);
void process_enter_frame(struct example_trap_frame *frame)
    __attribute__((noreturn));

#endif

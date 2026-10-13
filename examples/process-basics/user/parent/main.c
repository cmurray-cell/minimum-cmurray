#include <stddef.h>
#include <stdint.h>

#include "process_abi.h"

#define TRACE_SUCCESS UINT32_C(0x50524302)
#define TRACE_FAILURE UINT32_C(0x505243ff)
#define TARGET_STATUS 37

#ifndef PROCESS_EXAMPLE_ENABLE_BKPT
#define PROCESS_EXAMPLE_ENABLE_BKPT 0
#endif

static void write_message(const char *message, uint32_t length) {
    (void)ioctl(PROCESS_FD_UART_OUT, PROCESS_IOCTL_UART_WRITE,
                (void *)message, length);
}

static void user_breakpoint(void) {
#if PROCESS_EXAMPLE_ENABLE_BKPT
    /* The test pauses here and verifies that this executes in USR mode. */
    __asm__ volatile("bkpt #0");
#endif
}

static _Noreturn void stop_with_trace(uint32_t trace) {
    (void)ioctl(PROCESS_FD_TRACE, PROCESS_IOCTL_TRACE_EVENT, trace);
    user_breakpoint();
    for (;;) {
        __asm__ volatile("nop");
    }
}

void process_user_main(void) {
    process_pid_t child = fork();
    if (child < 0) {
        stop_with_trace(TRACE_FAILURE);
    }
    if (child == 0) {
        if (exec("process-target") < 0) {
            _exit(100);
        }
    }

    int status = -1;
    if (wait(child, &status) != child || status != TARGET_STATUS) {
        stop_with_trace(TRACE_FAILURE);
    }

    (void)ioctl(PROCESS_FD_TRACE, PROCESS_IOCTL_TRACE_EVENT, TRACE_SUCCESS);
    static const char success[] = "process-basics: success\n";
    write_message(success, sizeof(success) - 1U);
    user_breakpoint();
    for (;;) {
        __asm__ volatile("nop");
    }
}

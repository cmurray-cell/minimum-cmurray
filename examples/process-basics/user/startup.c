#include <stdarg.h>
#include <stdint.h>

#include "process_abi.h"

static int32_t process_svc(uint32_t number, uint32_t argument0,
                           uint32_t argument1, uint32_t argument2,
                           uint32_t argument3) {
    register uint32_t r0 __asm__("r0") = argument0;
    register uint32_t r1 __asm__("r1") = argument1;
    register uint32_t r2 __asm__("r2") = argument2;
    register uint32_t r3 __asm__("r3") = argument3;
    register uint32_t r7 __asm__("r7") = number;

    __asm__ volatile("svc #0"
                     : "+r"(r0)
                     : "r"(r1), "r"(r2), "r"(r3), "r"(r7)
                     : "memory", "cc");
    return (int32_t)r0;
}

void process_user_main(void);

_Noreturn void __process_user_start(void) {
    process_user_main();
    _exit(0);
}

int32_t ioctl(int fd, uint32_t request, ...) {
    uint32_t argument0 = 0;
    uint32_t argument1 = 0;
    va_list arguments;

    va_start(arguments, request);
    switch (request) {
    case PROCESS_IOCTL_UART_READ:
    case PROCESS_IOCTL_UART_WRITE:
        argument0 = (uint32_t)(uintptr_t)va_arg(arguments, void *);
        argument1 = va_arg(arguments, uint32_t);
        break;
    case PROCESS_IOCTL_RNG_NEXT:
    case PROCESS_IOCTL_RNG_STATE:
        argument0 = (uint32_t)(uintptr_t)va_arg(arguments, uint32_t *);
        break;
    case PROCESS_IOCTL_RNG_SEED:
    case PROCESS_IOCTL_TRACE_EVENT:
        argument0 = va_arg(arguments, uint32_t);
        break;
    default:
        break;
    }
    va_end(arguments);
    return process_svc(PROCESS_SYSCALL_IOCTL, (uint32_t)fd, request,
                       argument0, argument1);
}

process_pid_t fork(void) {
    return process_svc(PROCESS_SYSCALL_FORK, 0, 0, 0, 0);
}

int exec(const char *module_name) {
    return process_svc(PROCESS_SYSCALL_EXEC,
                       (uint32_t)(uintptr_t)module_name, 0, 0, 0);
}

process_pid_t wait(process_pid_t pid, int *status) {
    return process_svc(PROCESS_SYSCALL_WAIT, (uint32_t)pid,
                       (uint32_t)(uintptr_t)status, 0, 0);
}

_Noreturn void _exit(int status) {
    (void)process_svc(PROCESS_SYSCALL_EXIT, (uint32_t)status, 0, 0, 0);
    for (;;) {
    }
}

int sched_yield(void) {
    return process_svc(PROCESS_SYSCALL_SCHED_YIELD, 0, 0, 0, 0);
}

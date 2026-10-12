#include <stdint.h>

#include "minemu/user_abi.h"

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

void minemu_user_main(void);

_Noreturn void __minemu_user_start(void) {
    minemu_user_main();
    _exit(0);
}

pid_t fork(void) {
    return process_svc(MINEMU_SYSCALL_FORK, 0, 0, 0, 0);
}

int exec(const char *module_name) {
    return process_svc(MINEMU_SYSCALL_EXEC,
                       (uint32_t)(uintptr_t)module_name, 0, 0, 0);
}

pid_t wait(pid_t pid, int *status) {
    return process_svc(MINEMU_SYSCALL_WAIT, (uint32_t)pid,
                       (uint32_t)(uintptr_t)status, 0, 0);
}

_Noreturn void _exit(int status) {
    (void)process_svc(MINEMU_SYSCALL_EXIT, (uint32_t)status, 0, 0, 0);
    for (;;) {
    }
}

_Noreturn void __minemu_pthread_start(void *(*start_routine)(void *),
                                      void *argument) {
    pthread_exit(start_routine(argument));
}

int pthread_create(pthread_t *thread, void *(*start_routine)(void *),
                   void *argument) {
    return process_svc(MINEMU_SYSCALL_PTHREAD_CREATE,
                       (uint32_t)(uintptr_t)thread,
                       (uint32_t)(uintptr_t)start_routine,
                       (uint32_t)(uintptr_t)argument,
                       (uint32_t)(uintptr_t)__minemu_pthread_start);
}

int pthread_join(pthread_t thread, void **return_value) {
    return process_svc(MINEMU_SYSCALL_PTHREAD_JOIN, thread,
                       (uint32_t)(uintptr_t)return_value, 0, 0);
}

_Noreturn void pthread_exit(void *return_value) {
    (void)process_svc(MINEMU_SYSCALL_PTHREAD_EXIT,
                      (uint32_t)(uintptr_t)return_value, 0, 0, 0);
    for (;;) {
    }
}

int sched_yield(void) {
    return process_svc(MINEMU_SYSCALL_SCHED_YIELD, 0, 0, 0, 0);
}

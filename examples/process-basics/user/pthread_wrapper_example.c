/*
 * USER-SIDE EXAMPLE ONLY: this file is intentionally not linked into the demo.
 *
 * Integration TODO:
 *  1. Add these declarations to your user pthread header.
 *  2. Share your raw SVC helper instead of keeping this private copy.
 *  3. Implement the three corresponding kernel syscalls yourself. They are
 *     deliberately absent from this Task 1 process example.
 *  4. Have create start the new user context at pthread_start_trampoline.
 */
#include <stdint.h>

#include "process_abi.h"

typedef uint32_t pthread_t;

static int32_t pthread_svc(uint32_t number, uint32_t argument0,
                           uint32_t argument1, uint32_t argument2,
                           uint32_t argument3) {
    register uint32_t r0 __asm__("r0") = argument0;
    register uint32_t r1 __asm__("r1") = argument1;
    register uint32_t r2 __asm__("r2") = argument2;
    register uint32_t r3 __asm__("r3") = argument3;
    register uint32_t r7 __asm__("r7") = number;
    __asm__ volatile("svc #0" : "+r"(r0)
                     : "r"(r1), "r"(r2), "r"(r3), "r"(r7)
                     : "memory", "cc");
    return (int32_t)r0;
}

_Noreturn void pthread_exit(void *return_value) {
    (void)pthread_svc(PROCESS_SYSCALL_PTHREAD_EXIT,
                      (uint32_t)(uintptr_t)return_value, 0, 0, 0);
    for (;;) {
    }
}

static _Noreturn void pthread_start_trampoline(
    void *(*start_routine)(void *), void *argument) {
    pthread_exit(start_routine(argument));
}

int pthread_create(pthread_t *thread, void *(*start_routine)(void *),
                   void *argument) {
    return pthread_svc(PROCESS_SYSCALL_PTHREAD_CREATE,
                       (uint32_t)(uintptr_t)thread,
                       (uint32_t)(uintptr_t)start_routine,
                       (uint32_t)(uintptr_t)argument,
                       (uint32_t)(uintptr_t)pthread_start_trampoline);
}

int pthread_join(pthread_t thread, void **return_value) {
    return pthread_svc(PROCESS_SYSCALL_PTHREAD_JOIN, thread,
                       (uint32_t)(uintptr_t)return_value, 0, 0);
}

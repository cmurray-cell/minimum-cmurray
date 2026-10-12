#ifndef MINEMU_USER_ABI_H
#define MINEMU_USER_ABI_H

#define MINEMU_SYSCALL_IOCTL 1
#define MINEMU_SYSCALL_FORK 2
#define MINEMU_SYSCALL_EXEC 3
#define MINEMU_SYSCALL_WAIT 4
#define MINEMU_SYSCALL_EXIT 5
#define MINEMU_SYSCALL_PTHREAD_CREATE 6
#define MINEMU_SYSCALL_PTHREAD_JOIN 7
#define MINEMU_SYSCALL_PTHREAD_EXIT 8
#define MINEMU_SYSCALL_SCHED_YIELD 9

#define MINEMU_FD_UART_IN 0
#define MINEMU_FD_UART_OUT 1
#define MINEMU_FD_UART_DIAG 2
#define MINEMU_FD_RNG 3
#define MINEMU_FD_TRACE 4

#define MINEMU_IOCTL_UART_READ 0x00000101
#define MINEMU_IOCTL_UART_WRITE 0x00000102
#define MINEMU_IOCTL_UART_STATUS 0x00000103
#define MINEMU_IOCTL_RNG_NEXT 0x00000201
#define MINEMU_IOCTL_RNG_STATE 0x00000202
#define MINEMU_IOCTL_RNG_SEED 0x00000203
#define MINEMU_IOCTL_TRACE_EVENT 0x00000301

#define MINEMU_IOCTL_UART_RX_READY 1
#define MINEMU_IOCTL_UART_TX_READY 2
#define MINEMU_IOCTL_MAX_TRANSFER 256
#define MINEMU_IOCTL_ERROR (-1)

#ifndef __ASSEMBLER__
#include <stddef.h>
#include <stdint.h>

#ifndef MINIMUM_PID_T_DEFINED
typedef int32_t pid_t;
#define MINIMUM_PID_T_DEFINED
#endif
typedef uint32_t pthread_t;

/* UART buffer arguments use the exact variadic type void *. */
int32_t ioctl(int fd, uint32_t request, ...);
pid_t fork(void);
int exec(const char *module_name);
pid_t wait(pid_t pid, int *status);
_Noreturn void _exit(int status);
int pthread_create(pthread_t *thread, void *(*start_routine)(void *),
                   void *argument);
int pthread_join(pthread_t thread, void **return_value);
_Noreturn void pthread_exit(void *return_value);
int sched_yield(void);
#endif

#endif

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "process.h"
#include "process_abi.h"

#define CPSR_MODE_MASK UINT32_C(0x1f)
#define CPSR_MODE_USR UINT32_C(0x10)
#define CPSR_THUMB UINT32_C(0x20)
#define A32_SVC_ZERO UINT32_C(0xef000000)

struct example_uart_regs {
    uint32_t rx_data;
    uint32_t tx_data;
    uint32_t status;
    uint32_t control;
};

struct example_rng_regs {
    uint32_t seed;
    uint32_t data;
    uint32_t state;
};

#define UART0 ((volatile struct example_uart_regs *)(uintptr_t)EXAMPLE_UART0_BASE)
#define RNG ((volatile struct example_rng_regs *)(uintptr_t)EXAMPLE_RNG_BASE)
#define TRACE_EVENT (*(volatile uint32_t *)(uintptr_t)EXAMPLE_TRACE_BASE)

static int32_t uart_read_ioctl(uint32_t address, uint32_t count) {
    struct example_address_space *space = process_current_space();
    uint8_t bytes[PROCESS_IOCTL_MAX_TRANSFER];
    if (count > PROCESS_IOCTL_MAX_TRANSFER ||
        !address_space_writable(space, address, count)) {
        return PROCESS_IOCTL_ERROR;
    }
    uint32_t copied = 0;
    while (copied < count && (UART0->status & EXAMPLE_UART_RX_READY) != 0) {
        bytes[copied++] = (uint8_t)UART0->rx_data;
    }
    return address_space_copy_to(space, address, bytes, copied)
               ? (int32_t)copied
               : PROCESS_IOCTL_ERROR;
}

static int32_t uart_write_ioctl(uint32_t address, uint32_t count) {
    struct example_address_space *space = process_current_space();
    uint8_t bytes[PROCESS_IOCTL_MAX_TRANSFER];
    if (count > PROCESS_IOCTL_MAX_TRANSFER ||
        !address_space_copy_from(bytes, space, address, count)) {
        return PROCESS_IOCTL_ERROR;
    }
    for (uint32_t index = 0; index < count; ++index) {
        while ((UART0->status & EXAMPLE_UART_TX_READY) == 0) {
        }
        UART0->tx_data = bytes[index];
    }
    return (int32_t)count;
}

static int32_t ioctl_dispatch(int32_t fd, uint32_t request,
                              uint32_t argument0, uint32_t argument1) {
    switch (request) {
    case PROCESS_IOCTL_UART_READ:
        return fd == PROCESS_FD_UART_IN
                   ? uart_read_ioctl(argument0, argument1)
                   : PROCESS_IOCTL_ERROR;
    case PROCESS_IOCTL_UART_WRITE:
        return fd == PROCESS_FD_UART_OUT || fd == PROCESS_FD_UART_DIAG
                   ? uart_write_ioctl(argument0, argument1)
                   : PROCESS_IOCTL_ERROR;
    case PROCESS_IOCTL_UART_STATUS: {
        if (fd != PROCESS_FD_UART_IN && fd != PROCESS_FD_UART_OUT &&
            fd != PROCESS_FD_UART_DIAG) {
            return PROCESS_IOCTL_ERROR;
        }
        uint32_t result = 0;
        if ((UART0->status & EXAMPLE_UART_RX_READY) != 0) {
            result |= PROCESS_IOCTL_UART_RX_READY;
        }
        if ((UART0->status & EXAMPLE_UART_TX_READY) != 0) {
            result |= PROCESS_IOCTL_UART_TX_READY;
        }
        return (int32_t)result;
    }
    case PROCESS_IOCTL_RNG_NEXT:
    case PROCESS_IOCTL_RNG_STATE: {
        struct example_address_space *space = process_current_space();
        if (fd != PROCESS_FD_RNG ||
            !address_space_writable(space, argument0,
                                    sizeof(uint32_t))) {
            return PROCESS_IOCTL_ERROR;
        }
        uint32_t value = request == PROCESS_IOCTL_RNG_NEXT ? RNG->data :
                                                               RNG->state;
        return address_space_copy_to(space, argument0, &value,
                                     sizeof(value))
                   ? 0
                   : PROCESS_IOCTL_ERROR;
    }
    case PROCESS_IOCTL_RNG_SEED:
        if (fd != PROCESS_FD_RNG) {
            return PROCESS_IOCTL_ERROR;
        }
        RNG->seed = argument0;
        return 0;
    case PROCESS_IOCTL_TRACE_EVENT:
        if (fd != PROCESS_FD_TRACE) {
            return PROCESS_IOCTL_ERROR;
        }
        TRACE_EVENT = argument0;
        return 0;
    default:
        return PROCESS_IOCTL_ERROR;
    }
}

struct example_trap_frame *process_svc_dispatch(
    struct example_trap_frame *frame) {
    struct example_address_space *space = process_current_space();
    uint32_t instruction;
    if (frame->exception_id != EXAMPLE_EXCEPTION_SVC ||
        (frame->spsr & CPSR_MODE_MASK) != CPSR_MODE_USR ||
        (frame->spsr & CPSR_THUMB) != 0 ||
        !address_space_copy_from(&instruction, space, frame->fault_pc,
                                 sizeof(instruction)) ||
        instruction != A32_SVC_ZERO) {
        frame->r[0] = (uint32_t)PROCESS_IOCTL_ERROR;
        return frame;
    }
    if (frame->r[7] == PROCESS_SYSCALL_IOCTL) {
        frame->r[0] = (uint32_t)ioctl_dispatch((int32_t)frame->r[0],
                                               frame->r[1], frame->r[2],
                                               frame->r[3]);
        return frame;
    }
    return process_handle_syscall(frame, frame->r[7]);
}

void minemu_undefined_dispatch(struct example_trap_frame *frame) {
    (void)frame;
    minemu_fail_stop();
}

void minemu_abort_dispatch(struct example_trap_frame *frame) {
    (void)frame;
    minemu_fail_stop();
}

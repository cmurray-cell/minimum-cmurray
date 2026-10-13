#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "minemu/platform.h"
#include "minemu/trap.h"
#include "minemu/trace.h"
#include "minemu/mmu.h"
#include "minemu/user_abi.h"

#define CPSR_MODE_MASK UINT32_C(0x1f)
#define CPSR_MODE_USR  UINT32_C(0x10)
#define CPSR_THUMB     UINT32_C(0x20)

#define A32_SVC_ZERO UINT32_C(0xef000000)
#define BOOTSTRAP_PAGE_DIRECTORY_PADDR UINT32_C(0x40010000)

extern int uart_try_getc(uint8_t *byte);
extern int uart_rx_available(void);


//return a pointer to the active page directory.
static volatile uint32_t *active_page_directory(void) {
    return (volatile uint32_t *)(uintptr_t)(
        MINEMU_KERNEL_DIRECT_BASE + BOOTSTRAP_PAGE_DIRECTORY_PADDR - MINEMU_RAM_BASE);
}

//validate every page, including the first and last partial pages
static bool user_pages_have_flags(uint32_t address, uint32_t length,
                                  uint32_t access_flags) {
    if (length == 0) {
        return true;
    }
    if (length - 1U > UINT32_MAX - address) {
        return false;
    }
    uint32_t final_page = (address + length - 1U) & MINEMU_PTE_PAGE_MASK;
    uint32_t page = address & MINEMU_PTE_PAGE_MASK;
    volatile uint32_t *directory = active_page_directory();

    for (;;) {
        uint32_t pde = directory[page >> 22];
        if ((pde & MINEMU_PDE_VALID) == 0) {
            return false;
        }
        uint32_t paddr = pde & MINEMU_PDE_PAGE_MASK;
        if (paddr < MINEMU_RAM_BASE ||
            paddr > MINEMU_RAM_BASE + MINEMU_RAM_SIZE - MINEMU_PAGE_SIZE) {
            return false;
        }
        volatile uint32_t *table = (volatile uint32_t *)(uintptr_t)(
            MINEMU_KERNEL_DIRECT_BASE + paddr - MINEMU_RAM_BASE);
        uint32_t pte = table[(page >> 12) & 1023U];
        uint32_t required = MINEMU_PTE_VALID | MINEMU_PTE_USER | access_flags;
        if ((pte & required) != required) {
            return false;
        }
        if (page == final_page) {
            return true;
        }
        page += MINEMU_PAGE_SIZE;
    }
}

//check that a range of user memory is valid and accessible for the specified operation
static bool user_range_valid(uint32_t address, uint32_t length, bool write) {
    return user_pages_have_flags(
        address, length,
        write ? MINEMU_PTE_WRITABLE : MINEMU_PTE_READABLE);
}

static int32_t uart_read(uint32_t fd, uint32_t address, uint32_t length) {
    //input is only valid for the UART input descriptor and must not exceed the maximum transfer size
    if (fd != MINEMU_FD_UART_IN || length > MINEMU_IOCTL_MAX_TRANSFER) {
        return MINEMU_IOCTL_ERROR;
    }
    if (!user_range_valid(address, length, true)) {
        return MINEMU_IOCTL_ERROR;
    }
    uint8_t *destination = (uint8_t *)(uintptr_t)address;
    uint32_t copied = 0;
    while (copied < length) {
        uint8_t byte;
        if (!uart_try_getc(&byte)) {
            break;
        }
        destination[copied++] = byte;
    }
    return (int32_t)copied;
}

static int32_t uart_write(uint32_t fd, uint32_t address, uint32_t length) {
    if ((fd != MINEMU_FD_UART_OUT && fd != MINEMU_FD_UART_DIAG) ||
        length > MINEMU_IOCTL_MAX_TRANSFER) {
        return MINEMU_IOCTL_ERROR;
    }
    if (!user_range_valid(address, length, false)) {
        return MINEMU_IOCTL_ERROR;
    }

    volatile struct minemu_uart_regs *device =
        (fd == MINEMU_FD_UART_DIAG) ? MINEMU_UART1 : MINEMU_UART0;
    const uint8_t *source = (const uint8_t *)(uintptr_t)address;
    for (uint32_t i = 0; i < length; ++i) {
        while ((device->status & MINEMU_UART_STATUS_TX_READY) == 0) {
            __asm__ volatile("nop");
        }
        device->tx_data = source[i];
    }
    return (int32_t)length;
}

static int32_t uart_status(uint32_t fd) {
    uint32_t result = 0;
    if (fd == MINEMU_FD_UART_IN) {
        if (uart_rx_available() ||
            (MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) != 0) {
            result |= MINEMU_IOCTL_UART_RX_READY;
        }
    } else if (fd == MINEMU_FD_UART_OUT || fd == MINEMU_FD_UART_DIAG) {
        volatile struct minemu_uart_regs *device =
            (fd == MINEMU_FD_UART_OUT) ? MINEMU_UART0 : MINEMU_UART1;
        if ((device->status & MINEMU_UART_STATUS_TX_READY) != 0) {
            result |= MINEMU_IOCTL_UART_TX_READY;
        }
    } else {
        return MINEMU_IOCTL_ERROR;
    }
    return (int32_t)result;
}

//rng operations
static int32_t rng_ioctl(uint32_t request, uint32_t argument) {
    switch (request) {
        case MINEMU_IOCTL_RNG_NEXT:
        //validate destination before accessing the random number
            if ((argument & 3U) != 0 ||
                !user_range_valid(argument, sizeof(uint32_t), true)) {
                return MINEMU_IOCTL_ERROR;
            }
            *(uint32_t *)(uintptr_t)argument = MINEMU_RNG->data;
            return MINEMU_IOCTL_SUCCESS;

        case MINEMU_IOCTL_RNG_STATE:
            //validate destination before accessing state
            if ((argument & 3U) != 0 ||
                !user_range_valid(argument, sizeof(uint32_t), true)) {
                return MINEMU_IOCTL_ERROR;
            }
            *(uint32_t *)(uintptr_t)argument = MINEMU_RNG->state;
            return MINEMU_IOCTL_SUCCESS;

        case MINEMU_IOCTL_RNG_SEED:
            MINEMU_RNG->seed = argument;
            return MINEMU_IOCTL_SUCCESS;

        default:
            return MINEMU_IOCTL_ERROR;
    }
}

//trace ioctl operations
static int32_t trace_ioctl(uint32_t request, uint32_t argument) {
    if (request != MINEMU_IOCTL_TRACE_EVENT) {
        return MINEMU_IOCTL_ERROR;
    }

    minemu_trace_event(argument);
    return MINEMU_IOCTL_SUCCESS;
}

//main dispatcher for ioctl syscalls
static int32_t kernel_ioctl(uint32_t fd, uint32_t request,
                            uint32_t argument0, uint32_t argument1) {
    switch (fd) {
        case MINEMU_FD_UART_IN:
        case MINEMU_FD_UART_OUT:
        case MINEMU_FD_UART_DIAG:
            switch (request) {
                case MINEMU_IOCTL_UART_READ:
                    return uart_read(fd, argument0, argument1);

                case MINEMU_IOCTL_UART_WRITE:
                    return uart_write(fd, argument0, argument1);

                case MINEMU_IOCTL_UART_STATUS:
                    return uart_status(fd);

                default:
                    return MINEMU_IOCTL_ERROR;
            }

        case MINEMU_FD_RNG:
            return rng_ioctl(request, argument0);

        case MINEMU_FD_TRACE:
            return trace_ioctl(request, argument0);

        default:
            return MINEMU_IOCTL_ERROR;
    }
}

struct minemu_trap_frame *minemu_svc_dispatch(struct minemu_trap_frame *frame) {
    uint32_t mode = frame->spsr & CPSR_MODE_MASK;

    if (frame->exception_id != MINEMU_EXCEPTION_SVC ||
        mode != CPSR_MODE_USR ||
        (frame->spsr & CPSR_THUMB) != 0) {
        frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
        return frame;
    }

   // Validate the SVC instruction and the user PC before dispatching the syscall.
    if ((frame->fault_pc & 3U) != 0 ||
        !user_pages_have_flags(frame->fault_pc, sizeof(uint32_t),
                               MINEMU_PTE_READABLE | MINEMU_PTE_EXECUTABLE)) {
        frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
        return frame;
    }
    uint32_t instruction = *(const uint32_t *)(uintptr_t)frame->fault_pc;
    if (instruction != A32_SVC_ZERO) {
        frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
        return frame;
    }

    uint32_t syscall_number = frame->r[7];
    switch (syscall_number) {
        case MINEMU_SYSCALL_IOCTL:
            frame->r[0] = (uint32_t)kernel_ioctl(
                frame->r[0], //fd
                frame->r[1], //request
                frame->r[2], //argument0
                frame->r[3]  //argument1
            );
            break;

        default:
            frame->r[0] = (uint32_t)MINEMU_IOCTL_ERROR;
            break;
    }
    return frame;
}
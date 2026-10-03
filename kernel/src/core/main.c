#include <stdint.h>

#include "minemu/boot.h"
#include "minemu/irq.h"
#include "minemu/platform.h"
#include "minemu/trap.h"
#include "minemu/trace.h"


#define UART_RX_BUFFER_SIZE UINT32_C(64) //shared state buffer for UART RX bytes
#define MSH_LINE_MAX UINT32_C(20) //max 20 bytes per line

static volatile uint8_t uart_rx_buffer[UART_RX_BUFFER_SIZE];
static volatile uint32_t uart_rx_head;
static volatile uint32_t uart_rx_tail;

static void uart_putc(char byte) {
    while ((MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY) == 0) {
        // Wait until UART0 is ready.
    }

    // Send one character over UART0.
    MINEMU_UART0->tx_data = (uint32_t)(uint8_t)byte;
}


static void uart_puts(const char *string) {
    while (*string != '\0') {
        uart_putc(*string);
        ++string;
    }
}

//interrupt handler for UART0 receive interrupts
static void uart0_irq_handler(void) {
    while ((MINEMU_UART0->status & MINEMU_UART_STATUS_RX_READY) != 0) {

        // Reading rx_data consumes one byte from UART hardware buffer.
        uint8_t byte = (uint8_t)MINEMU_UART0->rx_data;

        uint32_t next =
            (uart_rx_head + UINT32_C(1)) &
            (UART_RX_BUFFER_SIZE - UINT32_C(1));

        //store incoming byte, discard if buffer is full
        if (next != uart_rx_tail) {
            uart_rx_buffer[uart_rx_head] = byte;
            uart_rx_head = next;
        }
    }
}

//shared state access
static int uart_try_getc(uint8_t *byte) {
    int available = 0;
    //disable IRQs before accessing shared UART buffer state.
    __asm__ volatile("cpsid i" : : : "memory");
    if (uart_rx_head != uart_rx_tail) {
        //Retrieve one byte produced by the IRQ handler.
        *byte = uart_rx_buffer[uart_rx_tail];

        uart_rx_tail =
            (uart_rx_tail + UINT32_C(1))
            & (UART_RX_BUFFER_SIZE - UINT32_C(1));

        available = 1;
    }
    //reenable IRQs after shared-state access is finished.
    __asm__ volatile("cpsie i" : : : "memory");

    return available;
}


//dispatcher
struct minemu_trap_frame *
minemu_irq_dispatch(struct minemu_trap_frame *frame) {
    //determine interrupt source
    uint32_t source = (uint32_t)frame->exception_id;

    //dispatch interrupts to handler.
    if (source == MINEMU_IRQ_UART0) {
        uart0_irq_handler();
    }

  //write source id to eoi register to signal end-of-interrupt to interrupt controller
    MINEMU_INTERRUPT->eoi = source;
    return frame;
}

//msh interpreter
static void msh_execute(char *line) {
    char *command = line;
    uint32_t command_length = 0;
    //ignore leading spaces to find the first command word
    while (*command == ' ') {
        ++command;
    }
    //treat empty or all space command as no command
    if (*command == '\0') {
        return;
    }


    //command is the first ASCII word
    while (command[command_length] != '\0' &&
           command[command_length] != ' ') {
        ++command_length; //determine length of first command word
    }

    //echo command
    if (command_length == UINT32_C(4) &&
        command[0] == 'e' &&
        command[1] == 'c' &&
        command[2] == 'h' &&
        command[3] == 'o') {
        char *text = command + command_length;

        //skip spaces between echo and the text to be echoed
        while (*text == ' ') {
            ++text;
        }

        //echo text
        uart_puts(text);
        uart_putc('\n');

        return;
    }

    uart_puts("command not found: ");


    //print exactly the command word
    for (uint32_t i = 0; i < command_length; ++i) {
        uart_putc(command[i]);
    }
    uart_putc('\n');
}

static void msh_run(void) __attribute__((noreturn));

static void msh_run(void) {
    char line[MSH_LINE_MAX + UINT32_C(1)];
    uint32_t line_length = 0;
    //overflow counter
    uint32_t overflow_count = 0;
    line[0] = '\0';
    //print prompt for the first command
    uart_puts("msh> ");
    for (;;) {
        uint8_t byte;
        //wait for byte to be available from buffer
        if (!uart_try_getc(&byte)) {
            __asm__ volatile("nop");
            continue;
        }
        if (byte == (uint8_t)'\n') {
            //terminate the line with a null character so it can be treated as a C string
            line[line_length] = '\0';

           //process command
            msh_execute(line);

            //reset
            line_length = 0;
            overflow_count = 0;
            line[0] = '\0';

            //print prompt for the next command
            uart_puts("msh> ");

            continue;
        }


        //backspace handler
        if (byte == UINT8_C(0x08) || byte == UINT8_C(0x7f)) {
            //backspace first removes overflow
            if (overflow_count != 0) {
                --overflow_count;
            }
            //remove last character from line if line is not empty
            else if (line_length != 0) {
                --line_length;
                line[line_length] = '\0';
            }

            continue;
        }

        //overflow counter iterator
        if (overflow_count != 0) {
            ++overflow_count;
        }

        //store input
        else if (line_length < MSH_LINE_MAX) {
            line[line_length] = (char)byte;
            ++line_length;
            line[line_length] = '\0';
        }

        else {
            overflow_count = UINT32_C(1);
        }
    }
}


void minemu_kernel_main(const struct minemu_boot_info *boot_info) {
    if ((uintptr_t)boot_info != MINEMU_BOOT_INFO_VADDR ||
        boot_info->magic != MINEMU_BOOT_INFO_MAGIC ||
        boot_info->version != MINEMU_ABI_VERSION ||
        boot_info->size != sizeof(*boot_info) ||
        boot_info->system_rom_base != UINT32_C(0x08000000) ||
        boot_info->direct_map_vaddr != UINT32_C(0xc0000000) ||
        boot_info->direct_map_paddr != UINT32_C(0x40000000) ||
        boot_info->direct_map_size != UINT32_C(0x04000000)) {

        minemu_trace_event(UINT32_C(0xb007bad0));
        minemu_fail_stop();
    }

    minemu_trace_event(UINT32_C(1));
    uart_puts("hello world\n");


    //enable interrupts for UART0 in the interrupt controller
    MINEMU_INTERRUPT->enable =
        UINT32_C(1) << MINEMU_IRQ_UART0;

    MINEMU_UART0->control =
        MINEMU_UART_CONTROL_RX_IRQ_ENABLE;


    //enable IRQs globally
    __asm__ volatile("cpsie i" : : : "memory");
    msh_run();
}
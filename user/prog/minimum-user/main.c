
#include <stdint.h>
#include "minemu/user_abi.h"
#define MSH_LINE_MAX UINT32_C(25)

//write a complete buffer through the kernel
static int uart_write_all(const char *buffer, uint32_t length) {
    uint32_t written = 0;

    while (written < length) {
        uint32_t remaining = length - written;

        if (remaining > MINEMU_IOCTL_MAX_TRANSFER) {
            remaining = MINEMU_IOCTL_MAX_TRANSFER;
        }

        int32_t result = ioctl(
            MINEMU_FD_UART_OUT,
            MINEMU_IOCTL_UART_WRITE,
            (void *)(buffer + written),
            remaining
        );

        //stop if the syscall fails or makes no progress
        if (result <= 0 || (uint32_t)result > remaining) {
            return -1;
        }

        written += (uint32_t)result;
    }

    return 0;
}

//write a null-terminated string
static void uart_puts(const char *string) {
    uint32_t length = 0;

    while (string[length] != '\0') {
        ++length;
    }

    //send the string through the kernel
    (void)uart_write_all(string, length);
}

//read a character through the kernel
static int32_t uart_try_getc(uint8_t *byte) {
    return ioctl(
        MINEMU_FD_UART_IN,
        MINEMU_IOCTL_UART_READ,
        (void *)byte,
        UINT32_C(1)
    );
}

//msh interpreter
static void msh_execute(char *line) {
    char *command = line;
    uint32_t command_length = 0;

    //ignore leading spaces
    while (*command == ' ') {
        ++command;
    }

    //empty or all-space command
    if (*command == '\0') {
        return;
    }

    //find the first ASCII-space-delimited word
    while (command[command_length] != '\0' &&
           command[command_length] != ' ') {
        ++command_length;
    }

    //echo command
    if (command_length == UINT32_C(4) &&
        command[0] == 'e' &&
        command[1] == 'c' &&
        command[2] == 'h' &&
        command[3] == 'o') {

        char *text = command + command_length;

        //skip spaces between echo and its arguments
        while (*text == ' ') {
            ++text;
        }

        //collapse repeated spaces
        char *read = text;
        char *write = text;

        while (*read != '\0') {
            if (*read != ' ') {
                *write++ = *read;
            }
            else if (write != text && write[-1] != ' ') {
                *write++ = ' ';
            }

            ++read;
        }

        //remove trailing space
        if (write != text && write[-1] == ' ') {
            --write;
        }

        *write = '\0';

        //print normalized echo text
        uart_puts(text);
        uart_puts("\n");
        return;
    }

    //unknown command
    uart_puts("command not found: ");

    //print exactly the first command word
    (void)uart_write_all(command, command_length);
    uart_puts("\n");
}

//shell input loop
static void msh_run(void) {
    char line[MSH_LINE_MAX + UINT32_C(1)];
    uint32_t line_length = 0;
    uint32_t overflow_count = 0;

    line[0] = '\0';

    //print the initial prompt
    uart_puts("msh> ");

    for (;;) {
        uint8_t byte;

        //wait for incoming input
        if (uart_try_getc(&byte) != 1) {
            __asm__ volatile("nop");
            continue;
        }

        //newline terminates a command
        if (byte == (uint8_t)'\n') {
            line[line_length] = '\0';

            //do not execute truncated commands
            if (overflow_count == 0) {
                msh_execute(line);
            }

            //reset the input state
            line_length = 0;
            overflow_count = 0;
            line[0] = '\0';

            //print next prompt
            uart_puts("msh> ");
            continue;
        }

        //backspace handler
        if (byte == UINT8_C(0x08) ||
            byte == UINT8_C(0x7f)) {

            //remove overflow characters first
            if (overflow_count != 0) {
                --overflow_count;
            }
            else if (line_length != 0) {
                --line_length;
                line[line_length] = '\0';
            }

            continue;
        }

        //count input exceeding the line limit
        if (overflow_count != 0) {
            if (overflow_count != UINT32_MAX) {
                ++overflow_count;
            }
        }

        //store input while space remains
        else if (line_length < MSH_LINE_MAX) {
            line[line_length++] = (char)byte;
            line[line_length] = '\0';
        }

        //input has exceeded the limit
        else {
            overflow_count = UINT32_C(1);
        }
    }
}

//user-mode entry point
void minemu_user_main(void) {
    msh_run();
}

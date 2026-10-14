#include <stdint.h>
#include "minemu/user_abi.h"

#define MSH_LINE_MAX UINT32_C(20)

//write a character through the kernel
static void uart_putc(char byte) {
    (void)ioctl(MINEMU_FD_UART_OUT,
                MINEMU_IOCTL_UART_WRITE,
                (void *)&byte, UINT32_C(1));
}

//write a string
static void uart_puts(const char *string) {
    while (*string != '\0') {
        uart_putc(*string);
        ++string;
    }
}

//read a character through the kernel
static int uart_try_getc(uint8_t *byte) {
    return ioctl(MINEMU_FD_UART_IN,
                 MINEMU_IOCTL_UART_READ,
                 (void *)byte, UINT32_C(1));
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

//shell input loop
static void __attribute__((unused)) msh_run(void) {
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
        if (uart_try_getc(&byte) != 1) {
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

//user mode entry point
//user mode entry point
void minemu_user_main(void) {
    char prompt[] = "msh> ";

    int32_t result = ioctl(
        MINEMU_FD_UART_OUT,
        MINEMU_IOCTL_UART_WRITE,
        (void *)prompt,
        UINT32_C(5)
    );

    (void)result;

    for (;;) {
        __asm__ volatile("nop");
    }
}

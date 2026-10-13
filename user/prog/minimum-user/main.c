#include <stdint.h>
#include "minemu/user_abi.h"

void minemu_user_main(void) {
    char message[] = "hello world\n";

    uint32_t written = 0;
    uint32_t total = (uint32_t)(sizeof(message) - 1U);
    int32_t result = MINEMU_IOCTL_ERROR;

    //check if the entire message has been written, and if not, call ioctl() to write the remaining bytes
    while (written < total) {
        result = ioctl(MINEMU_FD_UART_OUT, MINEMU_IOCTL_UART_WRITE,
                       (void *)(message + written), (uint32_t)(total - written));
        if (result <= 0) {
            break;
        }
        written += (uint32_t)result;
    }

    for (;;) {
        __asm__ volatile("nop");
    }
}

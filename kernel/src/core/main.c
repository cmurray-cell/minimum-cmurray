#include "minemu/boot.h"
#include "minemu/trap.h"
#include "minemu/trace.h"

static void uart_character(char c) {
    while (!(MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY)) {
        // Wait until UART0 is ready.
    }
    if (MINEMU_UART0->status & MINEMU_UART_STATUS_TX_READY) {
        // send a single character
        MINEMU_UART0->tx_data = (uint32_t)(uint8_t)c;
    }
}

static void uart_string(const char *str) {
    while (*str != '\0') {
        uart_character(*str);
        str++;
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
    uart_string("hello worldn");
    
    minemu_trace_event(1);
    minemu_fail_stop();
}

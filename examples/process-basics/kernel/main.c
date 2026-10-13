#include <stddef.h>
#include <stdint.h>

#include "address_space.h"
#include "process.h"

#define TRACE_BOOT_FAILURE UINT32_C(0x505243f0)

void minemu_kernel_main(const struct example_boot_info *boot_info) {
    static const char initial_module[] = "process-parent";

    if ((uintptr_t)boot_info != EXAMPLE_BOOT_INFO_VADDR ||
        boot_info->magic != EXAMPLE_BOOT_INFO_MAGIC ||
        boot_info->version != EXAMPLE_ABI_VERSION ||
        boot_info->size != sizeof(*boot_info) ||
        boot_info->direct_map_vaddr != EXAMPLE_KERNEL_DIRECT_BASE ||
        boot_info->direct_map_paddr != EXAMPLE_RAM_BASE ||
        boot_info->direct_map_size != EXAMPLE_KERNEL_DIRECT_SIZE) {
        *(volatile uint32_t *)(uintptr_t)EXAMPLE_TRACE_BASE = TRACE_BOOT_FAILURE;
        minemu_fail_stop();
    }
    if (!address_space_system_init(boot_info)) {
        *(volatile uint32_t *)(uintptr_t)EXAMPLE_TRACE_BASE = TRACE_BOOT_FAILURE;
        minemu_fail_stop();
    }
    if (!process_init(initial_module, sizeof(initial_module) - 1U)) {
        *(volatile uint32_t *)(uintptr_t)EXAMPLE_TRACE_BASE = TRACE_BOOT_FAILURE;
        minemu_fail_stop();
    }
    process_start();
}

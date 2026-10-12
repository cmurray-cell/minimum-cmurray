#ifndef PROCESS_EXAMPLE_KERNEL_H
#define PROCESS_EXAMPLE_KERNEL_H

#include <stddef.h>
#include <stdint.h>

#define EXAMPLE_BOOT_INFO_MAGIC UINT32_C(0x4d424f4f)
#define EXAMPLE_ABI_VERSION UINT16_C(1)
#define EXAMPLE_BOOT_INFO_VADDR UINT32_C(0xc0007000)

#define EXAMPLE_SEGMENT_READABLE UINT32_C(1)
#define EXAMPLE_SEGMENT_WRITABLE UINT32_C(2)
#define EXAMPLE_SEGMENT_EXECUTABLE UINT32_C(4)

#define EXAMPLE_SYSTEM_ROM_BASE UINT32_C(0x08000000)
#define EXAMPLE_SYSTEM_ROM_SIZE UINT32_C(0x01000000)
#define EXAMPLE_RAM_BASE UINT32_C(0x40000000)
#define EXAMPLE_RAM_SIZE UINT32_C(0x04000000)
#define EXAMPLE_KERNEL_DIRECT_BASE UINT32_C(0xc0000000)
#define EXAMPLE_KERNEL_DIRECT_SIZE EXAMPLE_RAM_SIZE

#define EXAMPLE_INTERRUPT_BASE UINT32_C(0x10000000)
#define EXAMPLE_RNG_BASE UINT32_C(0x10003000)
#define EXAMPLE_UART0_BASE UINT32_C(0x10004000)
#define EXAMPLE_TRACE_BASE UINT32_C(0x1000f000)
#define EXAMPLE_UART_RX_READY UINT32_C(1)
#define EXAMPLE_UART_TX_READY UINT32_C(2)

#define EXAMPLE_PAGE_SIZE UINT32_C(4096)
#define EXAMPLE_PTE_VALID UINT32_C(1)
#define EXAMPLE_PTE_WRITABLE UINT32_C(2)
#define EXAMPLE_PTE_USER UINT32_C(4)
#define EXAMPLE_PTE_EXECUTABLE UINT32_C(8)
#define EXAMPLE_PTE_READABLE UINT32_C(16)
#define EXAMPLE_PTE_PAGE_MASK UINT32_C(0xfffff000)
#define EXAMPLE_PDE_VALID UINT32_C(1)

#define EXAMPLE_EXCEPTION_SVC (-3)

struct example_boot_info {
    uint32_t magic;
    uint16_t version;
    uint16_t size;
    uint32_t system_rom_base;
    uint32_t image_size;
    uint32_t module_table_offset;
    uint32_t module_count;
    uint32_t direct_map_vaddr;
    uint32_t direct_map_paddr;
    uint32_t direct_map_size;
    uint32_t flags;
    uint32_t reserved[6];
};

struct example_module_record {
    uint32_t name_offset;
    uint32_t name_length;
    uint32_t segment_table_offset;
    uint32_t segment_count;
    uint32_t entry_vaddr;
    uint32_t flags;
    uint32_t reserved[2];
};

struct example_module_segment {
    uint32_t data_offset;
    uint32_t virtual_address;
    uint32_t file_size;
    uint32_t memory_size;
    uint32_t flags;
    uint32_t reserved[3];
};

struct example_trap_frame {
    uint32_t r[13];
    uint32_t return_lr;
    uint32_t spsr;
    int32_t exception_id;
    uint32_t fault_pc;
    uint32_t dfsr;
    uint32_t dfar;
    uint32_t user_sp;
    uint32_t user_lr;
    uint32_t reserved;
};

_Static_assert(sizeof(struct example_boot_info) == 64, "boot-info size");
_Static_assert(sizeof(struct example_module_record) == 32, "module size");
_Static_assert(sizeof(struct example_module_segment) == 32, "segment size");
_Static_assert(sizeof(struct example_trap_frame) == 88, "trap-frame size");

void *memcpy(void *destination, const void *source, size_t length);
void *memset(void *destination, int value, size_t length);
void minemu_fail_stop(void) __attribute__((noreturn));

static inline void example_set_ttbr0(uint32_t address) {
    __asm__ volatile("mcr p15, 0, %0, c2, c0, 0" : : "r"(address) : "memory");
}

static inline void example_invalidate_tlb(void) {
    uint32_t ignored = 0;
    __asm__ volatile("mcr p15, 0, %0, c8, c7, 0" : : "r"(ignored) : "memory");
}

#endif

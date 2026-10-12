#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "address_space.h"

#define BOOTSTRAP_DIRECTORY_PADDR UINT32_C(0x40010000)
#define USER_IMAGE_BASE UINT32_C(0x00400000)
#define USER_STACK_BOTTOM UINT32_C(0x007ff000)
#define USER_STACK_TOP UINT32_C(0x00800000)
#define MAX_IMAGE_PAGES 16U
#define MAX_ADDRESS_SPACES 5U
#define SYSTEM_ROM_TABLES 4U

struct example_address_space {
    uint32_t directory[1024];
    uint32_t page_table[1024];
    uint8_t image_pages[MAX_IMAGE_PAGES][EXAMPLE_PAGE_SIZE];
    uint8_t stack_page[EXAMPLE_PAGE_SIZE];
    uint32_t image_vaddrs[MAX_IMAGE_PAGES];
    uint32_t image_flags[MAX_IMAGE_PAGES];
    size_t image_page_count;
    bool used;
} __attribute__((aligned(EXAMPLE_PAGE_SIZE)));

static struct example_address_space spaces[MAX_ADDRESS_SPACES]
    __attribute__((aligned(EXAMPLE_PAGE_SIZE)));
static uint32_t system_rom_tables[SYSTEM_ROM_TABLES][1024]
    __attribute__((aligned(EXAMPLE_PAGE_SIZE)));
static struct example_boot_info saved_boot_info;
static struct example_address_space *active_space;
static bool system_ready;

static uint32_t kernel_to_physical(const void *address) {
    return (uint32_t)(uintptr_t)address - EXAMPLE_KERNEL_DIRECT_BASE +
           EXAMPLE_RAM_BASE;
}

static volatile uint32_t *bootstrap_directory(void) {
    return (volatile uint32_t *)(uintptr_t)(EXAMPLE_KERNEL_DIRECT_BASE +
                                            BOOTSTRAP_DIRECTORY_PADDR -
                                            EXAMPLE_RAM_BASE);
}

static bool image_range_valid(uint32_t offset, uint32_t length) {
    return offset <= saved_boot_info.image_size &&
           length <= saved_boot_info.image_size - offset;
}

static bool records_valid(uint32_t offset, uint32_t count) {
    return offset <= saved_boot_info.image_size &&
           count <= (saved_boot_info.image_size - offset) / 32U;
}

static bool names_equal(const uint8_t *left, const char *right, size_t length) {
    for (size_t index = 0; index < length; ++index) {
        if (left[index] != (uint8_t)right[index]) {
            return false;
        }
    }
    return true;
}

bool address_space_system_init(const struct example_boot_info *boot_info) {
    if (boot_info == NULL || boot_info->system_rom_base != EXAMPLE_SYSTEM_ROM_BASE ||
        boot_info->image_size == 0 ||
        boot_info->image_size > EXAMPLE_SYSTEM_ROM_SIZE) {
        return false;
    }

    saved_boot_info = *boot_info;
    uint32_t pages =
        (boot_info->image_size + EXAMPLE_PAGE_SIZE - 1U) / EXAMPLE_PAGE_SIZE;
    uint32_t tables = (pages + 1023U) / 1024U;
    volatile uint32_t *directory = bootstrap_directory();
    for (uint32_t table = 0; table < tables; ++table) {
        memset(system_rom_tables[table], 0, sizeof(system_rom_tables[table]));
        uint32_t remaining = pages - table * 1024U;
        uint32_t entries = remaining < 1024U ? remaining : 1024U;
        for (uint32_t index = 0; index < entries; ++index) {
            uint32_t page = table * 1024U + index;
            system_rom_tables[table][index] =
                (EXAMPLE_SYSTEM_ROM_BASE + page * EXAMPLE_PAGE_SIZE) |
                EXAMPLE_PTE_VALID | EXAMPLE_PTE_READABLE;
        }
        directory[(EXAMPLE_SYSTEM_ROM_BASE >> 22) + table] =
            kernel_to_physical(system_rom_tables[table]) | EXAMPLE_PDE_VALID;
    }
    example_invalidate_tlb();
    for (size_t index = 0; index < MAX_ADDRESS_SPACES; ++index) {
        spaces[index].used = false;
    }
    system_ready = true;
    return true;
}

static const struct example_module_record *find_module(const char *name,
                                                        size_t name_length) {
    const uint8_t *image =
        (const uint8_t *)(uintptr_t)saved_boot_info.system_rom_base;
    if (!records_valid(saved_boot_info.module_table_offset,
                       saved_boot_info.module_count)) {
        return NULL;
    }
    const struct example_module_record *modules =
        (const struct example_module_record *)(image +
                                               saved_boot_info.module_table_offset);
    for (uint32_t index = 0; index < saved_boot_info.module_count; ++index) {
        const struct example_module_record *module = &modules[index];
        if (module->name_length == name_length &&
            image_range_valid(module->name_offset, module->name_length) &&
            names_equal(image + module->name_offset, name, name_length)) {
            return module;
        }
    }
    return NULL;
}

static struct example_address_space *space_allocate(void) {
    for (size_t index = 0; index < MAX_ADDRESS_SPACES; ++index) {
        if (!spaces[index].used) {
            spaces[index].image_page_count = 0;
            spaces[index].used = true;
            return &spaces[index];
        }
    }
    return NULL;
}

static uint32_t flags_from_segment(uint32_t flags) {
    uint32_t result = 0;
    if ((flags & EXAMPLE_SEGMENT_READABLE) != 0) {
        result |= EXAMPLE_PTE_READABLE;
    }
    if ((flags & EXAMPLE_SEGMENT_WRITABLE) != 0) {
        result |= EXAMPLE_PTE_WRITABLE;
    }
    if ((flags & EXAMPLE_SEGMENT_EXECUTABLE) != 0) {
        result |= EXAMPLE_PTE_EXECUTABLE;
    }
    return result;
}

static int image_page_index(struct example_address_space *space,
                            uint32_t page_vaddr, uint32_t flags, bool create) {
    for (size_t index = 0; index < space->image_page_count; ++index) {
        if (space->image_vaddrs[index] == page_vaddr) {
            if (create) {
                space->image_flags[index] |= flags;
            }
            return (int)index;
        }
    }
    if (!create || space->image_page_count == MAX_IMAGE_PAGES) {
        return -1;
    }
    size_t index = space->image_page_count++;
    space->image_vaddrs[index] = page_vaddr;
    space->image_flags[index] = flags;
    memset(space->image_pages[index], 0, EXAMPLE_PAGE_SIZE);
    return (int)index;
}

static bool prepare_range(struct example_address_space *space, uint32_t address,
                          uint32_t length, uint32_t flags) {
    uint32_t end = address + length;
    for (uint32_t page = address & EXAMPLE_PTE_PAGE_MASK; page < end;
         page += EXAMPLE_PAGE_SIZE) {
        if (image_page_index(space, page, flags, true) < 0) {
            return false;
        }
    }
    return true;
}

static bool write_loaded_range(struct example_address_space *space,
                               uint32_t address, const uint8_t *source,
                               uint32_t length) {
    while (length != 0) {
        uint32_t page = address & EXAMPLE_PTE_PAGE_MASK;
        int index = image_page_index(space, page, 0, false);
        if (index < 0) {
            return false;
        }
        uint32_t offset = address - page;
        uint32_t chunk = EXAMPLE_PAGE_SIZE - offset;
        if (chunk > length) {
            chunk = length;
        }
        if (source == NULL) {
            memset(&space->image_pages[index][offset], 0, chunk);
        } else {
            memcpy(&space->image_pages[index][offset], source, chunk);
            source += chunk;
        }
        address += chunk;
        length -= chunk;
    }
    return true;
}

static void rebuild_mappings(struct example_address_space *space) {
    const volatile uint32_t *bootstrap = bootstrap_directory();
    for (size_t index = 0; index < 1024U; ++index) {
        space->directory[index] = bootstrap[index];
    }
    memset(space->page_table, 0, sizeof(space->page_table));
    for (size_t index = 0; index < space->image_page_count; ++index) {
        uint32_t slot = (space->image_vaddrs[index] >> 12) & 1023U;
        space->page_table[slot] =
            kernel_to_physical(space->image_pages[index]) | EXAMPLE_PTE_VALID |
            EXAMPLE_PTE_USER | space->image_flags[index];
    }
    space->page_table[(USER_STACK_BOTTOM >> 12) & 1023U] =
        kernel_to_physical(space->stack_page) | EXAMPLE_PTE_VALID |
        EXAMPLE_PTE_USER | EXAMPLE_PTE_READABLE | EXAMPLE_PTE_WRITABLE;
    space->directory[USER_IMAGE_BASE >> 22] =
        kernel_to_physical(space->page_table) | EXAMPLE_PDE_VALID;
}

static bool load_module(struct example_address_space *space,
                        const struct example_module_record *module) {
    const uint8_t *image =
        (const uint8_t *)(uintptr_t)saved_boot_info.system_rom_base;
    if (module->segment_count == 0 ||
        !records_valid(module->segment_table_offset, module->segment_count) ||
        (module->entry_vaddr & 3U) != 0) {
        return false;
    }

    const struct example_module_segment *segments =
        (const struct example_module_segment *)(image +
                                                module->segment_table_offset);
    bool executable_entry = false;
    for (uint32_t index = 0; index < module->segment_count; ++index) {
        const struct example_module_segment *segment = &segments[index];
        uint32_t allowed = EXAMPLE_SEGMENT_READABLE | EXAMPLE_SEGMENT_WRITABLE |
                           EXAMPLE_SEGMENT_EXECUTABLE;
        if (segment->memory_size == 0 ||
            segment->file_size > segment->memory_size ||
            (segment->flags & ~allowed) != 0 ||
            segment->virtual_address < USER_IMAGE_BASE ||
            segment->virtual_address >= USER_STACK_BOTTOM ||
            segment->memory_size > USER_STACK_BOTTOM - segment->virtual_address ||
            !image_range_valid(segment->data_offset, segment->file_size)) {
            return false;
        }
        uint32_t flags = flags_from_segment(segment->flags);
        if (!prepare_range(space, segment->virtual_address,
                           segment->memory_size, flags) ||
            !write_loaded_range(space, segment->virtual_address, NULL,
                                segment->memory_size) ||
            !write_loaded_range(space, segment->virtual_address,
                                image + segment->data_offset,
                                segment->file_size)) {
            return false;
        }
        if ((segment->flags & EXAMPLE_SEGMENT_EXECUTABLE) != 0 &&
            segment->file_size >= 4U &&
            module->entry_vaddr >= segment->virtual_address &&
            module->entry_vaddr - segment->virtual_address <=
                segment->file_size - 4U) {
            executable_entry = true;
        }
    }
    return executable_entry;
}

struct example_address_space *address_space_load(const char *name,
                                                  size_t name_length,
                                                  uint32_t *entry) {
    if (!system_ready || entry == NULL) {
        return NULL;
    }
    const struct example_module_record *module = find_module(name, name_length);
    struct example_address_space *space = space_allocate();
    if (module == NULL || space == NULL || !load_module(space, module)) {
        if (space != NULL) {
            space->used = false;
        }
        return NULL;
    }
    memset(space->stack_page, 0, sizeof(space->stack_page));
    rebuild_mappings(space);
    *entry = module->entry_vaddr;
    return space;
}

struct example_address_space *address_space_clone(
    const struct example_address_space *source) {
    if (source == NULL || !source->used) {
        return NULL;
    }
    struct example_address_space *destination = space_allocate();
    if (destination == NULL) {
        return NULL;
    }
    destination->image_page_count = source->image_page_count;
    for (size_t index = 0; index < source->image_page_count; ++index) {
        destination->image_vaddrs[index] = source->image_vaddrs[index];
        destination->image_flags[index] = source->image_flags[index];
        memcpy(destination->image_pages[index], source->image_pages[index],
               EXAMPLE_PAGE_SIZE);
    }
    memcpy(destination->stack_page, source->stack_page, EXAMPLE_PAGE_SIZE);
    rebuild_mappings(destination);
    return destination;
}

void address_space_destroy(struct example_address_space *space) {
    if (space == NULL || space == active_space) {
        if (space == active_space) {
            minemu_fail_stop();
        }
        return;
    }
    space->used = false;
}

void address_space_activate(struct example_address_space *space) {
    if (space == NULL || !space->used) {
        minemu_fail_stop();
    }
    __asm__ volatile("dsb sy" : : : "memory");
    example_set_ttbr0(kernel_to_physical(space->directory));
    example_invalidate_tlb();
    __asm__ volatile("dsb sy\n\tisb" : : : "memory");
    active_space = space;
}

static bool range_has_flags(const struct example_address_space *space,
                            uint32_t address, uint32_t length,
                            uint32_t required) {
    if (space == NULL || !space->used) {
        return false;
    }
    if (length == 0) {
        return true;
    }
    if (address < USER_IMAGE_BASE || length > UINT32_MAX - address ||
        address + length > USER_STACK_TOP) {
        return false;
    }
    uint32_t final_page =
        (address + length - 1U) & EXAMPLE_PTE_PAGE_MASK;
    for (uint32_t page = address & EXAMPLE_PTE_PAGE_MASK;;
         page += EXAMPLE_PAGE_SIZE) {
        uint32_t pte = space->page_table[(page >> 12) & 1023U];
        uint32_t needed = EXAMPLE_PTE_VALID | EXAMPLE_PTE_USER | required;
        if ((pte & needed) != needed) {
            return false;
        }
        if (page == final_page) {
            return true;
        }
    }
}

bool address_space_readable(const struct example_address_space *space,
                            uint32_t address, uint32_t length) {
    return range_has_flags(space, address, length, EXAMPLE_PTE_READABLE);
}

bool address_space_writable(const struct example_address_space *space,
                            uint32_t address, uint32_t length) {
    return range_has_flags(space, address, length, EXAMPLE_PTE_WRITABLE);
}

static void *kernel_pointer(const struct example_address_space *space,
                            uint32_t address) {
    uint32_t pte = space->page_table[(address >> 12) & 1023U];
    uint32_t physical = (pte & EXAMPLE_PTE_PAGE_MASK) |
                        (address & (EXAMPLE_PAGE_SIZE - 1U));
    return (void *)(uintptr_t)(EXAMPLE_KERNEL_DIRECT_BASE + physical -
                               EXAMPLE_RAM_BASE);
}

static void copy_range(const struct example_address_space *space,
                       uint32_t user_address, uint8_t *kernel_bytes,
                       uint32_t length, bool to_user) {
    while (length != 0) {
        uint32_t chunk =
            EXAMPLE_PAGE_SIZE - (user_address & (EXAMPLE_PAGE_SIZE - 1U));
        if (chunk > length) {
            chunk = length;
        }
        void *user_bytes = kernel_pointer(space, user_address);
        if (to_user) {
            memcpy(user_bytes, kernel_bytes, chunk);
        } else {
            memcpy(kernel_bytes, user_bytes, chunk);
        }
        user_address += chunk;
        kernel_bytes += chunk;
        length -= chunk;
    }
}

bool address_space_copy_from(void *destination,
                             const struct example_address_space *space,
                             uint32_t source, uint32_t length) {
    if (!address_space_readable(space, source, length)) {
        return false;
    }
    copy_range(space, source, destination, length, false);
    return true;
}

bool address_space_copy_to(struct example_address_space *space,
                           uint32_t destination, const void *source,
                           uint32_t length) {
    if (!address_space_writable(space, destination, length)) {
        return false;
    }
    copy_range(space, destination, (uint8_t *)(uintptr_t)source, length, true);
    return true;
}

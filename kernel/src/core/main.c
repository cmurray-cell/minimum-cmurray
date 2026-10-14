
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#include "minemu/boot.h"
#include "minemu/irq.h"
#include "minemu/platform.h"
#include "minemu/trap.h"
#include "minemu/trace.h"
#include "minemu/user_abi.h"
#include "minemu/mmu.h"


#define UART_RX_BUFFER_SIZE UINT32_C(256) //shared state buffer for UART RX bytes
#define MSH_LINE_MAX UINT32_C(25) //max 25 bytes per line

//module memory and page-table configuration
#define PAGE_DIRECTORY UINT32_C(0x40010000)
#define USER_IMAGE UINT32_C(0x00400000)
#define USER_STACK_BOTTOM UINT32_C(0x007ff000)
#define USER_STACK_TOP UINT32_C(0x00800000)
#define MAX_USER_IMAGE_PAGES 16U
#define SYSTEM_ROM_TABLE_COUNT 4U

static uint32_t system_rom_tables[SYSTEM_ROM_TABLE_COUNT][1024]
    __attribute__((aligned(MINEMU_PAGE_SIZE)));
static uint32_t user_page_table[1024]
    __attribute__((aligned(MINEMU_PAGE_SIZE)));
static uint8_t user_image_pages[MAX_USER_IMAGE_PAGES][MINEMU_PAGE_SIZE]
    __attribute__((aligned(MINEMU_PAGE_SIZE)));
static uint8_t user_stack[MINEMU_PAGE_SIZE]
    __attribute__((aligned(MINEMU_PAGE_SIZE)));
static uint32_t user_page_vaddrs[MAX_USER_IMAGE_PAGES];
static uint32_t user_page_flags[MAX_USER_IMAGE_PAGES];
static size_t user_page_count;

void minemu_enter_user(uint32_t entry_vaddr, uint32_t stack_top)
    __attribute__((noreturn));

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
 int uart_try_getc(uint8_t *byte) {
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
    

//check rx buffer for available bytes without consuming input
int uart_rx_available(void) {
    int available;
    __asm__ volatile("cpsid i" : : : "memory");
    available = (uart_rx_head != uart_rx_tail);
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
static void __attribute__((unused)) msh_execute(char *line)  {
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



//static void msh_run(void) __attribute__((noreturn));

/*
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
*/

//Helpers

//zero a range of bytes in memory
static void zero_bytes(void *destination, size_t length) {
    uint8_t *bytes = destination;
    for (size_t i = 0; i < length; ++i) {
        bytes[i] = 0;
    }
}

//copy a range of bytes in memory
static void copy_bytes(void *destination, const void *source, size_t length) {
    uint8_t *out = destination;
    const uint8_t *in = source;
    for (size_t i = 0; i < length; ++i) {
        out[i] = in[i];
    }
}

//compare a range of bytes in memory to a string
static bool bytes_equal(const uint8_t *left, const char *right, size_t length) {
    for (size_t i = 0; i < length; ++i) {
        if (left[i] != (uint8_t)right[i]) {
            return false;
        }
    }
    return true;
}

//check that a range of bytes is valid within the system ROM image
static bool image_range_valid(uint32_t offset, uint32_t length,
                              uint32_t image_size) {
    return offset <= image_size && length <= image_size - offset;
}


//check that a range of records is valid within the system ROM image
static bool image_records_valid(uint32_t offset, uint32_t count,
                                uint32_t image_size) {
    return offset <= image_size && count <= ((image_size - offset) / 32U);
}


//convert a kernel virtual address to a physical address
static uint32_t kernel_vaddr_to_paddr(const void *address) {
    return (uint32_t)(uintptr_t)address - MINEMU_KERNEL_DIRECT_BASE + MINEMU_RAM_BASE;
}

//get pointer to the active page directory
static volatile uint32_t *active_page_directory(void) {
    return (volatile uint32_t *)(uintptr_t)(
        MINEMU_KERNEL_DIRECT_BASE + PAGE_DIRECTORY - MINEMU_RAM_BASE);
}

//map the system ROM image into the kernel page tables
static bool map_system_rom(const struct minemu_boot_info *boot_info) {
    //check that the system ROM image is valid and fits within the system ROM region
    if (boot_info->system_rom_base != MINEMU_SYSTEM_ROM_BASE ||
        boot_info->image_size == 0 ||
        boot_info->image_size > MINEMU_SYSTEM_ROM_SIZE) {
        return false;
    }

    //determine how many pages and page tables are needed to map the system ROM image
    uint32_t pages = (boot_info->image_size + MINEMU_PAGE_SIZE - 1U) /
                     MINEMU_PAGE_SIZE;
    uint32_t tables = (pages + 1023U) / 1024U;
    if (tables > SYSTEM_ROM_TABLE_COUNT) {
        return false;
    }

    //map each page table and fill in the page table entries for the system ROM image
    volatile uint32_t *directory = active_page_directory();
    for (uint32_t table = 0; table < tables; ++table) {
        zero_bytes(system_rom_tables[table], sizeof(system_rom_tables[table]));
        uint32_t remaining = pages - table * 1024U;
        uint32_t entries = remaining < 1024U ? remaining : 1024U;
        for (uint32_t index = 0; index < entries; ++index) {
            uint32_t page = table * 1024U + index;
            system_rom_tables[table][index] =
                (boot_info->system_rom_base + page * MINEMU_PAGE_SIZE) |
                MINEMU_PTE_VALID | MINEMU_PTE_READABLE;
        }
        directory[(boot_info->system_rom_base >> 22) + table] =
            kernel_vaddr_to_paddr(system_rom_tables[table]) | MINEMU_PDE_VALID;
    }
    minemu_mmu_tlbiall();
    return true;
}

//find a module by name in the system ROM image
static const struct minemu_module_record *find_module(
    const struct minemu_boot_info *boot_info, const char *name, size_t name_length) {
    const uint8_t *image = (const uint8_t *)(uintptr_t)boot_info->system_rom_base;

    //check that the module table is valid and fits within the system ROM image
    if (!image_records_valid(boot_info->module_table_offset,
                             boot_info->module_count, boot_info->image_size)) {
        return NULL;
    }

    //get pointer to the module table
    const struct minemu_module_record *modules =
        (const struct minemu_module_record *)(image + boot_info->module_table_offset);
    for (uint32_t i = 0; i < boot_info->module_count; ++i) {
        const struct minemu_module_record *module = &modules[i];
        if (module->name_length == name_length &&
            image_range_valid(module->name_offset, module->name_length,
                              boot_info->image_size) &&
            bytes_equal(image + module->name_offset, name, name_length)) {
            return module;
        }
    }
    return NULL;
}

//convert segment flags to page table entry flags
static uint32_t pte_flags_from_segment(uint32_t segment_flags) {
    uint32_t flags = 0;
    if ((segment_flags & MINEMU_SEGMENT_READABLE) != 0) {
        flags |= MINEMU_PTE_READABLE;
    }
    if ((segment_flags & MINEMU_SEGMENT_WRITABLE) != 0) {
        flags |= MINEMU_PTE_WRITABLE;
    }
    if ((segment_flags & MINEMU_SEGMENT_EXECUTABLE) != 0) {
        flags |= MINEMU_PTE_EXECUTABLE;
    }
    return flags;
}

//get the index of a user page in the user image pages array, optionally creating a new page if it doesn't exist
static int user_page_index(uint32_t page_vaddr, uint32_t flags, bool create) {
    for (size_t i = 0; i < user_page_count; ++i) {
        if (user_page_vaddrs[i] == page_vaddr) {
            if (create && user_page_flags[i] != flags) {
                return -1;
            }
            return (int)i;
        }
    }
    if (!create || user_page_count >= MAX_USER_IMAGE_PAGES) {
        return -1;
    }
    size_t index = user_page_count++;
    user_page_vaddrs[index] = page_vaddr;
    user_page_flags[index] = flags;
    zero_bytes(user_image_pages[index], MINEMU_PAGE_SIZE);
    return (int)index;
}

//prepare a range of user pages, optionally creating new pages if they don't exist
static bool prepare_user_range(uint32_t address, uint32_t length, uint32_t flags) {
    uint32_t end = address + length;
    uint32_t page = address & MINEMU_PTE_PAGE_MASK;
    while (page < end) {
        if (user_page_index(page, flags, true) < 0) {
            return false;
        }
        page += MINEMU_PAGE_SIZE;
    }
    return true;
}

//write a range of bytes to user pages, optionally zeroing the range if source is NULL
static bool write_user_range(uint32_t address, const uint8_t *source, uint32_t length) {
    //write bytes to user pages, optionally zeroing the range if source is NULL
    while (length != 0) {
        uint32_t base = address & MINEMU_PTE_PAGE_MASK;
        int index = user_page_index(base, 0, false);
        if (index < 0) {
            return false;
        }
        uint32_t offset = address - base;
        uint32_t chunk = MINEMU_PAGE_SIZE - offset;
        if (chunk > length) {
            chunk = length;
        }
        if (source == NULL) {
            zero_bytes(&user_image_pages[index][offset], chunk);
        } else {
            copy_bytes(&user_image_pages[index][offset], source, chunk);
            source += chunk;
        }
        address += chunk;
        length -= chunk;
    }
    return true;
}

//load a user module from the system ROM image into user pages and set up the user page table
static bool load_user_module(const struct minemu_boot_info *boot_info,
                             const struct minemu_module_record *module) {
    const uint8_t *image = (const uint8_t *)(uintptr_t)boot_info->system_rom_base;
    if (module->segment_count == 0 ||
        !image_records_valid(module->segment_table_offset,
                             module->segment_count, boot_info->image_size) ||
        (module->entry_vaddr & 3U) != 0) {
        return false;
    }

    //get pointer to the module's segment table
    const struct minemu_module_segment *segments =
        (const struct minemu_module_segment *)(image + module->segment_table_offset);
    bool executable_entry = false;
    zero_bytes(user_page_table, sizeof(user_page_table));
    zero_bytes(user_stack, sizeof(user_stack));
    user_page_count = 0;

    //load each segment into user pages and set up the page table entries
    for (uint32_t i = 0; i < module->segment_count; ++i) {
        const struct minemu_module_segment *segment = &segments[i];
        if (segment->memory_size == 0 ||
            segment->file_size > segment->memory_size ||
            (segment->flags & ~(MINEMU_SEGMENT_READABLE |
                                MINEMU_SEGMENT_WRITABLE |
                                MINEMU_SEGMENT_EXECUTABLE)) != 0 ||
            ((segment->flags & MINEMU_SEGMENT_EXECUTABLE) != 0 &&
             (segment->flags & MINEMU_SEGMENT_READABLE) == 0) ||
            segment->virtual_address < USER_IMAGE ||
            segment->virtual_address >= USER_STACK_BOTTOM ||
            segment->memory_size > USER_STACK_BOTTOM - segment->virtual_address ||
            !image_range_valid(segment->data_offset, segment->file_size,
                               boot_info->image_size)) {
            return false;
        }

        //convert segment flags to page table entry flags
        uint32_t flags = pte_flags_from_segment(segment->flags);
        if (!prepare_user_range(segment->virtual_address, segment->memory_size, flags) ||
            !write_user_range(segment->virtual_address, NULL, segment->memory_size) ||
            !write_user_range(segment->virtual_address, image + segment->data_offset,
                              segment->file_size)) {
            return false;
        }
        if ((segment->flags & MINEMU_SEGMENT_EXECUTABLE) != 0 &&
            segment->file_size >= 4U &&
            module->entry_vaddr >= segment->virtual_address &&
            module->entry_vaddr - segment->virtual_address <=
                segment->file_size - 4U) {
            executable_entry = true;
        }
    }
    if (!executable_entry) {
        return false;
    }

    //set up the user page table entries for the loaded pages and the user stack
    for (size_t i = 0; i < user_page_count; ++i) {
        uint32_t table_index = (user_page_vaddrs[i] >> 12) & 1023U;
        user_page_table[table_index] =
            kernel_vaddr_to_paddr(user_image_pages[i]) |
            MINEMU_PTE_VALID | MINEMU_PTE_USER | user_page_flags[i];
    }

    //set up the user page table entry for the user stack
    user_page_table[(USER_STACK_BOTTOM >> 12) & 1023U] =
        kernel_vaddr_to_paddr(user_stack) |
        MINEMU_PTE_VALID | MINEMU_PTE_USER |
        MINEMU_PTE_READABLE | MINEMU_PTE_WRITABLE;

    //set up the user page directory entry for the user page table and flush the TLB
    active_page_directory()[USER_IMAGE >> 22] =
        kernel_vaddr_to_paddr(user_page_table) | MINEMU_PDE_VALID;
    minemu_mmu_tlbiall();
    return true;
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
 
    //enable interrupts for UART0 in the interrupt controller
    MINEMU_INTERRUPT->enable =
        UINT32_C(1) << MINEMU_IRQ_UART0;

    MINEMU_UART0->control =
        MINEMU_UART_CONTROL_RX_IRQ_ENABLE;

    //enable IRQs globally
    __asm__ volatile("cpsie i" : : : "memory");

    static const char module_name[] = "minimum-user";
    if (!map_system_rom(boot_info)) {
        minemu_trace_event(UINT32_C(0xb007bad1));
        minemu_fail_stop();
    }

    const struct minemu_module_record *module =
        find_module(boot_info, module_name, sizeof(module_name) - 1U);
    if (module == NULL || !load_user_module(boot_info, module)) {
        minemu_trace_event(UINT32_C(0xb007bad2));
        minemu_fail_stop();
    }

    minemu_enter_user(module->entry_vaddr, USER_STACK_TOP);
}

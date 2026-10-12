#ifndef PROCESS_EXAMPLE_ADDRESS_SPACE_H
#define PROCESS_EXAMPLE_ADDRESS_SPACE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "example_kernel.h"

struct example_address_space;

bool address_space_system_init(const struct example_boot_info *boot_info);
struct example_address_space *address_space_load(const char *name,
                                                  size_t name_length,
                                                  uint32_t *entry);
struct example_address_space *address_space_clone(
    const struct example_address_space *source);
void address_space_destroy(struct example_address_space *space);
void address_space_activate(struct example_address_space *space);
bool address_space_readable(const struct example_address_space *space,
                            uint32_t address, uint32_t length);
bool address_space_writable(const struct example_address_space *space,
                            uint32_t address, uint32_t length);
bool address_space_copy_from(void *destination,
                             const struct example_address_space *space,
                             uint32_t source, uint32_t length);
bool address_space_copy_to(struct example_address_space *space,
                           uint32_t destination, const void *source,
                           uint32_t length);

#endif

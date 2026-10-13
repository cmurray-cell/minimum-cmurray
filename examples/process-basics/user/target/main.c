#include <stdint.h>

#include "process_abi.h"

#define TRACE_TARGET UINT32_C(0x50524301)

void process_user_main(void) {
    (void)ioctl(PROCESS_FD_TRACE, PROCESS_IOCTL_TRACE_EVENT, TRACE_TARGET);
    _exit(37);
}

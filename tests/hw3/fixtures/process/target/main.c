#include <stdint.h>
#include <unistd.h>

#include "minemu/user_abi.h"

void minemu_user_main(void)
{
  (void)ioctl(MINEMU_FD_TRACE, MINEMU_IOCTL_TRACE_EVENT,
              UINT32_C(0xa3010020));
  _exit(7);
}

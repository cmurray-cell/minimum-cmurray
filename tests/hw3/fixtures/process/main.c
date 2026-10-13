#include <stdint.h>
#include <sys/wait.h>
#include <unistd.h>

#include "minemu/user_abi.h"

static volatile uint32_t writable = UINT32_C(0x13579bdf);

static void trace(uint32_t value)
{
  (void)ioctl(MINEMU_FD_TRACE, MINEMU_IOCTL_TRACE_EVENT, value);
}

static _Noreturn void stop(void)
{
  __asm__ volatile("bkpt #0");
  while (1);
}

static _Noreturn void fail(uint32_t check)
{
  trace(UINT32_C(0xa3ff0000) | check);
  stop();
}

void minemu_user_main(void)
{
  static const char missing[] = "hw3-no-such-module";
  static const char target[] = "hw3-process-target";
  int status = -1;

  trace(UINT32_C(0xa3010001));
  pid_t child = fork();
  if (child < 0) {
    fail(1);
  }

  if (child == 0) {
    if (writable != UINT32_C(0x13579bdf)) {
      fail(2);
    }
    trace(UINT32_C(0xa3010010));
    writable = UINT32_C(0xc1c1c1c1);
    if (exec(missing) >= 0 || writable != UINT32_C(0xc1c1c1c1)) {
      fail(3);
    }
    trace(UINT32_C(0xa3010011));
    if (exec(target) >= 0) {
      fail(4);
    }
    fail(5);
  }

  writable = UINT32_C(0xa5a5a5a5);
  trace(UINT32_C(0xa3010002));
  pid_t waited = wait(child, &status);
  if (waited != child) {
    fail(6);
  }
  if (status != 7) {
    fail(7);
  }
  if (writable != UINT32_C(0xa5a5a5a5)) {
    fail(8);
  }
  trace(UINT32_C(0xa30100ff));
  stop();
}

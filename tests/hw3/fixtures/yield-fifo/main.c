#include <stdint.h>
#include <sched.h>
#include <sys/wait.h>
#include <unistd.h>

#include "minemu/user_abi.h"

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

static _Noreturn void child_one(void)
{
  trace(UINT32_C(0xa3020011));
  if (sched_yield() != 0) {
    fail(3);
  }
  trace(UINT32_C(0xa3020012));
  _exit(11);
}

static _Noreturn void child_two(void)
{
  trace(UINT32_C(0xa3020021));
  if (sched_yield() != 0) {
    fail(4);
  }
  trace(UINT32_C(0xa3020022));
  _exit(22);
}

void minemu_user_main(void)
{
  int status = -1;

  trace(UINT32_C(0xa3020001));
  pid_t first = fork();
  if (first < 0) {
    fail(1);
  }
  if (first == 0) {
    child_one();
  }

  pid_t second = fork();
  if (second < 0) {
    fail(2);
  }
  if (second == 0) {
    child_two();
  }

  trace(UINT32_C(0xa3020002));
  if (sched_yield() != 0) {
    fail(5);
  }
  trace(UINT32_C(0xa3020003));

  if (wait(first, &status) != first || status != 11) {
    fail(6);
  }
  status = -1;
  if (wait(second, &status) != second || status != 22) {
    fail(7);
  }

  trace(UINT32_C(0xa30200ff));
  stop();
}

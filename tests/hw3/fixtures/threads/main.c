#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>

#include "minemu/user_abi.h"

static uint32_t argument_a = UINT32_C(0xaaaa0001);
static uint32_t argument_b = UINT32_C(0xbbbb0002);
static uint32_t result_a = UINT32_C(0xaaaa1001);
static uint32_t result_b = UINT32_C(0xbbbb2002);

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

static void *thread_a(void *argument)
{
  if (argument != &argument_a || *(uint32_t *)argument != UINT32_C(0xaaaa0001)) {
    fail(4);
  }
  trace(UINT32_C(0xa3030011));
  if (sched_yield() != 0) {
    fail(5);
  }
  trace(UINT32_C(0xa3030012));
  return &result_a;
}

static void *thread_b(void *argument)
{
  if (argument != &argument_b || *(uint32_t *)argument != UINT32_C(0xbbbb0002)) {
    fail(6);
  }
  trace(UINT32_C(0xa3030021));
  if (sched_yield() != 0) {
    fail(7);
  }
  trace(UINT32_C(0xa3030022));
  pthread_exit(&result_b);
}

void minemu_user_main(void)
{
  pthread_t a;
  pthread_t b;
  void *result = 0;

  trace(UINT32_C(0xa3030001));
  if (pthread_create(0, thread_a, &argument_a) != -EFAULT) {
    fail(1);
  }
  if (pthread_create(&a, thread_a, &argument_a) != 0) {
    fail(2);
  }
  if (pthread_create(&b, thread_b, &argument_b) != 0) {
    fail(3);
  }

  trace(UINT32_C(0xa3030002));
  if (sched_yield() != 0) {
    fail(8);
  }
  trace(UINT32_C(0xa3030003));

  if (pthread_join(a, &result) != 0 || result != &result_a) {
    fail(9);
  }
  trace(UINT32_C(0xa3030004));
  result = 0;
  if (pthread_join(b, &result) != 0 || result != &result_b) {
    fail(10);
  }
  trace(UINT32_C(0xa3030005));

  trace(UINT32_C(0xa30300ff));
  stop();
}

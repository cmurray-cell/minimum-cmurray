#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "process.h"
#include "process_abi.h"

#define USER_STACK_TOP UINT32_C(0x00800000)
#define CPSR_MODE_USR UINT32_C(0x10)
#define MAX_EXEC_NAME 64U
#define MAX_RUNNABLE_THREADS \
    (EXAMPLE_MAX_PROCESSES * EXAMPLE_MAX_THREADS_PER_PROCESS)

static struct example_process processes[EXAMPLE_MAX_PROCESSES];
static struct example_thread *run_queue[MAX_RUNNABLE_THREADS];
static size_t queue_head;
static size_t queue_count;
static int32_t next_pid = 1;
static struct example_thread *current_thread;
static struct example_address_space *deferred_space;

static struct example_trap_frame *thread_frame(struct example_thread *thread) {
    uintptr_t top = (uintptr_t)thread->kernel_stack +
                    sizeof(thread->kernel_stack);
    top &= ~(uintptr_t)7U;
    return (struct example_trap_frame *)(top -
                                         sizeof(struct example_trap_frame));
}

static void initialize_frame(struct example_thread *thread, uint32_t entry) {
    thread->frame = thread_frame(thread);
    memset(thread->frame, 0, sizeof(*thread->frame));
    thread->frame->return_lr = entry;
    thread->frame->spsr = CPSR_MODE_USR;
    thread->frame->exception_id = EXAMPLE_EXCEPTION_SVC;
    thread->frame->user_sp = USER_STACK_TOP;
}

/* FIFO runnable queue primitives: push at tail, pop at head. */
static bool queue_push(struct example_thread *thread) {
    if (queue_count == MAX_RUNNABLE_THREADS || thread->state != READY) {
        return false;
    }
    size_t tail = (queue_head + queue_count) % MAX_RUNNABLE_THREADS;
    run_queue[tail] = thread;
    ++queue_count;
    return true;
}

static struct example_thread *queue_pop(void) {
    if (queue_count == 0) {
        return NULL;
    }
    struct example_thread *thread = run_queue[queue_head];
    queue_head = (queue_head + 1U) % MAX_RUNNABLE_THREADS;
    --queue_count;
    return thread;
}

static void queue_remove_process(struct example_process *process) {
    size_t original_count = queue_count;
    for (size_t index = 0; index < original_count; ++index) {
        struct example_thread *thread = queue_pop();
        if (thread->process != process && !queue_push(thread)) {
            minemu_fail_stop();
        }
    }
}

static bool pid_in_use(int32_t pid) {
    for (size_t index = 0; index < EXAMPLE_MAX_PROCESSES; ++index) {
        if (processes[index].state != PROCESS_FREE &&
            processes[index].pid == pid) {
            return true;
        }
    }
    return false;
}

static int32_t allocate_pid(void) {
    for (uint32_t attempt = 0; attempt < UINT32_C(0xffff); ++attempt) {
        int32_t candidate = next_pid;
        next_pid = candidate == INT32_C(0xffff) ? 1 : candidate + 1;
        if (!pid_in_use(candidate)) {
            return candidate;
        }
    }
    return 0;
}

static struct example_process *allocate_process(void) {
    for (size_t index = 0; index < EXAMPLE_MAX_PROCESSES; ++index) {
        if (processes[index].state == PROCESS_FREE) {
            int32_t pid = allocate_pid();
            if (pid == 0) {
                return NULL;
            }
            processes[index].state = LIVE;
            processes[index].pid = pid;
            processes[index].exit_status = 0;
            processes[index].parent = NULL;
            processes[index].waiter = NULL;
            processes[index].wait_status_address = 0;
            processes[index].space = NULL;
            for (size_t thread = 0;
                 thread < EXAMPLE_MAX_THREADS_PER_PROCESS; ++thread) {
                processes[index].threads[thread].state = FREE;
            }
            return &processes[index];
        }
    }
    return NULL;
}

static struct example_thread *allocate_thread(struct example_process *process) {
    for (size_t index = 0; index < EXAMPLE_MAX_THREADS_PER_PROCESS; ++index) {
        struct example_thread *thread = &process->threads[index];
        if (thread->state == FREE) {
            thread->block_reason = BLOCK_NONE;
            thread->process = process;
            thread->frame = NULL;
            thread->tid = ((uint32_t)process->pid << 16) | (uint32_t)(index + 1U);
            return thread;
        }
    }
    return NULL;
}

static size_t allocated_thread_count(const struct example_process *process) {
    size_t count = 0;
    for (size_t index = 0; index < EXAMPLE_MAX_THREADS_PER_PROCESS; ++index) {
        if (process->threads[index].state != FREE) {
            ++count;
        }
    }
    return count;
}

static void release_deferred_space(void) {
    if (deferred_space != NULL) {
        address_space_destroy(deferred_space);
        deferred_space = NULL;
    }
}

static struct example_trap_frame *dispatch_next(void) {
    struct example_thread *next = queue_pop();
    if (next == NULL || next->state != READY) {
        minemu_fail_stop();
    }
    if (current_thread == NULL || next->process != current_thread->process) {
        address_space_activate(next->process->space);
    }
    current_thread = next;
    current_thread->state = RUNNING;
    release_deferred_space();
    return current_thread->frame;
}

static void reap_process(struct example_process *process) {
    process->state = PROCESS_FREE;
    process->parent = NULL;
    process->waiter = NULL;
    process->space = NULL;
}

static void orphan_children(struct example_process *parent) {
    for (size_t index = 0; index < EXAMPLE_MAX_PROCESSES; ++index) {
        struct example_process *child = &processes[index];
        if (child->state != PROCESS_FREE && child->parent == parent) {
            child->parent = NULL;
            child->waiter = NULL;
            child->wait_status_address = 0;
            if (child->state == ZOMBIE) {
                reap_process(child);
            }
        }
    }
}

static void wake_waiter(struct example_process *child) {
    struct example_thread *waiter = child->waiter;
    if (waiter == NULL) {
        return;
    }
    bool copied = child->wait_status_address == 0 ||
                  address_space_copy_to(waiter->process->space,
                                        child->wait_status_address,
                                        &child->exit_status,
                                        sizeof(child->exit_status));
    child->waiter = NULL;
    child->wait_status_address = 0;
    waiter->frame->r[0] = copied ? (uint32_t)child->pid :
                                  (uint32_t)-EFAULT;
    waiter->block_reason = BLOCK_NONE;
    waiter->state = READY;
    if (!queue_push(waiter)) {
        minemu_fail_stop();
    }
    if (copied) {
        reap_process(child);
    }
}

/* _exit is process-wide: every TCB is removed or marked EXITED. */
static void terminate_process(struct example_process *process, int status) {
    queue_remove_process(process);
    for (size_t index = 0; index < EXAMPLE_MAX_THREADS_PER_PROCESS; ++index) {
        if (process->threads[index].state != FREE) {
            process->threads[index].state = EXITED;
        }
    }
    orphan_children(process);
    process->exit_status = status;
    process->state = ZOMBIE;
    if (deferred_space != NULL) {
        minemu_fail_stop();
    }
    deferred_space = process->space;
    process->space = NULL;
    if (process->waiter != NULL) {
        wake_waiter(process);
    } else if (process->parent == NULL || process->parent->state != LIVE) {
        reap_process(process);
    }
}

static struct example_process *find_child(struct example_process *parent,
                                           int32_t pid) {
    for (size_t index = 0; index < EXAMPLE_MAX_PROCESSES; ++index) {
        if (processes[index].state != PROCESS_FREE &&
            processes[index].pid == pid && processes[index].parent == parent) {
            return &processes[index];
        }
    }
    return NULL;
}

static struct example_trap_frame *sys_fork(struct example_trap_frame *frame) {
    struct example_process *parent = current_thread->process;
    if (allocated_thread_count(parent) != 1U) {
        frame->r[0] = (uint32_t)-EAGAIN;
        return frame;
    }
    struct example_address_space *space = address_space_clone(parent->space);
    if (space == NULL) {
        frame->r[0] = (uint32_t)-ENOMEM;
        return frame;
    }
    struct example_process *child = allocate_process();
    if (child == NULL) {
        address_space_destroy(space);
        frame->r[0] = (uint32_t)-EAGAIN;
        return frame;
    }
    child->parent = parent;
    child->space = space;
    struct example_thread *thread = allocate_thread(child);
    if (thread == NULL) {
        child->state = PROCESS_FREE;
        address_space_destroy(space);
        frame->r[0] = (uint32_t)-EAGAIN;
        return frame;
    }
    thread->frame = thread_frame(thread);
    memcpy(thread->frame, frame, sizeof(*frame));
    thread->frame->r[0] = 0;
    thread->state = READY;
    if (!queue_push(thread)) {
        child->state = PROCESS_FREE;
        address_space_destroy(space);
        frame->r[0] = (uint32_t)-EAGAIN;
        return frame;
    }
    frame->r[0] = (uint32_t)child->pid;
    return frame;
}

static struct example_trap_frame *sys_exec(struct example_trap_frame *frame) {
    struct example_process *process = current_thread->process;
    if (allocated_thread_count(process) != 1U) {
        frame->r[0] = (uint32_t)-EINVAL;
        return frame;
    }
    char name[MAX_EXEC_NAME];
    size_t length = 0;
    for (; length < sizeof(name); ++length) {
        if (!address_space_copy_from(&name[length], process->space,
                                     frame->r[0] + (uint32_t)length, 1U)) {
            frame->r[0] = (uint32_t)-EFAULT;
            return frame;
        }
        if (name[length] == '\0') {
            break;
        }
    }
    if (length == 0 || length == sizeof(name)) {
        frame->r[0] = (uint32_t)-EINVAL;
        return frame;
    }
    uint32_t entry;
    struct example_address_space *new_space =
        address_space_load(name, length, &entry);
    if (new_space == NULL) {
        frame->r[0] = (uint32_t)-ENOENT;
        return frame;
    }
    struct example_address_space *old_space = process->space;
    process->space = new_space;
    address_space_activate(new_space);
    address_space_destroy(old_space);
    initialize_frame(current_thread, entry);
    return current_thread->frame;
}

static struct example_trap_frame *sys_wait(struct example_trap_frame *frame) {
    int32_t pid = (int32_t)frame->r[0];
    uint32_t status_address = frame->r[1];
    struct example_process *parent = current_thread->process;
    if (pid <= 0) {
        frame->r[0] = (uint32_t)-EINVAL;
        return frame;
    }
    struct example_process *child = find_child(parent, pid);
    if (child == NULL) {
        frame->r[0] = (uint32_t)-ECHILD;
        return frame;
    }
    /* A zero address is wait(pid, NULL), which is explicitly valid. */
    if (status_address != 0 &&
        !address_space_writable(parent->space, status_address, sizeof(int))) {
        frame->r[0] = (uint32_t)-EFAULT;
        return frame;
    }
    if (child->state == ZOMBIE) {
        if (status_address != 0 &&
            !address_space_copy_to(parent->space, status_address,
                                   &child->exit_status, sizeof(int))) {
            frame->r[0] = (uint32_t)-EFAULT;
            return frame;
        }
        frame->r[0] = (uint32_t)pid;
        reap_process(child);
        return frame;
    }
    if (child->waiter != NULL) {
        frame->r[0] = (uint32_t)-EINVAL;
        return frame;
    }
    child->waiter = current_thread;
    child->wait_status_address = status_address;
    current_thread->block_reason = BLOCK_WAIT;
    current_thread->state = BLOCKED;
    return dispatch_next();
}

static struct example_trap_frame *sys_sched_yield(
    struct example_trap_frame *frame) {
    /*
     * STUDENT TODO (HW3 scheduling task): enqueue the running TCB as READY,
     * select the FIFO head, activate its address space if its process differs,
     * and return its frame.
     * The instructor implementation is intentionally not included here.
     */
    frame->r[0] = (uint32_t)-ENOSYS;
    return frame;
}

bool process_init(const char *module_name, size_t name_length) {
    struct example_process *process = allocate_process();
    if (process == NULL) {
        return false;
    }
    uint32_t entry;
    process->space = address_space_load(module_name, name_length, &entry);
    if (process->space == NULL) {
        process->state = PROCESS_FREE;
        return false;
    }
    struct example_thread *thread = allocate_thread(process);
    if (thread == NULL) {
        address_space_destroy(process->space);
        process->state = PROCESS_FREE;
        return false;
    }
    initialize_frame(thread, entry);
    thread->state = RUNNING;
    current_thread = thread;
    address_space_activate(process->space);
    return true;
}

void process_start(void) {
    process_enter_frame(current_thread->frame);
}

struct example_address_space *process_current_space(void) {
    return current_thread == NULL ? NULL : current_thread->process->space;
}

struct example_trap_frame *process_handle_syscall(
    struct example_trap_frame *frame, uint32_t syscall_number) {
    if (current_thread == NULL || current_thread->state != RUNNING) {
        minemu_fail_stop();
    }
    current_thread->frame = frame;
    switch (syscall_number) {
    case PROCESS_SYSCALL_FORK:
        return sys_fork(frame);
    case PROCESS_SYSCALL_EXEC:
        return sys_exec(frame);
    case PROCESS_SYSCALL_WAIT:
        return sys_wait(frame);
    case PROCESS_SYSCALL_EXIT:
        terminate_process(current_thread->process, (int)frame->r[0]);
        return dispatch_next();
    case PROCESS_SYSCALL_SCHED_YIELD:
        return sys_sched_yield(frame);
    default:
        /* pthread create/join/exit intentionally have no kernel implementation. */
        frame->r[0] = (uint32_t)-ENOSYS;
        return frame;
    }
}

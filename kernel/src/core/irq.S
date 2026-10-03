.syntax unified
.cpu cortex-a9
.arm

.global minemu_irq_trampoline
.type minemu_irq_trampoline, %function
.extern minemu_irq_dispatch
@taken from example to be used in main
minemu_irq_trampoline:
    sub sp, sp, #88
    stmia sp, {r0-r12}
    str lr, [sp, #52]
    mrs r1, spsr
    str r1, [sp, #56]
    ldr r1, =0x10000000
    ldr r1, [r1, #8]
    str r1, [sp, #60]
    sub r1, lr, #4
    str r1, [sp, #64]
    mov r1, #0
    str r1, [sp, #68]
    str r1, [sp, #72]
    add r1, sp, #76
    stmia r1, {sp, lr}^
    mov r1, #0
    str r1, [sp, #84]
    mov r0, sp
    bl minemu_irq_dispatch
    mov sp, r0
    ldr lr, [sp, #52]
    ldr r1, [sp, #56]
    msr spsr_cxsf, r1
    add r1, sp, #76
    ldmia r1, {sp, lr}^
    ldmia sp, {r0-r12}
    add sp, sp, #88
    subs pc, lr, #4

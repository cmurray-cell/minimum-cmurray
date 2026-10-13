.syntax unified
.cpu cortex-a9
.arm

.global minemu_svc_trampoline
.type minemu_svc_trampoline, %function
.extern process_svc_dispatch

minemu_svc_trampoline:
    sub sp, sp, #88
    stmia sp, {r0-r12}
    str lr, [sp, #52]
    mrs r1, spsr
    str r1, [sp, #56]
    ldr r1, =-3
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
    bl process_svc_dispatch
    b process_restore_frame

.global process_enter_frame
.type process_enter_frame, %function
process_enter_frame:
    cpsid if

process_restore_frame:
    mov sp, r0
    ldr lr, [sp, #52]
    ldr r1, [sp, #56]
    msr spsr_cxsf, r1
    add r1, sp, #76
    ldmia r1, {sp, lr}^
    ldmia sp, {r0-r12}
    add sp, sp, #88
    movs pc, lr

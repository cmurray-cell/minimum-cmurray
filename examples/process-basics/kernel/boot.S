.syntax unified
.cpu cortex-a9
.arm

.section .bootstrap.text, "ax", %progbits
.global minemu_bootstrap
.type minemu_bootstrap, %function
.extern minemu_vectors
.extern minemu_kernel_main
.extern minemu_fail_stop

minemu_bootstrap:
    mov r8, r0
    cpsid if

    ldr r0, =__minemu_svc_stack_top_paddr
    msr cpsr_c, #0xd3
    mov sp, r0
    ldr r0, =__minemu_irq_stack_top_paddr
    msr cpsr_c, #0xd2
    mov sp, r0
    ldr r0, =__minemu_abt_stack_top_paddr
    msr cpsr_c, #0xd7
    mov sp, r0
    ldr r0, =__minemu_und_stack_top_paddr
    msr cpsr_c, #0xdb
    mov sp, r0
    msr cpsr_c, #0xd3

    bl minemu_bootstrap_map
    ldr r0, =0x40010000
    mcr p15, 0, r0, c2, c0, 0
    mov r0, #0
    mcr p15, 0, r0, c8, c7, 0
    mov r0, #1
    mcr p15, 0, r0, c1, c0, 0
    ldr r0, =minemu_vectors
    mcr p15, 0, r0, c12, c0, 0
    ldr r0, =minemu_bootstrap_high
    bx r0

minemu_bootstrap_map:
    ldr r0, =0x40010000
    mov r1, #0
    mov r2, #1024
1:
    str r1, [r0], #4
    subs r2, r2, #1
    bne 1b

    ldr r0, =0x40011000
    ldr r1, =0x4000001b
    mov r2, #1024
2:
    str r1, [r0], #4
    add r1, r1, #4096
    subs r2, r2, #1
    bne 2b
    ldr r0, =0x40010400
    ldr r1, =0x40011001
    str r1, [r0]

    ldr r3, =0x40010c00
    ldr r0, =0x40012000
    ldr r1, =0x4000001b
    mov r4, #16
3:
    orr r5, r0, #1
    str r5, [r3], #4
    mov r2, #1024
4:
    str r1, [r0], #4
    add r1, r1, #4096
    subs r2, r2, #1
    bne 4b
    subs r4, r4, #1
    bne 3b

    /* Identity-map the implemented supervisor MMIO window as non-executable. */
    ldr r0, =0x40022000
    mov r1, #0
    mov r2, #1024
5:
    str r1, [r0], #4
    subs r2, r2, #1
    bne 5b
    ldr r0, =0x40022000
    ldr r1, =0x10000013
    mov r2, #16
6:
    str r1, [r0], #4
    add r1, r1, #4096
    subs r2, r2, #1
    bne 6b
    ldr r0, =0x40010100
    ldr r1, =0x40022001
    str r1, [r0]
    bx lr

.section .text.startup, "ax", %progbits
.global minemu_bootstrap_high
.type minemu_bootstrap_high, %function
minemu_bootstrap_high:
    msr cpsr_c, #0xd3
    ldr sp, =__minemu_svc_stack_top
    msr cpsr_c, #0xd2
    ldr sp, =__minemu_irq_stack_top
    msr cpsr_c, #0xd7
    ldr sp, =__minemu_abt_stack_top
    msr cpsr_c, #0xdb
    ldr sp, =__minemu_und_stack_top
    msr cpsr_c, #0xd3
    mov r0, r8
    bl minemu_kernel_main
    bl minemu_fail_stop

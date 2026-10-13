.syntax unified
.cpu cortex-a9
.arm

// r0 = user entry point; r1 = user stack top.

.global minemu_enter_user
.type minemu_enter_user, %function
minemu_enter_user:
    cps #0x1f                
    mov sp, r1
    mov lr, #0
    cps #0x13                
    mrs r2, cpsr
    bic r2, r2, #0x1f
    orr r2, r2, #0x10       
    msr spsr_cxsf, r2
    mov lr, r0
    movs pc, lr          
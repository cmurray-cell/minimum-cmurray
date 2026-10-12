.syntax unified
.cpu cortex-a9
.arm

.section .vectors, "ax", %progbits
.global minemu_vectors
.type minemu_vectors, %function
.extern minemu_fail_stop
.extern minemu_svc_trampoline

minemu_vectors:
    b minemu_fail_stop
    b minemu_fail_stop
    b minemu_svc_trampoline
    b minemu_fail_stop
    b minemu_fail_stop
    .word 0
    b minemu_fail_stop
    .word 0

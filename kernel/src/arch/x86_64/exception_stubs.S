.pushsection .text

.global kernel_x86_64_exception_0
kernel_x86_64_exception_0:
    pushq $0
    pushq $0
    jmp kernel_x86_64_exception_common

.global kernel_x86_64_exception_6
kernel_x86_64_exception_6:
    pushq $0
    pushq $6
    jmp kernel_x86_64_exception_common

.global kernel_x86_64_exception_13
kernel_x86_64_exception_13:
    pushq $13
    jmp kernel_x86_64_exception_common

.global kernel_x86_64_exception_14
kernel_x86_64_exception_14:
    pushq $14
    jmp kernel_x86_64_exception_common

kernel_x86_64_exception_common:
    cld
    pushq %r15
    pushq %r14
    pushq %r13
    pushq %r12
    pushq %r11
    pushq %r10
    pushq %r9
    pushq %r8
    pushq %rdi
    pushq %rsi
    pushq %rbp
    pushq %rdx
    pushq %rcx
    pushq %rbx
    pushq %rax
    movq %rsp, %rdi
    call kernel_x86_64_exception_dispatch

1:
    cli
    hlt
    jmp 1b

.popsection

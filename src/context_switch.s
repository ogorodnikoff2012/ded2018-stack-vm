	.text
	.globl	DoSwitch
	.type	DoSwitch, @function
DoSwitch:
    # See https://gitlab.com/Lipovsky/twist/blob/master/twist/fiber/core/context.S for details
    push %r15
    push %r14
    push %r13
    push %r12

    push %rbx
    push %rbp

    mov %rsp, (%rdi)
    mov (%rsi), %rsp

    pop %rbp
    pop %rbx
    pop %r12
    pop %r13
    pop %r14
    pop %r15

    retq

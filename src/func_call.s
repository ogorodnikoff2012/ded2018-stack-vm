    .text
    .globl  FuncCall
    .type   FuncCall, @function
FuncCall:
    pop %rax
    sub $8, %rbp
    mov %rax, (%rbp)
    mov %rcx, %rax
    jmpq *%rbx

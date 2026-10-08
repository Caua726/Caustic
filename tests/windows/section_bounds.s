.intel_syntax noprefix
.section .pointers, "aw", @progbits
.globl pointer_value
pointer_value:
    .quad .Lliteral
.globl __start_pointers
.globl __stop_pointers
.text
.globl main
main:
    lea rax, [rip + __start_pointers]
    lea rcx, [rip + pointer_value]
    cmp rax, rcx
    jne .Lfail
    mov rax, QWORD PTR [rax]
    mov rax, QWORD PTR [rax]
    cmp rax, 11
    jne .Lfail
    lea rax, [rip + __stop_pointers]
    sub rax, rcx
    cmp rax, 8
    jne .Lfail
    xor eax, eax
    ret
.Lfail:
    mov eax, 1
    ret
.section .rodata
.Lliteral:
    .quad 11

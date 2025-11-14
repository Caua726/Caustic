.intel_syntax noprefix
.global main

main:
  push rbp
  mov rbp, rsp
  mov rbx, 16
  mov QWORD PTR [rbp-8], rbx
  mov rbx, 14
  mov QWORD PTR [rbp-16], rbx
  mov rbx, QWORD PTR [rbp-8]
  mov rsi, QWORD PTR [rbp-16]
  mov r15, rbx
  mov r14, rsi
  xor rax, rax
  cmp r15, r14
  setl al
  mov rdi, rax
  mov r15, rdi
  test r15, r15
  jz .L0
  mov rbx, 1
  mov rax, rbx
  mov rsp, rbp
  pop rbp
  ret
  jmp .L1
.L0:
  mov rbx, 0
  mov rax, rbx
  mov rsp, rbp
  pop rbp
  ret
.L1:


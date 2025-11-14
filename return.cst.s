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
  mov rdi, rbx
  sub rdi, rsi
  mov QWORD PTR [rbp-24], rdi
  mov rbx, QWORD PTR [rbp-24]
  mov r15, rbx
  test r15, r15
  jz .L0
  mov rbx, 99
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


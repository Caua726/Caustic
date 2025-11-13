.intel_syntax noprefix
.global main

main:
  push rbp
  mov rbp, rsp
  mov rbx, 5
  mov QWORD PTR [rbp-8], rbx
  mov rbx, QWORD PTR [rbp-8]
  mov rsi, 1
  mov rdi, rbx
  add rdi, rsi
  mov rax, rdi
  mov rsp, rbp
  pop rbp
  ret


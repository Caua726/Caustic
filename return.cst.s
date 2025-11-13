.intel_syntax noprefix
.global main

main:
  push rbp
  mov rbp, rsp
  mov rbx, 10
  mov QWORD PTR [rbp-8], rbx
  mov rbx, 20
  mov QWORD PTR [rbp-16], rbx
  mov rbx, QWORD PTR [rbp-8]
  mov rax, rbx
  mov rsp, rbp
  pop rbp
  ret


.intel_syntax noprefix
.global main

main:
  push rbp
  mov rbp, rsp
  mov rbx, 1
  mov rsi, 3
  mov rdi, 6
  mov r8, rsi
  imul r8, rdi
  mov rsi, rbx
  add rsi, r8
  mov rax, rsi
  mov rsp, rbp
  pop rbp
  ret


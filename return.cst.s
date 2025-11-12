.intel_syntax noprefix
.global main

main:
  push rbp
  mov rbp, rsp
  mov rbx, 1
  mov rsi, 1
  mov rdi, 3
  mov r8, rsi
  imul r8, rdi
  mov rsi, 6
  mov rdi, r8
  imul rdi, rsi
  mov rsi, rbx
  add rsi, rdi
  mov rbx, 1
  mov rdi, rsi
  add rdi, rbx
  mov rbx, 1
  mov rsi, 6
  mov rax, rbx
  mov r15, rsi
  cqo
  idiv r15
  mov r8, rax
  mov rbx, 4
  mov rsi, r8
  imul rsi, rbx
  mov rbx, 2
  mov r8, rsi
  imul r8, rbx
  mov rbx, rdi
  add rbx, r8
  mov rax, rbx
  mov rsp, rbp
  pop rbp
  ret


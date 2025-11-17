.intel_syntax noprefix
.global main

add:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  mov rbx, 1
  mov rsi, 5
  mov r15, rbx
  mov r14, rsi
  add r15, r14
  mov rdi, r15
  mov rax, rdi
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

main:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  call add
  mov rbx, rax
  mov BYTE PTR [rbp-1], bl
  movsx rbx, BYTE PTR [rbp-1]
  mov rax, rbx
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret


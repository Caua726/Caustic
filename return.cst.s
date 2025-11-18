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
  mov rbx, rdi
  mov DWORD PTR [rbp-4], ebx
  mov rbx, rsi
  mov DWORD PTR [rbp-8], ebx
  movsxd rbx, DWORD PTR [rbp-4]
  movsxd r12, DWORD PTR [rbp-8]
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov DWORD PTR [rbp-12], r13d
  movsxd rbx, DWORD PTR [rbp-12]
  mov rax, rbx
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
  mov rbx, 10
  mov DWORD PTR [rbp-16], ebx
  mov rbx, 20
  mov DWORD PTR [rbp-20], ebx
  movsxd rbx, DWORD PTR [rbp-16]
  movsxd r12, DWORD PTR [rbp-20]
  mov rdi, rbx
  mov rsi, r12
  call add
  mov rbx, rax
  mov rax, rbx
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret


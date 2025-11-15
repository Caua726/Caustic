.intel_syntax noprefix
.global main

main:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  mov rbx, 0
  mov QWORD PTR [rbp-8], rbx
.L0:
  mov rbx, QWORD PTR [rbp-8]
  mov rsi, 100
  mov r15, rbx
  mov r14, rsi
  xor rax, rax
  cmp r15, r14
  setl al
  mov rdi, rax
  mov r15, rdi
  test r15, r15
  jz .L1
  mov r8, QWORD PTR [rbp-8]
  mov r9, 10
  mov r10, r8
  add r10, r9
  mov QWORD PTR [rbp-8], r10
  jmp .L0
.L1:
  mov rbx, QWORD PTR [rbp-8]
  mov rax, rbx
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret


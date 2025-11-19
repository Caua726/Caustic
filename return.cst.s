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
  sub rsp, 80
  mov rbx, 10
  lea r12, [rbp-56]
  mov r15, r12
  mov rax, rbx
  mov DWORD PTR [r15], eax
  mov rbx, 20
  lea r12, [rbp-56]
  mov QWORD PTR [rbp-64], 4
  mov r15, r12
  mov r14, QWORD PTR [rbp-64]
  add r15, r14
  mov r13, r15
  mov r15, r13
  mov rax, rbx
  mov DWORD PTR [r15], eax
  lea rbx, [rbp-56]
  mov r15, rbx
  movsxd r12, DWORD PTR [r15]
  lea rbx, [rbp-56]
  mov QWORD PTR [rbp-72], 4
  mov r15, rbx
  mov r14, QWORD PTR [rbp-72]
  add r15, r14
  mov r13, r15
  mov r15, r13
  movsxd rbx, DWORD PTR [r15]
  mov r15, r12
  mov r14, rbx
  add r15, r14
  mov r13, r15
  mov rax, r13
  add rsp, 80
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret


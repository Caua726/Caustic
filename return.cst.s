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
  sub rsp, 112
  mov rbx, 79
  lea r12, [rbp-48]
  mov QWORD PTR [rbp-72], 0
  mov r15, r12
  mov r14, QWORD PTR [rbp-72]
  add r15, r14
  mov r13, r15
  mov r15, r13
  mov rax, rbx
  mov BYTE PTR [r15], al
  mov rbx, 108
  lea r12, [rbp-48]
  mov QWORD PTR [rbp-80], 1
  mov r15, r12
  mov r14, QWORD PTR [rbp-80]
  add r15, r14
  mov r13, r15
  mov r15, r13
  mov rax, rbx
  mov BYTE PTR [r15], al
  mov rbx, 97
  lea r12, [rbp-48]
  mov QWORD PTR [rbp-88], 2
  mov r15, r12
  mov r14, QWORD PTR [rbp-88]
  add r15, r14
  mov r13, r15
  mov r15, r13
  mov rax, rbx
  mov BYTE PTR [r15], al
  mov rbx, 10
  lea r12, [rbp-48]
  mov QWORD PTR [rbp-96], 3
  mov r15, r12
  mov r14, QWORD PTR [rbp-96]
  add r15, r14
  mov r13, r15
  mov r15, r13
  mov rax, rbx
  mov BYTE PTR [r15], al
  lea rbx, [rbp-48]
  mov r12, 0
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov QWORD PTR [rbp-64], r13
  mov QWORD PTR [rbp-104], 1
  mov r12, 1
  mov r13, QWORD PTR [rbp-64]
  mov rbx, 4
  mov rax, QWORD PTR [rbp-104]
  mov rdi, r12
  mov rsi, r13
  mov rdx, rbx
  syscall
  mov rbx, rax
  mov QWORD PTR [rbp-72], rbx
  mov rbx, 0
  mov rax, rbx
  add rsp, 112
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret


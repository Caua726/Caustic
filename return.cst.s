.intel_syntax noprefix
.text
.global main

main:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 128
  mov rbx, 0
  mov DWORD PTR [rbp-48], ebx
.L0:
  movsxd r15, DWORD PTR [rbp-48]
  mov QWORD PTR [rbp-64], r15
  mov r12, 10
  mov r15, QWORD PTR [rbp-64]
  mov r14, r12
  xor rax, rax
  cmp r15, r14
  setl al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L1
  movsxd r15, DWORD PTR [rbp-48]
  mov QWORD PTR [rbp-72], r15
  mov QWORD PTR [rbp-80], 1
  mov r15, QWORD PTR [rbp-72]
  mov r14, QWORD PTR [rbp-80]
  add r15, r14
  mov QWORD PTR [rbp-88], r15
  mov r15, QWORD PTR [rbp-88]
  mov DWORD PTR [rbp-48], r15d
  movsxd r15, DWORD PTR [rbp-48]
  mov QWORD PTR [rbp-96], r15
  mov QWORD PTR [rbp-104], 5
  mov r15, QWORD PTR [rbp-96]
  mov r14, QWORD PTR [rbp-104]
  xor rax, rax
  cmp r15, r14
  sete al
  mov QWORD PTR [rbp-112], rax
  mov r15, QWORD PTR [rbp-112]
  test r15, r15
  jz .L2
  jmp .L0
  jmp .L3
.L2:
.L3:
  movsxd r15, DWORD PTR [rbp-48]
  mov QWORD PTR [rbp-120], r15
  mov QWORD PTR [rbp-128], 8
  mov r15, QWORD PTR [rbp-120]
  mov r14, QWORD PTR [rbp-128]
  xor rax, rax
  cmp r15, r14
  sete al
  mov rbx, rax
  mov r15, rbx
  test r15, r15
  jz .L4
  jmp .L1
  jmp .L5
.L4:
.L5:
  jmp .L0
.L1:
  mov rbx, 0
  mov DWORD PTR [rbp-52], ebx
  mov rbx, 1
  mov r12, 1
  mov r15, rbx
  mov r14, r12
  xor rax, rax
  cmp r15, r14
  sete al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jnz .L8
  mov rbx, 1
  mov r12, 0
  mov r15, rbx
  mov r14, r12
  xor rax, rax
  cmp r15, r14
  sete al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jnz .L8
  mov rbx, 0
  jmp .L9
.L8:
  mov rbx, 1
.L9:
  mov r15, rbx
  test r15, r15
  jz .L6
  movsxd rbx, DWORD PTR [rbp-52]
  mov r12, 1
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov DWORD PTR [rbp-52], r13d
  jmp .L7
.L6:
.L7:
  mov rbx, 1
  mov r12, 0
  mov r15, rbx
  mov r14, r12
  xor rax, rax
  cmp r15, r14
  sete al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L12
  mov rbx, 1
  mov r12, 1
  mov r15, rbx
  mov r14, r12
  xor rax, rax
  cmp r15, r14
  sete al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L12
  mov rbx, 1
  jmp .L13
.L12:
  mov rbx, 0
.L13:
  mov r15, rbx
  test r15, r15
  jz .L10
  movsxd rbx, DWORD PTR [rbp-52]
  mov r12, 100
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov DWORD PTR [rbp-52], r13d
  jmp .L11
.L10:
.L11:
  movsxd rbx, DWORD PTR [rbp-48]
  movsxd r12, DWORD PTR [rbp-52]
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov rax, r13
  add rsp, 128
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret


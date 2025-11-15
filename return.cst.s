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
  sub rsp, 304
  mov rbx, 1
  mov WORD PTR [rbp-2], bx
  mov rbx, 2
  mov WORD PTR [rbp-4], bx
  mov rbx, 4
  mov WORD PTR [rbp-6], bx
  mov rbx, 6
  mov WORD PTR [rbp-8], bx
.L0:
  movsx rax, WORD PTR [rbp-2]
  mov rsi, 100
  mov r15, rax
  mov r14, rsi
  xor rax, rax
  cmp r15, r14
  setl al
  mov rdi, rax
  mov r15, rdi
  test r15, r15
  jz .L1
  movsx r8, WORD PTR [rbp-2]
  mov r9, 1
  mov r15, r8
  mov r14, r9
  add r15, r14
  mov r10, r15
  mov WORD PTR [rbp-2], r10w
  movsx r11, WORD PTR [rbp-4]
  movsx r12, WORD PTR [rbp-6]
  mov r15, r11
  mov r14, r12
  xor rax, rax
  cmp r15, r14
  sete al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L2
  movsx r14, WORD PTR [rbp-6]
  mov r15, 57
  mov r15, r14
  mov r14, r15
  add r15, r14
  mov QWORD PTR [rbp-16], r15
  mov r15, QWORD PTR [rbp-16]
  mov WORD PTR [rbp-6], r15w
  jmp .L3
.L2:
.L3:
  movsx r15, WORD PTR [rbp-6]
  mov QWORD PTR [rbp-24], r15
  mov QWORD PTR [rbp-32], 4
  mov r15, QWORD PTR [rbp-24]
  mov r14, QWORD PTR [rbp-32]
  xor rax, rax
  cmp r15, r14
  sete al
  mov QWORD PTR [rbp-40], rax
  mov r15, QWORD PTR [rbp-40]
  test r15, r15
  jz .L4
  movsx r15, WORD PTR [rbp-4]
  mov QWORD PTR [rbp-48], r15
  mov QWORD PTR [rbp-56], 25
  mov r15, QWORD PTR [rbp-48]
  mov r14, QWORD PTR [rbp-56]
  add r15, r14
  mov QWORD PTR [rbp-64], r15
  mov r15, QWORD PTR [rbp-64]
  mov WORD PTR [rbp-4], r15w
  jmp .L5
.L4:
.L5:
  movsx r15, WORD PTR [rbp-4]
  mov QWORD PTR [rbp-72], r15
  movsx r15, WORD PTR [rbp-6]
  mov QWORD PTR [rbp-80], r15
  mov r15, QWORD PTR [rbp-72]
  mov r14, QWORD PTR [rbp-80]
  xor rax, rax
  cmp r15, r14
  sete al
  mov QWORD PTR [rbp-88], rax
  mov r15, QWORD PTR [rbp-88]
  test r15, r15
  jz .L6
  movsx r15, WORD PTR [rbp-8]
  mov QWORD PTR [rbp-96], r15
  mov QWORD PTR [rbp-104], 15
  mov r15, QWORD PTR [rbp-96]
  mov r14, QWORD PTR [rbp-104]
  add r15, r14
  mov QWORD PTR [rbp-112], r15
  mov r15, QWORD PTR [rbp-112]
  mov WORD PTR [rbp-8], r15w
  jmp .L7
.L6:
.L7:
  movsx r15, WORD PTR [rbp-4]
  mov QWORD PTR [rbp-120], r15
  movsx r15, WORD PTR [rbp-6]
  mov QWORD PTR [rbp-128], r15
  mov r15, QWORD PTR [rbp-120]
  mov r14, QWORD PTR [rbp-128]
  xor rax, rax
  cmp r15, r14
  sete al
  mov QWORD PTR [rbp-136], rax
  mov r15, QWORD PTR [rbp-136]
  test r15, r15
  jz .L8
  movsx r15, WORD PTR [rbp-8]
  mov QWORD PTR [rbp-144], r15
  mov QWORD PTR [rbp-152], 15
  mov r15, QWORD PTR [rbp-144]
  mov r14, QWORD PTR [rbp-152]
  add r15, r14
  mov QWORD PTR [rbp-160], r15
  mov r15, QWORD PTR [rbp-160]
  mov WORD PTR [rbp-8], r15w
  jmp .L9
.L8:
.L9:
  movsx r15, WORD PTR [rbp-2]
  mov QWORD PTR [rbp-168], r15
  mov QWORD PTR [rbp-176], 5
  mov r15, QWORD PTR [rbp-168]
  mov r14, QWORD PTR [rbp-176]
  xor rax, rax
  cmp r15, r14
  sete al
  mov QWORD PTR [rbp-184], rax
  mov r15, QWORD PTR [rbp-184]
  test r15, r15
  jz .L10
  movsx r15, WORD PTR [rbp-2]
  mov QWORD PTR [rbp-192], r15
  mov QWORD PTR [rbp-200], 55
  mov r15, QWORD PTR [rbp-192]
  mov r14, QWORD PTR [rbp-200]
  add r15, r14
  mov QWORD PTR [rbp-208], r15
  mov r15, QWORD PTR [rbp-208]
  mov WORD PTR [rbp-2], r15w
  jmp .L11
.L10:
.L11:
  movsx r15, WORD PTR [rbp-2]
  mov QWORD PTR [rbp-216], r15
  mov QWORD PTR [rbp-224], 10
  mov r15, QWORD PTR [rbp-216]
  mov r14, QWORD PTR [rbp-224]
  xor rax, rax
  cmp r15, r14
  sete al
  mov QWORD PTR [rbp-232], rax
  mov r15, QWORD PTR [rbp-232]
  test r15, r15
  jz .L12
  movsx r15, WORD PTR [rbp-2]
  mov QWORD PTR [rbp-240], r15
  mov QWORD PTR [rbp-248], 10
  mov r15, QWORD PTR [rbp-240]
  mov r14, QWORD PTR [rbp-248]
  add r15, r14
  mov QWORD PTR [rbp-256], r15
  mov r15, QWORD PTR [rbp-256]
  mov WORD PTR [rbp-2], r15w
  jmp .L13
.L12:
.L13:
  movsx r15, WORD PTR [rbp-2]
  mov QWORD PTR [rbp-264], r15
  mov QWORD PTR [rbp-272], 15
  mov r15, QWORD PTR [rbp-264]
  mov r14, QWORD PTR [rbp-272]
  xor rax, rax
  cmp r15, r14
  sete al
  mov QWORD PTR [rbp-280], rax
  mov r15, QWORD PTR [rbp-280]
  test r15, r15
  jz .L14
  movsx r15, WORD PTR [rbp-2]
  mov QWORD PTR [rbp-288], r15
  mov QWORD PTR [rbp-296], 15
  mov r15, QWORD PTR [rbp-288]
  mov r14, QWORD PTR [rbp-296]
  add r15, r14
  mov rbx, r15
  mov WORD PTR [rbp-2], bx
  jmp .L15
.L14:
.L15:
  jmp .L0
.L1:
  movsx rbx, WORD PTR [rbp-2]
  mov rax, rbx
  add rsp, 304
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret


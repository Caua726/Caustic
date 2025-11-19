.intel_syntax noprefix
.text
.globl main
main:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 64
  mov rbx, 8
  mov DWORD PTR [rbp-52], ebx
  mov rbx, 1024
  mov r12, rbx
  mov QWORD PTR [rbp-60], r12
  mov rbx, 1
  mov r12, 4
  mov rcx, r12
  mov r15, rbx
  shl r15, cl
  mov r13, r15
  mov DWORD PTR [rbp-64], r13d
  movsxd rbx, DWORD PTR [rbp-52]
  mov r12, 8
  mov r15, rbx
  mov r14, r12
  xor rax, rax
  cmp r15, r14
  sete al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L2
  movsxd rbx, DWORD PTR [rbp-64]
  mov r12, 16
  mov r15, rbx
  mov r14, r12
  xor rax, rax
  cmp r15, r14
  sete al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L2
  mov rbx, 1
  jmp .L3
.L2:
  mov rbx, 0
.L3:
  mov r15, rbx
  test r15, r15
  jz .L0
  mov rbx, 1
  mov rax, rbx
  add rsp, 64
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  jmp .L1
.L0:
.L1:
  mov rbx, 0
  mov rax, rbx
  add rsp, 64
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret


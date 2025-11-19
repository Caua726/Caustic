.intel_syntax noprefix
.section .data
.global global_var
global_var:
  .long 10
.global global_const
global_const:
  .long 20
.section .bss
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
  sub rsp, 48
  lea rbx, [rip+global_var]
  mov r15, rbx
  movsxd r12, DWORD PTR [r15]
  lea rbx, [rip+global_const]
  mov r15, rbx
  movsxd r13, DWORD PTR [r15]
  mov r15, r12
  mov r14, r13
  add r15, r14
  mov rbx, r15
  mov DWORD PTR [rbp-48], ebx
  movsxd rbx, DWORD PTR [rbp-48]
  mov rax, rbx
  add rsp, 48
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret


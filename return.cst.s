.intel_syntax noprefix
.section .rodata
.LC0:
  .string "Ola Mundo\n"
.text
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
  lea rbx, [rip+.LC0]
  mov QWORD PTR [rbp-48], rbx
  mov QWORD PTR [rbp-56], 1
  mov r12, 1
  mov r13, QWORD PTR [rbp-48]
  mov rbx, 10
  mov rax, QWORD PTR [rbp-56]
  mov rdi, r12
  mov rsi, r13
  mov rdx, rbx
  syscall
  mov rbx, rax
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


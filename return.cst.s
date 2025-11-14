.intel_syntax noprefix
.global main

main:
  push rbp
  mov rbp, rsp
  sub rsp, 16
  mov byte ptr [rsp+0], 'H'
  mov byte ptr [rsp+1], 'e'
  mov byte ptr [rsp+2], 'l'
  mov byte ptr [rsp+3], 'l'
  mov byte ptr [rsp+4], 'o'
  mov byte ptr [rsp+5], ','
  mov byte ptr [rsp+6], ' '
  mov byte ptr [rsp+7], 'W'
  mov byte ptr [rsp+8], 'o'
  mov byte ptr [rsp+9], 'r'
  mov byte ptr [rsp+10], 'l'
  mov byte ptr [rsp+11], 'd'
  mov byte ptr [rsp+12], '!'
  mov byte ptr [rsp+13], 10
  mov rax, 1
  mov rdi, 1
  mov rsi, rsp
  mov rdx, 14
  syscall
  add rsp, 16
  mov rbx, 0
  mov rax, rbx
  mov rsp, rbp
  pop rbp
  ret


.intel_syntax noprefix
.section .rodata
.LC9:
  .string ""
.LC8:
  .string "Neg: "
.LC7:
  .string ""
.LC6:
  .string "Int: "
.LC5:
  .string ""
.LC4:
  .string "World!"
.LC3:
  .string "Hello, "
.LC2:
  .string "
"
.LC1:
  .string "Error: Global Heap not init
"
.LC0:
  .string "mmap failed
"
.text
.section .data
.global STDOUT
STDOUT:
  .quad 1
.global STDERR
STDERR:
  .quad 2
.section .bss
.global _std_heap
_std_heap:
  .zero 8
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
  sub rsp, 224
  mov rbx, 1048576
  mov rdi, rbx
  call gheapinit
  mov rbx, rax
  xor r10, r10
  lea rbx, [rbp-96]
  lea r12, [rip+.LC3]
  mov rdi, rbx
  mov rsi, r12
  call String
  mov r12, rax
  lea r12, [rbp-64]
  push rcx
  push rsi
  push rdi
  mov rdi, r12
  mov rsi, rbx
  mov rcx, 16
  cld
  rep movsb
  pop rdi
  pop rsi
  pop rcx
  xor r10, r10
  lea rbx, [rbp-112]
  lea r12, [rip+.LC4]
  mov rdi, rbx
  mov rsi, r12
  call String
  mov r12, rax
  lea r12, [rbp-80]
  push rcx
  push rsi
  push rdi
  mov rdi, r12
  mov rsi, rbx
  mov rcx, 16
  cld
  rep movsb
  pop rdi
  pop rsi
  pop rcx
  xor r10, r10
  lea rbx, [rbp-128]
  lea r12, [rbp-64]
  lea r13, [rbp-80]
  mov rdi, rbx
  mov rsi, r12
  mov rdx, r13
  call concat
  mov r12, rax
  lea r12, [rbp-96]
  push rcx
  push rsi
  push rdi
  mov rdi, r12
  mov rsi, rbx
  mov rcx, 16
  cld
  rep movsb
  pop rdi
  pop rsi
  pop rcx
  lea rbx, [rbp-96]
  mov rdi, rbx
  call print
  mov rbx, rax
  lea rbx, [rbp-144]
  lea r12, [rip+.LC5]
  mov rdi, rbx
  mov rsi, r12
  call String
  mov r12, rax
  mov rdi, rbx
  call println
  mov rbx, rax
  lea rbx, [rbp-160]
  lea r12, [rip+.LC6]
  mov rdi, rbx
  mov rsi, r12
  call String
  mov r12, rax
  mov rdi, rbx
  call print
  mov rbx, rax
  mov rbx, 12345
  mov rdi, rbx
  call print_int
  mov rbx, rax
  lea rbx, [rbp-176]
  lea r12, [rip+.LC7]
  mov rdi, rbx
  mov rsi, r12
  call String
  mov r12, rax
  mov rdi, rbx
  call println
  mov rbx, rax
  lea rbx, [rbp-192]
  lea r12, [rip+.LC8]
  mov rdi, rbx
  mov rsi, r12
  call String
  mov r12, rax
  mov rdi, rbx
  call print
  mov rbx, rax
  mov rbx, -987
  mov rdi, rbx
  call print_int
  mov rbx, rax
  lea rbx, [rbp-208]
  lea r12, [rip+.LC9]
  mov rdi, rbx
  mov rsi, r12
  call String
  mov r12, rax
  mov rdi, rbx
  call println
  mov rbx, rax
  lea rbx, [rbp-64]
  mov rdi, rbx
  call string_free
  mov rbx, rax
  lea rbx, [rbp-80]
  mov rdi, rbx
  call string_free
  mov rbx, rax
  lea rbx, [rbp-96]
  mov rdi, rbx
  call string_free
  mov rbx, rax
  mov rbx, 0
  mov rax, rbx
  add rsp, 224
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  mov rbx, 0
  mov rax, rbx
  add rsp, 224
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

reserve:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 144
  mov rbx, rdi
  mov QWORD PTR [rbp-56], rbx
  mov rbx, QWORD PTR [rbp-56]
  mov QWORD PTR [rbp-64], rbx
  mov rbx, QWORD PTR [rbp-64]
  mov r12, 32
  mov r13, r12
  mov r15, rbx
  mov r14, r13
  add r15, r14
  mov r12, r15
  mov QWORD PTR [rbp-72], r12
  mov rbx, -1
  mov QWORD PTR [rbp-80], rbx
  xor r10, r10
  mov rbx, 0
  mov r12, rbx
  mov r15, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-120], r15
  mov r13, 3
  mov QWORD PTR [rbp-128], 34
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-136], r15
  mov rbx, 0
  mov rdi, r12
  mov rsi, QWORD PTR [rbp-120]
  mov rdx, r13
  mov rcx, QWORD PTR [rbp-128]
  mov r8, QWORD PTR [rbp-136]
  mov r9, rbx
  call mmap
  mov rbx, rax
  mov QWORD PTR [rbp-88], rbx
  mov rbx, QWORD PTR [rbp-88]
  mov r12, rbx
  mov rbx, 0
  mov r13, rbx
  mov r15, r12
  mov r14, r13
  xor rax, rax
  cmp r15, r14
  setl al
  mov rbx, rax
  mov r15, rbx
  test r15, r15
  jz .L0
  lea rbx, [rip+STDERR]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  lea rbx, [rip+.LC0]
  mov r13, 12
  mov rdi, r12
  mov rsi, rbx
  mov rdx, r13
  call write
  mov rbx, rax
  mov rbx, 0
  mov r12, rbx
  mov rax, r12
  add rsp, 144
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
  mov rbx, QWORD PTR [rbp-88]
  mov r12, rbx
  mov QWORD PTR [rbp-96], r12
  mov rbx, QWORD PTR [rbp-88]
  mov r12, rbx
  mov QWORD PTR [rbp-104], r12
  mov rbx, QWORD PTR [rbp-104]
  mov r12, 32
  mov r13, r12
  mov r15, rbx
  mov r14, r13
  add r15, r14
  mov r12, r15
  mov QWORD PTR [rbp-112], r12
  mov rbx, QWORD PTR [rbp-96]
  mov r12, 8
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov rbx, QWORD PTR [rbp-112]
  mov r12, rbx
  mov r15, r13
  mov QWORD PTR [r15], r12
  mov rbx, QWORD PTR [rbp-96]
  mov r12, 16
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov rbx, QWORD PTR [rbp-112]
  mov r12, rbx
  mov r15, r13
  mov QWORD PTR [r15], r12
  mov rbx, QWORD PTR [rbp-96]
  mov r12, 24
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov rbx, QWORD PTR [rbp-104]
  mov r15, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-144], r15
  mov r15, rbx
  mov r14, QWORD PTR [rbp-144]
  add r15, r14
  mov r12, r15
  mov rbx, r12
  mov r15, r13
  mov QWORD PTR [r15], rbx
  mov rbx, QWORD PTR [rbp-96]
  mov r12, 0
  mov r13, r12
  mov r15, rbx
  mov QWORD PTR [r15], r13
  mov rbx, QWORD PTR [rbp-96]
  mov rax, rbx
  add rsp, 144
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  mov rbx, 0
  mov rax, rbx
  add rsp, 144
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

alloc:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 784
  mov rbx, rdi
  mov QWORD PTR [rbp-56], rbx
  mov rbx, rsi
  mov QWORD PTR [rbp-64], rbx
  mov rbx, QWORD PTR [rbp-64]
  mov QWORD PTR [rbp-72], rbx
  mov rbx, QWORD PTR [rbp-56]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  mov QWORD PTR [rbp-80], r12
  mov rbx, 0
  mov r12, rbx
  mov QWORD PTR [rbp-88], r12
.L2:
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-160], r15
  mov r14, QWORD PTR [rbp-160]
  mov r15, r14
  mov QWORD PTR [rbp-352], r15
  mov QWORD PTR [rbp-408], 0
  mov r14, QWORD PTR [rbp-408]
  mov r15, r14
  mov QWORD PTR [rbp-168], r15
  mov r15, QWORD PTR [rbp-352]
  mov r14, QWORD PTR [rbp-168]
  xor rax, rax
  cmp r15, r14
  setne al
  mov QWORD PTR [rbp-176], rax
  mov r15, QWORD PTR [rbp-176]
  test r15, r15
  jz .L3
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-184], r15
  mov r15, QWORD PTR [rbp-184]
  mov rax, QWORD PTR [r15]
  mov QWORD PTR [rbp-192], rax
  mov r15, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-200], r15
  mov r15, QWORD PTR [rbp-192]
  mov r14, QWORD PTR [rbp-200]
  xor rax, rax
  cmp r15, r14
  setge al
  mov QWORD PTR [rbp-208], rax
  mov r15, QWORD PTR [rbp-208]
  test r15, r15
  jz .L4
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-216], r15
  mov r15, QWORD PTR [rbp-216]
  mov rax, QWORD PTR [r15]
  mov QWORD PTR [rbp-224], rax
  mov r15, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-232], r15
  mov r15, QWORD PTR [rbp-224]
  mov r14, QWORD PTR [rbp-232]
  sub r15, r14
  mov QWORD PTR [rbp-240], r15
  mov r15, QWORD PTR [rbp-240]
  mov QWORD PTR [rbp-96], r15
  mov r15, QWORD PTR [rbp-96]
  mov QWORD PTR [rbp-248], r15
  mov QWORD PTR [rbp-256], 16
  mov r14, QWORD PTR [rbp-256]
  mov r15, r14
  mov QWORD PTR [rbp-264], r15
  mov r15, QWORD PTR [rbp-248]
  mov r14, QWORD PTR [rbp-264]
  xor rax, rax
  cmp r15, r14
  setge al
  mov QWORD PTR [rbp-272], rax
  mov r15, QWORD PTR [rbp-272]
  test r15, r15
  jz .L6
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-280], r15
  mov r14, QWORD PTR [rbp-280]
  mov r15, r14
  mov QWORD PTR [rbp-288], r15
  mov QWORD PTR [rbp-296], 16
  mov r14, QWORD PTR [rbp-296]
  mov r15, r14
  mov QWORD PTR [rbp-304], r15
  mov r15, QWORD PTR [rbp-288]
  mov r14, QWORD PTR [rbp-304]
  add r15, r14
  mov QWORD PTR [rbp-312], r15
  mov r15, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-320], r15
  mov r15, QWORD PTR [rbp-312]
  mov r14, QWORD PTR [rbp-320]
  add r15, r14
  mov QWORD PTR [rbp-328], r15
  mov r15, QWORD PTR [rbp-328]
  mov QWORD PTR [rbp-104], r15
  mov r15, QWORD PTR [rbp-104]
  mov QWORD PTR [rbp-336], r15
  mov r14, QWORD PTR [rbp-336]
  mov r15, r14
  mov QWORD PTR [rbp-344], r15
  mov r15, QWORD PTR [rbp-344]
  mov QWORD PTR [rbp-112], r15
  mov r15, QWORD PTR [rbp-112]
  mov QWORD PTR [rbp-488], r15
  mov r15, QWORD PTR [rbp-96]
  mov QWORD PTR [rbp-360], r15
  mov QWORD PTR [rbp-368], 16
  mov r14, QWORD PTR [rbp-368]
  mov r15, r14
  mov QWORD PTR [rbp-376], r15
  mov r15, QWORD PTR [rbp-360]
  mov r14, QWORD PTR [rbp-376]
  sub r15, r14
  mov QWORD PTR [rbp-384], r15
  mov r15, QWORD PTR [rbp-488]
  mov rax, QWORD PTR [rbp-384]
  mov QWORD PTR [r15], rax
  mov r15, QWORD PTR [rbp-112]
  mov QWORD PTR [rbp-392], r15
  mov QWORD PTR [rbp-400], 8
  mov r15, QWORD PTR [rbp-392]
  mov r14, QWORD PTR [rbp-400]
  add r15, r14
  mov r12, r15
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-416], r15
  mov QWORD PTR [rbp-424], 8
  mov r15, QWORD PTR [rbp-416]
  mov r14, QWORD PTR [rbp-424]
  add r15, r14
  mov QWORD PTR [rbp-432], r15
  mov r15, QWORD PTR [rbp-432]
  mov rax, QWORD PTR [r15]
  mov QWORD PTR [rbp-440], rax
  mov r15, r12
  mov rax, QWORD PTR [rbp-440]
  mov QWORD PTR [r15], rax
  mov r15, QWORD PTR [rbp-88]
  mov QWORD PTR [rbp-448], r15
  mov r14, QWORD PTR [rbp-448]
  mov r15, r14
  mov QWORD PTR [rbp-456], r15
  mov QWORD PTR [rbp-464], 0
  mov r14, QWORD PTR [rbp-464]
  mov r15, r14
  mov QWORD PTR [rbp-472], r15
  mov r15, QWORD PTR [rbp-456]
  mov r14, QWORD PTR [rbp-472]
  xor rax, rax
  cmp r15, r14
  sete al
  mov QWORD PTR [rbp-480], rax
  mov r15, QWORD PTR [rbp-480]
  test r15, r15
  jz .L8
  mov r13, QWORD PTR [rbp-56]
  mov r15, QWORD PTR [rbp-112]
  mov QWORD PTR [rbp-496], r15
  mov r15, r13
  mov rax, QWORD PTR [rbp-496]
  mov QWORD PTR [r15], rax
  jmp .L9
.L8:
  mov r15, QWORD PTR [rbp-88]
  mov QWORD PTR [rbp-504], r15
  mov QWORD PTR [rbp-512], 8
  mov r15, QWORD PTR [rbp-504]
  mov r14, QWORD PTR [rbp-512]
  add r15, r14
  mov QWORD PTR [rbp-520], r15
  mov r15, QWORD PTR [rbp-112]
  mov QWORD PTR [rbp-528], r15
  mov r15, QWORD PTR [rbp-520]
  mov rax, QWORD PTR [rbp-528]
  mov QWORD PTR [r15], rax
.L9:
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-536], r15
  mov r15, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-544], r15
  mov r15, QWORD PTR [rbp-536]
  mov rax, QWORD PTR [rbp-544]
  mov QWORD PTR [r15], rax
  jmp .L7
.L6:
  mov r15, QWORD PTR [rbp-88]
  mov QWORD PTR [rbp-552], r15
  mov r14, QWORD PTR [rbp-552]
  mov r15, r14
  mov QWORD PTR [rbp-560], r15
  mov QWORD PTR [rbp-568], 0
  mov r14, QWORD PTR [rbp-568]
  mov r15, r14
  mov QWORD PTR [rbp-576], r15
  mov r15, QWORD PTR [rbp-560]
  mov r14, QWORD PTR [rbp-576]
  xor rax, rax
  cmp r15, r14
  sete al
  mov QWORD PTR [rbp-584], rax
  mov r15, QWORD PTR [rbp-584]
  test r15, r15
  jz .L10
  mov r15, QWORD PTR [rbp-56]
  mov QWORD PTR [rbp-592], r15
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-600], r15
  mov QWORD PTR [rbp-608], 8
  mov r15, QWORD PTR [rbp-600]
  mov r14, QWORD PTR [rbp-608]
  add r15, r14
  mov QWORD PTR [rbp-616], r15
  mov r15, QWORD PTR [rbp-616]
  mov rax, QWORD PTR [r15]
  mov QWORD PTR [rbp-624], rax
  mov r15, QWORD PTR [rbp-592]
  mov rax, QWORD PTR [rbp-624]
  mov QWORD PTR [r15], rax
  jmp .L11
.L10:
  mov r15, QWORD PTR [rbp-88]
  mov QWORD PTR [rbp-632], r15
  mov QWORD PTR [rbp-640], 8
  mov r15, QWORD PTR [rbp-632]
  mov r14, QWORD PTR [rbp-640]
  add r15, r14
  mov QWORD PTR [rbp-648], r15
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-656], r15
  mov QWORD PTR [rbp-664], 8
  mov r15, QWORD PTR [rbp-656]
  mov r14, QWORD PTR [rbp-664]
  add r15, r14
  mov QWORD PTR [rbp-672], r15
  mov r15, QWORD PTR [rbp-672]
  mov rax, QWORD PTR [r15]
  mov QWORD PTR [rbp-680], rax
  mov r15, QWORD PTR [rbp-648]
  mov rax, QWORD PTR [rbp-680]
  mov QWORD PTR [r15], rax
.L11:
.L7:
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-688], r15
  mov r14, QWORD PTR [rbp-688]
  mov r15, r14
  mov QWORD PTR [rbp-696], r15
  mov QWORD PTR [rbp-704], 16
  mov r14, QWORD PTR [rbp-704]
  mov r15, r14
  mov QWORD PTR [rbp-712], r15
  mov r15, QWORD PTR [rbp-696]
  mov r14, QWORD PTR [rbp-712]
  add r15, r14
  mov QWORD PTR [rbp-720], r15
  mov r14, QWORD PTR [rbp-720]
  mov r15, r14
  mov QWORD PTR [rbp-728], r15
  mov rax, QWORD PTR [rbp-728]
  add rsp, 784
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  jmp .L5
.L4:
.L5:
  lea r15, [rbp-88]
  mov QWORD PTR [rbp-736], r15
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-744], r15
  mov r15, QWORD PTR [rbp-736]
  mov rax, QWORD PTR [rbp-744]
  mov QWORD PTR [r15], rax
  lea r15, [rbp-80]
  mov QWORD PTR [rbp-752], r15
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-760], r15
  mov QWORD PTR [rbp-768], 8
  mov r15, QWORD PTR [rbp-760]
  mov r14, QWORD PTR [rbp-768]
  add r15, r14
  mov QWORD PTR [rbp-776], r15
  mov r15, QWORD PTR [rbp-776]
  mov rbx, QWORD PTR [r15]
  mov r15, QWORD PTR [rbp-752]
  mov QWORD PTR [r15], rbx
  jmp .L2
.L3:
  mov rbx, QWORD PTR [rbp-56]
  mov r12, 16
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov r15, r13
  mov rbx, QWORD PTR [r15]
  mov QWORD PTR [rbp-120], rbx
  mov rbx, QWORD PTR [rbp-56]
  mov r12, 16
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov r15, r13
  mov rbx, QWORD PTR [r15]
  mov r12, rbx
  mov QWORD PTR [rbp-128], r12
  mov rbx, QWORD PTR [rbp-56]
  mov r12, 24
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov r15, r13
  mov rbx, QWORD PTR [r15]
  mov r12, rbx
  mov QWORD PTR [rbp-136], r12
  mov rbx, 16
  mov r12, QWORD PTR [rbp-72]
  mov r13, rbx
  mov r15, r13
  mov r14, r12
  add r15, r14
  mov rbx, r15
  mov QWORD PTR [rbp-144], rbx
  mov rbx, QWORD PTR [rbp-128]
  mov r12, QWORD PTR [rbp-144]
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov rbx, QWORD PTR [rbp-136]
  mov r15, r13
  mov r14, rbx
  xor rax, rax
  cmp r15, r14
  setg al
  mov r12, rax
  mov r15, r12
  test r15, r15
  jz .L12
  mov rbx, 0
  mov r12, rbx
  mov rax, r12
  add rsp, 784
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  jmp .L13
.L12:
.L13:
  mov rbx, QWORD PTR [rbp-56]
  mov r12, 16
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov r15, r13
  mov rbx, QWORD PTR [r15]
  mov r12, rbx
  mov QWORD PTR [rbp-152], r12
  mov rbx, QWORD PTR [rbp-152]
  mov r12, QWORD PTR [rbp-72]
  mov r15, rbx
  mov QWORD PTR [r15], r12
  mov rbx, QWORD PTR [rbp-56]
  mov r12, 16
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov rbx, QWORD PTR [rbp-128]
  mov r15, QWORD PTR [rbp-144]
  mov QWORD PTR [rbp-784], r15
  mov r15, rbx
  mov r14, QWORD PTR [rbp-784]
  add r15, r14
  mov r12, r15
  mov rbx, r12
  mov r15, r13
  mov QWORD PTR [r15], rbx
  mov rbx, QWORD PTR [rbp-128]
  mov r12, 16
  mov r13, r12
  mov r15, rbx
  mov r14, r13
  add r15, r14
  mov r12, r15
  mov rbx, r12
  mov rax, rbx
  add rsp, 784
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  mov rbx, 0
  mov rax, rbx
  add rsp, 784
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

free:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 80
  mov rbx, rdi
  mov QWORD PTR [rbp-56], rbx
  mov rbx, rsi
  mov QWORD PTR [rbp-64], rbx
  mov rbx, QWORD PTR [rbp-64]
  mov r12, rbx
  mov rbx, 0
  mov r13, rbx
  mov r15, r12
  mov r14, r13
  xor rax, rax
  cmp r15, r14
  sete al
  mov rbx, rax
  mov r15, rbx
  test r15, r15
  jz .L14
  add rsp, 80
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  jmp .L15
.L14:
.L15:
  mov rbx, QWORD PTR [rbp-56]
  mov r12, rbx
  mov rbx, 0
  mov r13, rbx
  mov r15, r12
  mov r14, r13
  xor rax, rax
  cmp r15, r14
  sete al
  mov rbx, rax
  mov r15, rbx
  test r15, r15
  jz .L16
  add rsp, 80
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  jmp .L17
.L16:
.L17:
  mov rbx, QWORD PTR [rbp-64]
  mov r12, rbx
  mov rbx, 16
  mov r13, rbx
  mov r15, r12
  mov r14, r13
  sub r15, r14
  mov rbx, r15
  mov QWORD PTR [rbp-72], rbx
  mov rbx, QWORD PTR [rbp-72]
  mov r12, rbx
  mov QWORD PTR [rbp-80], r12
  mov rbx, QWORD PTR [rbp-80]
  mov r12, 8
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov rbx, QWORD PTR [rbp-56]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  mov r15, r13
  mov QWORD PTR [r15], r12
  mov rbx, QWORD PTR [rbp-56]
  mov r12, QWORD PTR [rbp-80]
  mov r15, rbx
  mov QWORD PTR [r15], r12
  mov rbx, 0
  mov rax, rbx
  add rsp, 80
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

gheapinit:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 64
  mov rbx, rdi
  mov QWORD PTR [rbp-56], rbx
  lea rbx, [rip+_std_heap]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  mov rbx, r12
  mov r12, 0
  mov r13, r12
  mov r15, rbx
  mov r14, r13
  xor rax, rax
  cmp r15, r14
  sete al
  mov r12, rax
  mov r15, r12
  test r15, r15
  jz .L18
  lea rbx, [rip+_std_heap]
  mov r10, rbx
  mov r12, QWORD PTR [rbp-56]
  mov rdi, r12
  call reserve
  mov r12, rax
  mov r15, rbx
  mov QWORD PTR [r15], r12
  jmp .L19
.L18:
.L19:
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

galloc:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 64
  mov rbx, rdi
  mov QWORD PTR [rbp-56], rbx
  lea rbx, [rip+_std_heap]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  mov rbx, r12
  mov r12, 0
  mov r13, r12
  mov r15, rbx
  mov r14, r13
  xor rax, rax
  cmp r15, r14
  sete al
  mov r12, rax
  mov r15, r12
  test r15, r15
  jz .L20
  lea rbx, [rip+STDERR]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  lea rbx, [rip+.LC1]
  mov r13, 28
  mov rdi, r12
  mov rsi, rbx
  mov rdx, r13
  call write
  mov rbx, rax
  mov rbx, 1
  mov rdi, rbx
  call exit
  mov rbx, rax
  jmp .L21
.L20:
.L21:
  lea rbx, [rip+_std_heap]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  mov rbx, QWORD PTR [rbp-56]
  mov rdi, r12
  mov rsi, rbx
  call alloc
  mov rbx, rax
  mov rax, rbx
  add rsp, 64
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
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

gfree:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 64
  mov rbx, rdi
  mov QWORD PTR [rbp-56], rbx
  lea rbx, [rip+_std_heap]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  mov rbx, QWORD PTR [rbp-56]
  mov rdi, r12
  mov rsi, rbx
  call free
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

memcpy:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 176
  mov rbx, rdi
  mov QWORD PTR [rbp-56], rbx
  mov rbx, rsi
  mov QWORD PTR [rbp-64], rbx
  mov rbx, rdx
  mov QWORD PTR [rbp-72], rbx
  mov rbx, 0
  mov QWORD PTR [rbp-80], rbx
.L22:
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-88], r15
  mov r15, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-112], r15
  mov r15, QWORD PTR [rbp-88]
  mov r14, QWORD PTR [rbp-112]
  xor rax, rax
  cmp r15, r14
  setl al
  mov QWORD PTR [rbp-152], rax
  mov r15, QWORD PTR [rbp-152]
  test r15, r15
  jz .L23
  mov r15, QWORD PTR [rbp-56]
  mov QWORD PTR [rbp-96], r15
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-104], r15
  mov r15, QWORD PTR [rbp-96]
  mov r14, QWORD PTR [rbp-104]
  add r15, r14
  mov rbx, r15
  mov r15, QWORD PTR [rbp-64]
  mov QWORD PTR [rbp-120], r15
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-128], r15
  mov r15, QWORD PTR [rbp-120]
  mov r14, QWORD PTR [rbp-128]
  add r15, r14
  mov QWORD PTR [rbp-136], r15
  mov r15, QWORD PTR [rbp-136]
  movzx rax, BYTE PTR [r15]
  mov QWORD PTR [rbp-144], rax
  mov r15, rbx
  mov rax, QWORD PTR [rbp-144]
  mov BYTE PTR [r15], al
  lea r12, [rbp-80]
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-160], r15
  mov QWORD PTR [rbp-168], 1
  mov r14, QWORD PTR [rbp-168]
  mov r15, r14
  mov QWORD PTR [rbp-176], r15
  mov r15, QWORD PTR [rbp-160]
  mov r14, QWORD PTR [rbp-176]
  add r15, r14
  mov r13, r15
  mov r15, r12
  mov QWORD PTR [r15], r13
  jmp .L22
.L23:
  mov rbx, QWORD PTR [rbp-56]
  mov rax, rbx
  add rsp, 176
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  mov rbx, 0
  mov rax, rbx
  add rsp, 176
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

write:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 80
  mov rbx, rdi
  mov QWORD PTR [rbp-56], rbx
  mov rbx, rsi
  mov QWORD PTR [rbp-64], rbx
  mov rbx, rdx
  mov QWORD PTR [rbp-72], rbx
  mov QWORD PTR [rbp-80], 1
  mov r12, QWORD PTR [rbp-56]
  mov r13, QWORD PTR [rbp-64]
  mov rbx, QWORD PTR [rbp-72]
  mov rax, QWORD PTR [rbp-80]
  mov rdi, r12
  mov rsi, r13
  mov rdx, rbx
  push rcx
  push r11
  syscall
  pop r11
  pop rcx
  mov rbx, rax
  mov rax, rbx
  add rsp, 80
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  mov rbx, 0
  mov rax, rbx
  add rsp, 80
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

mmap:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 128
  mov rbx, rdi
  mov QWORD PTR [rbp-56], rbx
  mov rbx, rsi
  mov QWORD PTR [rbp-64], rbx
  mov rbx, rdx
  mov QWORD PTR [rbp-72], rbx
  mov rbx, rcx
  mov QWORD PTR [rbp-80], rbx
  mov rbx, r8
  mov QWORD PTR [rbp-88], rbx
  mov rbx, r9
  mov QWORD PTR [rbp-96], rbx
  mov QWORD PTR [rbp-104], 9
  mov r12, QWORD PTR [rbp-56]
  mov r13, QWORD PTR [rbp-64]
  mov r15, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-112], r15
  mov r15, QWORD PTR [rbp-80]
  mov QWORD PTR [rbp-120], r15
  mov r15, QWORD PTR [rbp-88]
  mov QWORD PTR [rbp-128], r15
  mov rbx, QWORD PTR [rbp-96]
  mov rax, QWORD PTR [rbp-104]
  mov rdi, r12
  mov rsi, r13
  mov rdx, QWORD PTR [rbp-112]
  mov r10, QWORD PTR [rbp-120]
  mov r8, QWORD PTR [rbp-128]
  mov r9, rbx
  push rcx
  push r11
  syscall
  pop r11
  pop rcx
  mov rbx, rax
  mov r12, rbx
  mov rax, r12
  add rsp, 128
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  mov rbx, 0
  mov rax, rbx
  add rsp, 128
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

exit:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 64
  mov rbx, rdi
  mov DWORD PTR [rbp-52], ebx
  mov rbx, 60
  movsxd r12, DWORD PTR [rbp-52]
  mov rax, rbx
  mov rdi, r12
  push rcx
  push r11
  syscall
  pop r11
  pop rcx
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

print:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 96
  mov rbx, rdi
  lea rdi, [rbp-64]
  mov rsi, rbx
  mov rcx, 16
  cld
  rep movsb
  lea rbx, [rbp-64]
  mov r12, 8
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov r15, r13
  movsxd rbx, DWORD PTR [r15]
  mov r12, 0
  mov r15, rbx
  mov r14, r12
  xor rax, rax
  cmp r15, r14
  setg al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L30
  lea rbx, [rip+STDOUT]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  lea rbx, [rbp-64]
  mov r15, rbx
  mov r13, QWORD PTR [r15]
  lea r15, [rbp-64]
  mov QWORD PTR [rbp-72], r15
  mov QWORD PTR [rbp-80], 8
  mov r15, QWORD PTR [rbp-72]
  mov r14, QWORD PTR [rbp-80]
  add r15, r14
  mov QWORD PTR [rbp-88], r15
  mov r15, QWORD PTR [rbp-88]
  movsxd rax, DWORD PTR [r15]
  mov QWORD PTR [rbp-96], rax
  mov r14, QWORD PTR [rbp-96]
  mov rbx, r14
  mov rdi, r12
  mov rsi, r13
  mov rdx, rbx
  call write
  mov rbx, rax
  jmp .L31
.L30:
.L31:
  mov rbx, 0
  mov rax, rbx
  add rsp, 96
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

println:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 64
  mov rbx, rdi
  lea rdi, [rbp-64]
  mov rsi, rbx
  mov rcx, 16
  cld
  rep movsb
  lea rbx, [rbp-64]
  mov rdi, rbx
  call print
  mov rbx, rax
  lea rbx, [rip+STDOUT]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  lea rbx, [rip+.LC2]
  mov r13, 1
  mov rdi, r12
  mov rsi, rbx
  mov rdx, r13
  call write
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

print_int:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 96
  mov rbx, rdi
  mov QWORD PTR [rbp-56], rbx
  xor r10, r10
  lea rbx, [rbp-72]
  mov r12, QWORD PTR [rbp-56]
  mov rdi, rbx
  mov rsi, r12
  call int_to_string
  mov r12, rax
  lea r12, [rbp-72]
  push rcx
  push rsi
  push rdi
  mov rdi, r12
  mov rsi, rbx
  mov rcx, 16
  cld
  rep movsb
  pop rdi
  pop rsi
  pop rcx
  lea rbx, [rbp-72]
  mov rdi, rbx
  call print
  mov rbx, rax
  lea rbx, [rbp-72]
  mov rdi, rbx
  call string_free
  mov rbx, rax
  mov rbx, 0
  mov rax, rbx
  add rsp, 96
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

strlen:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 128
  mov rbx, rdi
  mov QWORD PTR [rbp-56], rbx
  mov rbx, 0
  mov QWORD PTR [rbp-64], rbx
.L32:
  mov r15, QWORD PTR [rbp-56]
  mov QWORD PTR [rbp-72], r15
  mov r15, QWORD PTR [rbp-64]
  mov QWORD PTR [rbp-104], r15
  mov r15, QWORD PTR [rbp-72]
  mov r14, QWORD PTR [rbp-104]
  add r15, r14
  mov r13, r15
  mov r15, r13
  movzx rax, BYTE PTR [r15]
  mov QWORD PTR [rbp-80], rax
  mov QWORD PTR [rbp-88], 0
  mov r15, QWORD PTR [rbp-80]
  mov r14, QWORD PTR [rbp-88]
  xor rax, rax
  cmp r15, r14
  setne al
  mov QWORD PTR [rbp-96], rax
  mov r15, QWORD PTR [rbp-96]
  test r15, r15
  jz .L33
  lea rbx, [rbp-64]
  mov r15, QWORD PTR [rbp-64]
  mov QWORD PTR [rbp-112], r15
  mov QWORD PTR [rbp-120], 1
  mov r14, QWORD PTR [rbp-120]
  mov r15, r14
  mov QWORD PTR [rbp-128], r15
  mov r15, QWORD PTR [rbp-112]
  mov r14, QWORD PTR [rbp-128]
  add r15, r14
  mov r12, r15
  mov r15, rbx
  mov QWORD PTR [r15], r12
  jmp .L32
.L33:
  mov rbx, QWORD PTR [rbp-64]
  mov rax, rbx
  add rsp, 128
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  mov rbx, 0
  mov rax, rbx
  add rsp, 128
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

getbeforevalue:
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 48
  mov rax, r10
  mov rbx, 0
  mov rax, rbx
  add rsp, 48
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  ret

String:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 320
  mov rbx, rdi
  mov r12, rsi
  mov QWORD PTR [rbp-56], r12
  xor r10, r10
  call getbeforevalue
  mov r12, rax
  mov QWORD PTR [rbp-64], r12
  xor r10, r10
  mov r12, QWORD PTR [rbp-56]
  mov rdi, r12
  call strlen
  mov r12, rax
  mov QWORD PTR [rbp-72], r12
  mov r12, QWORD PTR [rbp-72]
  movsxd r13, r12d
  mov DWORD PTR [rbp-76], r13d
  mov r12, QWORD PTR [rbp-64]
  mov r13, r12
  mov QWORD PTR [rbp-104], 0
  mov r14, QWORD PTR [rbp-104]
  mov r15, r14
  mov QWORD PTR [rbp-112], r15
  mov r15, r13
  mov r14, QWORD PTR [rbp-112]
  xor rax, rax
  cmp r15, r14
  setne al
  mov r12, rax
  mov r15, r12
  test r15, r15
  jz .L34
  mov r12, QWORD PTR [rbp-64]
  mov r15, r12
  mov r13, QWORD PTR [r15]
  mov r12, r13
  mov QWORD PTR [rbp-120], 0
  mov r14, QWORD PTR [rbp-120]
  mov r15, r14
  mov QWORD PTR [rbp-128], r15
  mov r15, r12
  mov r14, QWORD PTR [rbp-128]
  xor rax, rax
  cmp r15, r14
  setne al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L38
  mov r12, QWORD PTR [rbp-64]
  mov QWORD PTR [rbp-136], 12
  mov r15, r12
  mov r14, QWORD PTR [rbp-136]
  add r15, r14
  mov r13, r15
  mov r15, r13
  movsxd r12, DWORD PTR [r15]
  movsxd r15, DWORD PTR [rbp-76]
  mov QWORD PTR [rbp-144], r15
  mov r15, r12
  mov r14, QWORD PTR [rbp-144]
  xor rax, rax
  cmp r15, r14
  setge al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L38
  mov r12, 1
  jmp .L39
.L38:
  mov r12, 0
.L39:
  mov r15, r12
  test r15, r15
  jz .L36
  mov r12, QWORD PTR [rbp-64]
  mov r15, r12
  mov r13, QWORD PTR [r15]
  mov r15, QWORD PTR [rbp-56]
  mov QWORD PTR [rbp-152], r15
  mov r12, QWORD PTR [rbp-72]
  mov rdi, r13
  mov rsi, QWORD PTR [rbp-152]
  mov rdx, r12
  call memcpy
  mov r12, rax
  mov r12, QWORD PTR [rbp-64]
  mov r15, r12
  mov r13, QWORD PTR [r15]
  mov r15, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-160], r15
  mov r15, r13
  mov r14, QWORD PTR [rbp-160]
  add r15, r14
  mov r12, r15
  mov r13, 0
  mov r15, r12
  mov rax, r13
  mov BYTE PTR [r15], al
  lea r12, [rbp-92]
  mov r15, QWORD PTR [rbp-64]
  mov QWORD PTR [rbp-168], r15
  mov r15, QWORD PTR [rbp-168]
  mov r13, QWORD PTR [r15]
  mov r15, r12
  mov QWORD PTR [r15], r13
  lea r12, [rbp-92]
  mov QWORD PTR [rbp-176], 12
  mov r15, r12
  mov r14, QWORD PTR [rbp-176]
  add r15, r14
  mov r13, r15
  mov r15, QWORD PTR [rbp-64]
  mov QWORD PTR [rbp-184], r15
  mov QWORD PTR [rbp-192], 12
  mov r15, QWORD PTR [rbp-184]
  mov r14, QWORD PTR [rbp-192]
  add r15, r14
  mov QWORD PTR [rbp-200], r15
  mov r15, QWORD PTR [rbp-200]
  movsxd r12, DWORD PTR [r15]
  mov r15, r13
  mov rax, r12
  mov DWORD PTR [r15], eax
  lea r12, [rbp-92]
  mov QWORD PTR [rbp-208], 8
  mov r15, r12
  mov r14, QWORD PTR [rbp-208]
  add r15, r14
  mov r13, r15
  movsxd r12, DWORD PTR [rbp-76]
  mov r15, r13
  mov rax, r12
  mov DWORD PTR [r15], eax
  lea r12, [rbp-92]
  push rcx
  push rsi
  push rdi
  mov rdi, rbx
  mov rsi, r12
  mov rcx, 16
  cld
  rep movsb
  pop rdi
  pop rsi
  pop rcx
  mov rax, rbx
  add rsp, 320
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  jmp .L37
.L36:
.L37:
  mov r12, QWORD PTR [rbp-64]
  mov r15, r12
  mov r13, QWORD PTR [r15]
  mov r12, r13
  mov QWORD PTR [rbp-216], 0
  mov r14, QWORD PTR [rbp-216]
  mov r15, r14
  mov QWORD PTR [rbp-224], r15
  mov r15, r12
  mov r14, QWORD PTR [rbp-224]
  xor rax, rax
  cmp r15, r14
  setne al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L40
  mov r12, QWORD PTR [rbp-64]
  mov r15, r12
  mov r13, QWORD PTR [r15]
  mov rdi, r13
  call gfree
  mov r12, rax
  jmp .L41
.L40:
.L41:
  jmp .L35
.L34:
.L35:
  lea r12, [rbp-92]
  mov QWORD PTR [rbp-232], 8
  mov r15, r12
  mov r14, QWORD PTR [rbp-232]
  add r15, r14
  mov r13, r15
  movsxd r12, DWORD PTR [rbp-76]
  mov r15, r13
  mov rax, r12
  mov DWORD PTR [r15], eax
  lea r12, [rbp-92]
  mov QWORD PTR [rbp-240], 12
  mov r15, r12
  mov r14, QWORD PTR [rbp-240]
  add r15, r14
  mov r13, r15
  movsxd r12, DWORD PTR [rbp-76]
  mov r15, r13
  mov rax, r12
  mov DWORD PTR [r15], eax
  mov r12, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-248], 0
  mov r14, QWORD PTR [rbp-248]
  mov r15, r14
  mov QWORD PTR [rbp-256], r15
  mov r15, r12
  mov r14, QWORD PTR [rbp-256]
  xor rax, rax
  cmp r15, r14
  setg al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L42
  lea r12, [rbp-92]
  mov r15, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-264], r15
  mov QWORD PTR [rbp-272], 1
  mov r14, QWORD PTR [rbp-272]
  mov r15, r14
  mov QWORD PTR [rbp-280], r15
  mov r15, QWORD PTR [rbp-264]
  mov r14, QWORD PTR [rbp-280]
  add r15, r14
  mov r13, r15
  mov rdi, r13
  call galloc
  mov QWORD PTR [rbp-288], rax
  mov r14, QWORD PTR [rbp-288]
  mov r13, r14
  mov r15, r12
  mov QWORD PTR [r15], r13
  lea r12, [rbp-92]
  mov r15, r12
  mov r13, QWORD PTR [r15]
  mov r15, QWORD PTR [rbp-56]
  mov QWORD PTR [rbp-296], r15
  mov r12, QWORD PTR [rbp-72]
  mov rdi, r13
  mov rsi, QWORD PTR [rbp-296]
  mov rdx, r12
  call memcpy
  mov r12, rax
  lea r12, [rbp-92]
  mov r15, r12
  mov r13, QWORD PTR [r15]
  mov r15, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-304], r15
  mov r15, r13
  mov r14, QWORD PTR [rbp-304]
  add r15, r14
  mov r12, r15
  mov r13, 0
  mov r15, r12
  mov rax, r13
  mov BYTE PTR [r15], al
  jmp .L43
.L42:
  lea r12, [rbp-92]
  mov QWORD PTR [rbp-312], 0
  mov r14, QWORD PTR [rbp-312]
  mov r13, r14
  mov r15, r12
  mov QWORD PTR [r15], r13
.L43:
  lea r12, [rbp-92]
  push rcx
  push rsi
  push rdi
  mov rdi, rbx
  mov rsi, r12
  mov rcx, 16
  cld
  rep movsb
  pop rdi
  pop rsi
  pop rcx
  mov rax, rbx
  add rsp, 320
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  mov rbx, 0
  mov rax, rbx
  add rsp, 320
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

string_free:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 64
  mov rbx, rdi
  lea rdi, [rbp-64]
  mov rsi, rbx
  mov rcx, 16
  cld
  rep movsb
  lea rbx, [rbp-64]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  mov rbx, r12
  mov r12, 0
  mov r13, r12
  mov r15, rbx
  mov r14, r13
  xor rax, rax
  cmp r15, r14
  setne al
  mov r12, rax
  mov r15, r12
  test r15, r15
  jz .L44
  lea rbx, [rbp-64]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  mov rdi, r12
  call gfree
  mov rbx, rax
  jmp .L45
.L44:
.L45:
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

concat:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 352
  mov rbx, rdi
  mov r12, rsi
  lea rdi, [rbp-64]
  mov rsi, r12
  mov rcx, 16
  cld
  rep movsb
  mov r12, rdx
  lea rdi, [rbp-80]
  mov rsi, r12
  mov rcx, 16
  cld
  rep movsb
  lea r12, [rbp-64]
  mov QWORD PTR [rbp-152], 8
  mov r15, r12
  mov r14, QWORD PTR [rbp-152]
  add r15, r14
  mov r13, r15
  mov r15, r13
  movsxd r12, DWORD PTR [r15]
  mov r13, r12
  mov QWORD PTR [rbp-88], r13
  lea r12, [rbp-80]
  mov QWORD PTR [rbp-160], 8
  mov r15, r12
  mov r14, QWORD PTR [rbp-160]
  add r15, r14
  mov r13, r15
  mov r15, r13
  movsxd r12, DWORD PTR [r15]
  mov r13, r12
  mov QWORD PTR [rbp-96], r13
  mov r12, QWORD PTR [rbp-88]
  mov r15, QWORD PTR [rbp-96]
  mov QWORD PTR [rbp-168], r15
  mov r15, r12
  mov r14, QWORD PTR [rbp-168]
  add r15, r14
  mov r13, r15
  mov QWORD PTR [rbp-104], r13
  lea r12, [rbp-120]
  mov QWORD PTR [rbp-176], 8
  mov r15, r12
  mov r14, QWORD PTR [rbp-176]
  add r15, r14
  mov r13, r15
  mov r15, QWORD PTR [rbp-104]
  mov QWORD PTR [rbp-184], r15
  mov r14, QWORD PTR [rbp-184]
  movsxd r12, r14d
  mov r15, r13
  mov rax, r12
  mov DWORD PTR [r15], eax
  lea r12, [rbp-120]
  mov QWORD PTR [rbp-192], 12
  mov r15, r12
  mov r14, QWORD PTR [rbp-192]
  add r15, r14
  mov r13, r15
  mov r15, QWORD PTR [rbp-104]
  mov QWORD PTR [rbp-200], r15
  mov r14, QWORD PTR [rbp-200]
  movsxd r12, r14d
  mov r15, r13
  mov rax, r12
  mov DWORD PTR [r15], eax
  mov r12, QWORD PTR [rbp-104]
  mov QWORD PTR [rbp-208], 0
  mov r14, QWORD PTR [rbp-208]
  mov r15, r14
  mov QWORD PTR [rbp-216], r15
  mov r15, r12
  mov r14, QWORD PTR [rbp-216]
  xor rax, rax
  cmp r15, r14
  setg al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L46
  lea r12, [rbp-120]
  mov r15, QWORD PTR [rbp-104]
  mov QWORD PTR [rbp-224], r15
  mov QWORD PTR [rbp-232], 1
  mov r14, QWORD PTR [rbp-232]
  mov r15, r14
  mov QWORD PTR [rbp-240], r15
  mov r15, QWORD PTR [rbp-224]
  mov r14, QWORD PTR [rbp-240]
  add r15, r14
  mov r13, r15
  mov rdi, r13
  call galloc
  mov QWORD PTR [rbp-248], rax
  mov r14, QWORD PTR [rbp-248]
  mov r13, r14
  mov r15, r12
  mov QWORD PTR [r15], r13
  mov r12, QWORD PTR [rbp-88]
  mov QWORD PTR [rbp-256], 0
  mov r14, QWORD PTR [rbp-256]
  mov r15, r14
  mov QWORD PTR [rbp-264], r15
  mov r15, r12
  mov r14, QWORD PTR [rbp-264]
  xor rax, rax
  cmp r15, r14
  setg al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L48
  lea r12, [rbp-120]
  mov r15, r12
  mov r13, QWORD PTR [r15]
  lea r15, [rbp-64]
  mov QWORD PTR [rbp-272], r15
  mov r15, QWORD PTR [rbp-272]
  mov rax, QWORD PTR [r15]
  mov QWORD PTR [rbp-280], rax
  mov r12, QWORD PTR [rbp-88]
  mov rdi, r13
  mov rsi, QWORD PTR [rbp-280]
  mov rdx, r12
  call memcpy
  mov r12, rax
  jmp .L49
.L48:
.L49:
  mov r12, QWORD PTR [rbp-96]
  mov QWORD PTR [rbp-288], 0
  mov r14, QWORD PTR [rbp-288]
  mov r15, r14
  mov QWORD PTR [rbp-296], r15
  mov r15, r12
  mov r14, QWORD PTR [rbp-296]
  xor rax, rax
  cmp r15, r14
  setg al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L50
  lea r12, [rbp-120]
  mov r15, r12
  mov r13, QWORD PTR [r15]
  mov r12, r13
  mov r15, QWORD PTR [rbp-88]
  mov QWORD PTR [rbp-304], r15
  mov r15, r12
  mov r14, QWORD PTR [rbp-304]
  add r15, r14
  mov r13, r15
  mov QWORD PTR [rbp-128], r13
  mov r12, QWORD PTR [rbp-128]
  mov r13, r12
  lea r15, [rbp-80]
  mov QWORD PTR [rbp-312], r15
  mov r15, QWORD PTR [rbp-312]
  mov rax, QWORD PTR [r15]
  mov QWORD PTR [rbp-320], rax
  mov r12, QWORD PTR [rbp-96]
  mov rdi, r13
  mov rsi, QWORD PTR [rbp-320]
  mov rdx, r12
  call memcpy
  mov r12, rax
  jmp .L51
.L50:
.L51:
  lea r12, [rbp-120]
  mov r15, r12
  mov r13, QWORD PTR [r15]
  mov r12, r13
  mov r15, QWORD PTR [rbp-104]
  mov QWORD PTR [rbp-328], r15
  mov r15, r12
  mov r14, QWORD PTR [rbp-328]
  add r15, r14
  mov r13, r15
  mov QWORD PTR [rbp-136], r13
  mov r12, QWORD PTR [rbp-136]
  mov r13, r12
  mov QWORD PTR [rbp-144], r13
  mov r12, QWORD PTR [rbp-144]
  mov QWORD PTR [rbp-336], 0
  mov r15, r12
  mov r14, QWORD PTR [rbp-336]
  add r15, r14
  mov r13, r15
  mov r12, 0
  mov r15, r13
  mov rax, r12
  mov BYTE PTR [r15], al
  jmp .L47
.L46:
  lea r12, [rbp-120]
  mov QWORD PTR [rbp-344], 0
  mov r14, QWORD PTR [rbp-344]
  mov r13, r14
  mov r15, r12
  mov QWORD PTR [r15], r13
.L47:
  lea r12, [rbp-120]
  push rcx
  push rsi
  push rdi
  mov rdi, rbx
  mov rsi, r12
  mov rcx, 16
  cld
  rep movsb
  pop rdi
  pop rsi
  pop rcx
  mov rax, rbx
  add rsp, 352
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  mov rbx, 0
  mov rax, rbx
  add rsp, 352
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret

int_to_string:
  push rbp
  mov rbp, rsp
  push rbx
  push r12
  push r13
  push r14
  push r15
  sub rsp, 560
  mov QWORD PTR [rbp-320], rdi
  mov r12, rsi
  mov QWORD PTR [rbp-56], r12
  mov r12, QWORD PTR [rbp-56]
  mov QWORD PTR [rbp-64], r12
  mov r12, 0
  mov QWORD PTR [rbp-72], r12
  mov r12, 0
  mov QWORD PTR [rbp-80], r12
  mov r12, QWORD PTR [rbp-56]
  mov QWORD PTR [rbp-176], 0
  mov r14, QWORD PTR [rbp-176]
  mov r15, r14
  mov QWORD PTR [rbp-184], r15
  mov r15, r12
  mov r14, QWORD PTR [rbp-184]
  xor rax, rax
  cmp r15, r14
  sete al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L52
  lea r12, [rbp-72]
  mov r13, 1
  mov r15, r12
  mov QWORD PTR [r15], r13
  jmp .L53
.L52:
  mov r12, QWORD PTR [rbp-56]
  mov QWORD PTR [rbp-192], 0
  mov r14, QWORD PTR [rbp-192]
  mov r15, r14
  mov QWORD PTR [rbp-200], r15
  mov r15, r12
  mov r14, QWORD PTR [rbp-200]
  xor rax, rax
  cmp r15, r14
  setl al
  mov r13, rax
  mov r15, r13
  test r15, r15
  jz .L54
  lea r12, [rbp-80]
  mov r13, 1
  mov r15, r12
  mov QWORD PTR [r15], r13
  lea r12, [rbp-64]
  mov QWORD PTR [rbp-208], 0
  mov r15, QWORD PTR [rbp-56]
  mov QWORD PTR [rbp-216], r15
  mov r14, QWORD PTR [rbp-208]
  mov r15, r14
  mov QWORD PTR [rbp-224], r15
  mov r15, QWORD PTR [rbp-224]
  mov r14, QWORD PTR [rbp-216]
  sub r15, r14
  mov r13, r15
  mov r15, r12
  mov QWORD PTR [r15], r13
  lea r12, [rbp-72]
  mov r15, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-232], r15
  mov QWORD PTR [rbp-240], 1
  mov r14, QWORD PTR [rbp-240]
  mov r15, r14
  mov QWORD PTR [rbp-248], r15
  mov r15, QWORD PTR [rbp-232]
  mov r14, QWORD PTR [rbp-248]
  add r15, r14
  mov r13, r15
  mov r15, r12
  mov QWORD PTR [r15], r13
  jmp .L55
.L54:
.L55:
  mov r12, QWORD PTR [rbp-64]
  mov QWORD PTR [rbp-88], r12
.L56:
  mov r15, QWORD PTR [rbp-88]
  mov QWORD PTR [rbp-256], r15
  mov QWORD PTR [rbp-280], 0
  mov r14, QWORD PTR [rbp-280]
  mov r15, r14
  mov QWORD PTR [rbp-264], r15
  mov r15, QWORD PTR [rbp-256]
  mov r14, QWORD PTR [rbp-264]
  xor rax, rax
  cmp r15, r14
  setg al
  mov QWORD PTR [rbp-272], rax
  mov r15, QWORD PTR [rbp-272]
  test r15, r15
  jz .L57
  lea r12, [rbp-88]
  mov r15, QWORD PTR [rbp-88]
  mov QWORD PTR [rbp-288], r15
  mov QWORD PTR [rbp-296], 10
  mov r14, QWORD PTR [rbp-296]
  mov r15, r14
  mov QWORD PTR [rbp-304], r15
  mov rax, QWORD PTR [rbp-288]
  mov r15, QWORD PTR [rbp-304]
  push rdx
  cqo
  idiv r15
  mov r15, rax
  pop rdx
  mov QWORD PTR [rbp-312], r15
  mov r15, r12
  mov rax, QWORD PTR [rbp-312]
  mov QWORD PTR [r15], rax
  lea r13, [rbp-72]
  mov r15, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-328], r15
  mov QWORD PTR [rbp-336], 1
  mov r14, QWORD PTR [rbp-336]
  mov r15, r14
  mov QWORD PTR [rbp-344], r15
  mov r15, QWORD PTR [rbp-328]
  mov r14, QWORD PTR [rbp-344]
  add r15, r14
  mov rbx, r15
  mov r15, r13
  mov QWORD PTR [r15], rbx
  jmp .L56
.L57:
.L53:
  lea rbx, [rbp-104]
  mov r12, 8
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov rbx, QWORD PTR [rbp-72]
  movsxd r12, ebx
  mov r15, r13
  mov rax, r12
  mov DWORD PTR [r15], eax
  lea rbx, [rbp-104]
  mov r12, 12
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov rbx, QWORD PTR [rbp-72]
  movsxd r12, ebx
  mov r15, r13
  mov rax, r12
  mov DWORD PTR [r15], eax
  lea rbx, [rbp-104]
  mov r12, QWORD PTR [rbp-72]
  mov QWORD PTR [rbp-352], 1
  mov r14, QWORD PTR [rbp-352]
  mov r15, r14
  mov QWORD PTR [rbp-360], r15
  mov r15, r12
  mov r14, QWORD PTR [rbp-360]
  add r15, r14
  mov r13, r15
  mov rdi, r13
  call galloc
  mov r12, rax
  mov r13, r12
  mov r15, rbx
  mov QWORD PTR [r15], r13
  mov rbx, QWORD PTR [rbp-72]
  mov r12, 1
  mov r13, r12
  mov r15, rbx
  mov r14, r13
  sub r15, r14
  mov r12, r15
  mov QWORD PTR [rbp-112], r12
  mov rbx, QWORD PTR [rbp-56]
  mov r12, 0
  mov r13, r12
  mov r15, rbx
  mov r14, r13
  xor rax, rax
  cmp r15, r14
  sete al
  mov r12, rax
  mov r15, r12
  test r15, r15
  jz .L58
  lea rbx, [rbp-104]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  mov QWORD PTR [rbp-120], r12
  mov rbx, QWORD PTR [rbp-120]
  mov r12, 0
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov rbx, 48
  mov r15, r13
  mov rax, rbx
  mov BYTE PTR [r15], al
  jmp .L59
.L58:
  mov rbx, QWORD PTR [rbp-64]
  mov QWORD PTR [rbp-128], rbx
.L60:
  mov r15, QWORD PTR [rbp-128]
  mov QWORD PTR [rbp-368], r15
  mov QWORD PTR [rbp-448], 0
  mov r14, QWORD PTR [rbp-448]
  mov r15, r14
  mov QWORD PTR [rbp-496], r15
  mov r15, QWORD PTR [rbp-368]
  mov r14, QWORD PTR [rbp-496]
  xor rax, rax
  cmp r15, r14
  setg al
  mov QWORD PTR [rbp-376], rax
  mov r15, QWORD PTR [rbp-376]
  test r15, r15
  jz .L61
  mov r15, QWORD PTR [rbp-128]
  mov QWORD PTR [rbp-384], r15
  mov QWORD PTR [rbp-392], 10
  mov r14, QWORD PTR [rbp-392]
  mov r15, r14
  mov QWORD PTR [rbp-400], r15
  mov rax, QWORD PTR [rbp-384]
  mov r15, QWORD PTR [rbp-400]
  push rdx
  cqo
  idiv r15
  mov r15, rdx
  pop rdx
  mov QWORD PTR [rbp-408], r15
  mov r15, QWORD PTR [rbp-408]
  mov QWORD PTR [rbp-136], r15
  lea r15, [rbp-104]
  mov QWORD PTR [rbp-416], r15
  mov r15, QWORD PTR [rbp-416]
  mov rax, QWORD PTR [r15]
  mov QWORD PTR [rbp-424], rax
  mov r15, QWORD PTR [rbp-424]
  mov QWORD PTR [rbp-144], r15
  mov r15, QWORD PTR [rbp-144]
  mov QWORD PTR [rbp-432], r15
  mov r15, QWORD PTR [rbp-112]
  mov QWORD PTR [rbp-440], r15
  mov r15, QWORD PTR [rbp-432]
  mov r14, QWORD PTR [rbp-440]
  add r15, r14
  mov QWORD PTR [rbp-536], r15
  mov r15, QWORD PTR [rbp-136]
  mov QWORD PTR [rbp-456], r15
  mov QWORD PTR [rbp-464], 48
  mov r14, QWORD PTR [rbp-464]
  mov r15, r14
  mov QWORD PTR [rbp-472], r15
  mov r15, QWORD PTR [rbp-456]
  mov r14, QWORD PTR [rbp-472]
  add r15, r14
  mov QWORD PTR [rbp-480], r15
  mov r14, QWORD PTR [rbp-480]
  movzx r15, r14b
  mov QWORD PTR [rbp-488], r15
  mov r15, QWORD PTR [rbp-536]
  mov rax, QWORD PTR [rbp-488]
  mov BYTE PTR [r15], al
  lea r12, [rbp-128]
  mov r15, QWORD PTR [rbp-128]
  mov QWORD PTR [rbp-504], r15
  mov QWORD PTR [rbp-512], 10
  mov r14, QWORD PTR [rbp-512]
  mov r15, r14
  mov QWORD PTR [rbp-520], r15
  mov rax, QWORD PTR [rbp-504]
  mov r15, QWORD PTR [rbp-520]
  push rdx
  cqo
  idiv r15
  mov r15, rax
  pop rdx
  mov QWORD PTR [rbp-528], r15
  mov r15, r12
  mov rax, QWORD PTR [rbp-528]
  mov QWORD PTR [r15], rax
  lea r13, [rbp-112]
  mov r15, QWORD PTR [rbp-112]
  mov QWORD PTR [rbp-544], r15
  mov QWORD PTR [rbp-552], 1
  mov r14, QWORD PTR [rbp-552]
  mov r15, r14
  mov QWORD PTR [rbp-560], r15
  mov r15, QWORD PTR [rbp-544]
  mov r14, QWORD PTR [rbp-560]
  sub r15, r14
  mov rbx, r15
  mov r15, r13
  mov QWORD PTR [r15], rbx
  jmp .L60
.L61:
  mov rbx, QWORD PTR [rbp-80]
  mov r12, 1
  mov r13, r12
  mov r15, rbx
  mov r14, r13
  xor rax, rax
  cmp r15, r14
  sete al
  mov r12, rax
  mov r15, r12
  test r15, r15
  jz .L62
  lea rbx, [rbp-104]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  mov QWORD PTR [rbp-152], r12
  mov rbx, QWORD PTR [rbp-152]
  mov r12, 0
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov rbx, 45
  mov r15, r13
  mov rax, rbx
  mov BYTE PTR [r15], al
  jmp .L63
.L62:
.L63:
.L59:
  lea rbx, [rbp-104]
  mov r15, rbx
  mov r12, QWORD PTR [r15]
  mov rbx, r12
  mov r12, QWORD PTR [rbp-72]
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov QWORD PTR [rbp-160], r13
  mov rbx, QWORD PTR [rbp-160]
  mov r12, rbx
  mov QWORD PTR [rbp-168], r12
  mov rbx, QWORD PTR [rbp-168]
  mov r12, 0
  mov r15, rbx
  mov r14, r12
  add r15, r14
  mov r13, r15
  mov rbx, 0
  mov r15, r13
  mov rax, rbx
  mov BYTE PTR [r15], al
  lea rbx, [rbp-104]
  push rcx
  push rsi
  push rdi
  mov rdi, QWORD PTR [rbp-320]
  mov rsi, rbx
  mov rcx, 16
  cld
  rep movsb
  pop rdi
  pop rsi
  pop rcx
  mov rax, QWORD PTR [rbp-320]
  add rsp, 560
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret
  mov rbx, 0
  mov rax, rbx
  add rsp, 560
  pop r15
  pop r14
  pop r13
  pop r12
  pop rbx
  pop rbp
  ret


# _start Entry Stub

Every non-freestanding static executable produced by caustic-ld begins with a
28-byte, target-specific `_start` stub. The stub bridges the kernel's process
entry convention to the Caustic `main` convention, then performs a clean
process exit. Both the x86_64 and AArch64 variants deliberately have the same
size so the shared linker layout stays architecture-independent.

## Why _start Exists

The Linux kernel does not call main() directly. When exec() loads an ELF executable, it
transfers control to the address in e_entry (the entry point). By convention this is a symbol
named _start, not main. The kernel sets up the initial stack with argc, argv, and envp but
does not follow the C function call ABI — there is no call instruction, so there is no return
address on the stack.

_start reads argc and argv from the stack, sets up a proper call frame, calls main(), and
then exits the process using the return value as the exit code.

## AArch64 Machine Code

The AArch64 stub reads `argc` and `argv` without changing the initial stack,
branches to `main`, and invokes Linux `exit` through syscall 93:

```asm
_start:
    ldr  x0, [sp]          // argc
    add  x1, sp, #8        // argv
    bl   main
    movz x8, #93
    svc  #0
    brk  #0
    brk  #0                // padding; total size remains 28 bytes
```

The linker patches the immediate in `bl main` after final symbol addresses are
known.

## x86_64 Machine Code

The x86_64 variant is emitted directly by caustic-ld:

```asm
_start:
    xor  rbp, rbp           ; 48 31 ED        (3 bytes)
                            ; clear frame pointer — signals outermost stack frame
                            ; required by System V ABI for stack unwinders

    pop  rdi                ; 5F               (1 byte)
                            ; argc — on entry, rsp points to argc on the stack
                            ; pop increments rsp past it and puts value in rdi (arg1)

    mov  rsi, rsp           ; 48 89 E6        (3 bytes)
                            ; argv — after popping argc, rsp points to argv[0]
                            ; rsi = argv (arg2), a pointer to the array of string pointers

    and  rsp, -16           ; 48 83 E4 F0     (4 bytes)
                            ; align stack to 16-byte boundary before call
                            ; System V ABI requires rsp % 16 == 0 at CALL instruction
                            ; (which pushes 8 bytes, so rsp % 16 == 8 when main starts)

    call main               ; E8 <rel32>      (5 bytes)
                            ; call Caustic main(argc as i64, argv as **u8) as i64

    mov  rdi, rax           ; 48 89 C7        (3 bytes)
                            ; exit code — main's return value is in rax (System V ABI)
                            ; move it to rdi for sys_exit (first argument)

    mov  rax, 60            ; 48 C7 C0 3C 00 00 00   (7 bytes)
                            ; sys_exit syscall number = 60 on x86_64 Linux

    syscall                 ; 0F 05            (2 bytes)
                            ; invoke the kernel — process terminates here
```

Total: 3 + 1 + 3 + 4 + 5 + 3 + 7 + 2 = 28 bytes.

## Byte Sequence

```
48 31 ED
5F
48 89 E6
48 83 E4 F0
E8 xx xx xx xx
48 89 C7
48 C7 C0 3C 00 00 00
0F 05
```

The `call main` instruction uses a PC-relative 32-bit offset (R_X86_64_PC32 with addend -4).
caustic-ld resolves this immediately when emitting the stub, since main's address is known
after symbol resolution.

## main Signature in Caustic

The expected signature for the entry point:

```cst
fn main() as i64 {
    return 0;
}
```

Or with command-line argument access:

```cst
fn main(argc as i64, argv as **u8) as i64 {
    return 0;
}
```

The stub always passes argc in rdi and argv in rsi. If main does not declare those parameters,
the Caustic ABI ignores extra arguments without corruption (they are simply unused registers).

## Exit Code Behavior

The return value of main (a 64-bit integer in rax) is passed directly to sys_exit as the exit
code. The kernel truncates it to 8 bits (0-255). Values outside this range wrap around.

```bash
./program
echo $?    # prints the exit code (0-255)
```

## Stack Layout at _start Entry

```
rsp →  [ argc         ]   8 bytes
       [ argv[0]      ]   8 bytes pointer to first argument string
       [ argv[1]      ]   8 bytes pointer to second argument string
       [ ...          ]
       [ argv[argc-1] ]   8 bytes pointer to last argument string
       [ NULL         ]   8 bytes null terminator
       [ envp[0]      ]   8 bytes pointer to first environment string
       [ ...          ]
       [ NULL         ]   8 bytes null terminator
```

After `pop rdi`, rsp points to argv[0]. After `and rsp, -16`, the stack is aligned.

## Placement in Output File

The stub is placed immediately after the ELF header and program headers, before the merged
.text section. This means _start is at a fixed offset (176 bytes from the start of the file)
in the executable. Its virtual address is 0x400000 + 176 = 0x4000B0 for a standard load.

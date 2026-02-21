# Inline Assembly

`asm("instructions\n")` emits raw x86_64 assembly directly into the compiler output.

## Syntax

```cst
asm("instruction\n");
```

The string is inserted verbatim into the generated `.s` file. Each instruction must end with `\n`.

## Basic Examples

```cst
// Single instruction
asm("nop\n");

// Move immediate to register
asm("mov rax, 1\n");

// Multiple instructions
asm("push rbx\npush rcx\npop rcx\npop rbx\n");
```

## Practical Example: CPUID

```cst
fn get_cpu_vendor() as void {
    asm("mov eax, 0\n");
    asm("cpuid\n");
    // ebx:edx:ecx now contain vendor string
}
```

## Practical Example: Read Timestamp Counter

```cst
fn rdtsc_low() as i64 {
    asm("rdtsc\n");
    // Result is in edx:eax, rax has the low 32 bits
    // The return value convention uses rax
    return 0;  // rax already holds the value from rdtsc
}
```

## Warnings

- **Register clobbering**: The compiler does not track which registers inline assembly uses. If you modify registers that the compiler expects to be preserved (rbx, r12-r15, rbp, rsp), your program will break.
- **No input/output constraints**: Unlike GCC's `asm()` with constraints, Caustic simply emits the string. You must manually coordinate with the compiler's register allocation.
- **Calling convention**: If used inside a function, be aware that the compiler may have values in any GPR or SSE register.
- **Syntax**: Uses Intel syntax (destination first), matching the rest of Caustic's assembly output.

## When to Use

- Performance-critical tight loops
- CPU feature detection (cpuid, rdtsc)
- Atomic operations not exposed by the language
- Debugging (e.g., `asm("int3\n")` for a breakpoint)

In most cases, prefer `syscall()` for OS interaction and normal Caustic code for everything else.

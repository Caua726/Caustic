# Function Prologue

The function prologue is the sequence of instructions emitted at the start of every function. It establishes the stack frame, saves callee-saved registers, and allocates space for local variables and spill slots.

## Prologue Sequence

```asm
function_name:
  push rbp                  ; save caller's frame pointer
  mov rbp, rsp              ; establish new frame pointer
  push rbx                  ; save callee-saved registers
  push r12
  push r13
  push r14
  push r15
  sub rsp, <aligned_size>   ; allocate stack space
```

## Step-by-Step Breakdown

### 1. Save Frame Pointer

```asm
push rbp
mov rbp, rsp
```

The caller's `rbp` is saved on the stack, then `rbp` is set to the current stack pointer. All local variable accesses and spill slot accesses use `rbp`-relative addressing (`[rbp - offset]`).

### 2. Save Callee-Saved Registers

```asm
push rbx
push r12
push r13
push r14
push r15
```

All five callee-saved general-purpose registers are pushed unconditionally. This is a simplification -- a more optimized compiler would only save registers that are actually used. Caustic saves all five because:
- `rbx`, `r12`, `r13` may be used by the register allocator.
- `r14`, `r15` are always used as scratch registers.

After these 5 pushes (40 bytes) plus the `push rbp` (8 bytes) and the return address pushed by `call` (8 bytes), the stack is at an offset of 56 bytes from its 16-byte-aligned entry point.

### 3. Allocate Stack Space

```asm
sub rsp, <aligned_size>
```

The `aligned_size` is computed by `gen_func` to accommodate:
- **Local variables**: `func.stack_size` bytes (computed by semantic analysis)
- **Spill slots**: `ctx.stack_slots * 8` bytes (determined by register allocation)
- **Allocation space**: `func.alloc_stack_size` bytes (for SRET buffers and enum payloads)

## Stack Alignment

The total stack frame size must maintain 16-byte alignment. The alignment calculation accounts for the fixed overhead:

- Return address: 8 bytes
- `push rbp`: 8 bytes
- 5 callee-saved pushes: 40 bytes
- Total fixed: 56 bytes (56 mod 16 = 8)

Therefore, the variable portion (`sub rsp, N`) must be congruent to 8 mod 16 to achieve overall 16-byte alignment. The code enforces this:

```cst
if (total % 16 != 8) {
    total = total + (8 - (total % 8));
    if (total % 16 != 8) { total = total + 8; }
}
if (total < 8) { total = 8; }
```

A minimum of 8 bytes is always allocated to ensure the alignment invariant holds.

## Stack Frame After Prologue

```
Higher addresses
  +------------------+
  | return address   |  [rbp + 8]
  +------------------+
  | saved rbp        |  [rbp]
  +------------------+
  | saved rbx        |  [rbp - 8]
  +------------------+
  | saved r12        |  [rbp - 16]
  +------------------+
  | saved r13        |  [rbp - 24]
  +------------------+
  | saved r14        |  [rbp - 32]
  +------------------+
  | saved r15        |  [rbp - 40]
  +------------------+
  | local variables  |  [rbp - 48] to [rbp - 48 - locals]
  +------------------+
  | spill slots      |  continuing downward
  +------------------+
  | alloc space      |  continuing downward
  +------------------+  <- rsp (16-byte aligned)
Lower addresses
```

Note: The callee-saved register saves overlap with the local variable region from the perspective of `rbp` offsets. The semantic analysis assigns variable offsets starting from a base that accounts for the saved registers (typically starting at offset 48).

## Function Label

Before the prologue, the function's assembly-level name is emitted as a label:

```asm
function_name:
```

All reachable functions also have a `.globl function_name` directive emitted earlier in the `.text` section, making them visible to the linker.

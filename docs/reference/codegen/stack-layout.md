# Stack Layout

Every Caustic function establishes a standard stack frame in its prologue. This document
describes the layout and how each region is addressed.

## Frame Diagram

```
Higher addresses (toward caller)
  +---------------------------+
  | stack args (arg 6+)       |  [rbp + 16 + (idx-6)*8]
  +---------------------------+
  | return address            |  [rbp + 8]   (pushed by call)
  +---------------------------+
  | saved rbp                 |  [rbp]       (push rbp)
  +---------------------------+
  | saved rbx                 |  [rbp - 8]
  +---------------------------+
  | saved r12                 |  [rbp - 16]
  +---------------------------+
  | saved r13                 |  [rbp - 24]
  +---------------------------+
  | saved r14                 |  [rbp - 32]
  +---------------------------+
  | saved r15                 |  [rbp - 40]
  +---------------------------+
  | local variables           |  [rbp - 48] downward
  |   (func.stack_size bytes) |
  +---------------------------+
  | spill slots               |  [rbp - (48 + func.stack_size)] downward
  |   (stack_slots * 8 bytes) |
  +---------------------------+
  | alloc space               |  (SRET buffers, enum payloads)
  |   (func.alloc_stack_size) |
  +---------------------------+  <- rsp (16-byte aligned at any call)
Lower addresses
```

## Fixed-Size Overhead

The prologue pushes a fixed set of registers before any variable-size allocation:

| Instruction | Bytes used | Cumulative |
|-------------|-----------|------------|
| `call` (return addr) | 8 | 8 |
| `push rbp` | 8 | 16 |
| `push rbx` | 8 | 24 |
| `push r12` | 8 | 32 |
| `push r13` | 8 | 40 |
| `push r14` | 8 | 48 |
| `push r15` | 8 | 56 |

The fixed overhead is always 56 bytes. Since 56 mod 16 = 8, the `sub rsp, N` that follows must
satisfy `N % 16 == 8` to achieve 16-byte alignment at any subsequent `call`.

## Variable-Size Regions

### Local Variables (`func.stack_size`)

Assigned by the semantic analysis phase. Each local variable gets a fixed offset from `rbp`,
starting at 48 (immediately below the saved callee registers). Variable addresses are computed
with `IR_ADDR`:

```asm
lea r15, [rbp-<offset>]
```

### Spill Slots (`ctx.stack_slots * 8`)

Assigned by the register allocator when more vregs are live simultaneously than there are
allocatable registers (rbx, r12, r13). Each spill slot is 8 bytes wide.

The base offset for spill slots is `ctx.arg_spill_base`, which is set to `func.stack_size` so
spill slots start immediately after the last local variable:

```
spill_slot_offset = arg_spill_base + slot_index * 8
```

where `slot_index` starts at 1 (slot 0 is reserved; `next_spill` begins at 1).

A spill slot is accessed as a negative `vreg_to_loc` entry:

```asm
; vreg_to_loc[v] = -k (where k >= 1)
; offset = arg_spill_base + k * 8
mov r15, QWORD PTR [rbp-<offset>]
```

### Alloc Space (`func.alloc_stack_size`)

Reserved for SRET return buffers and any other compiler-generated stack allocations (e.g., enum
payload buffers). Addressed via `IR_GET_ALLOC_ADDR`:

```asm
lea r15, [rbp-<arg_spill_base + stack_slots*8 + alloc_offset>]
```

## Stack Alignment Enforcement

The total variable-size allocation (`local_size + spill_size + alloc_size`) is rounded up to
satisfy `total % 16 == 8`:

```cst
if (total % 16 != 8) {
    total = total + (8 - (total % 8));
    if (total % 16 != 8) { total = total + 8; }
}
if (total < 8) { total = 8; }
```

A minimum of 8 bytes is always allocated. This ensures that even leaf functions with no locals
or spills maintain the ABI alignment requirement.

## Accessing Stack-Passed Arguments (index >= 6)

Arguments beyond the 6th are pushed by the caller before `call`. They appear above the return
address in the caller's frame, accessible from the callee as:

```
offset = 16 + (arg_index - 6) * 8
```

```asm
mov r15, QWORD PTR [rbp+16]   ; arg 6
mov r15, QWORD PTR [rbp+24]   ; arg 7
mov r15, QWORD PTR [rbp+32]   ; arg 8
```

## Example Frame (2 locals, 1 spill slot)

Assume:
- `func.stack_size = 16` (two 8-byte locals at offsets 8 and 16 from top of locals)
- `ctx.stack_slots = 1` (one spill slot)
- `func.alloc_stack_size = 0`
- `ctx.arg_spill_base = 16`

The allocator sets `sub rsp, 24` (16 + 8 = 24, and 24 % 16 = 8, satisfying alignment).

```
[rbp + 8]   = return address
[rbp]       = saved rbp
[rbp - 8]   = saved rbx
[rbp - 16]  = saved r12
[rbp - 24]  = saved r13
[rbp - 32]  = saved r14
[rbp - 40]  = saved r15
[rbp - 48]  = local variable 1  (stack_size offset 8)
[rbp - 56]  = local variable 2  (stack_size offset 16)
[rbp - 64]  = spill slot 1      (arg_spill_base=16, slot=1 -> offset 16+8=24)
rsp         = [rbp - 64]
```

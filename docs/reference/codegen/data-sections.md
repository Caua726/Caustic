# Data Sections

Caustic's codegen emits four assembly sections: `.rodata`, `.data`, `.bss`, and `.text`. This
document describes the three data sections and how global variables and string literals are laid
out in the output assembly.

## Section Overview

| Section | Contents | Permissions |
|---------|----------|-------------|
| `.rodata` | String literal constants | Read-only |
| `.data` | Initialized global variables | Read + Write |
| `.bss` | Uninitialized global variables | Read + Write |
| `.text` | Executable code | Read + Execute |

The linker (`caustic-ld`) maps these into two PT_LOAD segments in the ELF executable:
- **R+X segment**: `.text` and `.rodata`
- **R+W segment**: `.data` and `.bss`

## .rodata -- Read-Only Data

The `.rodata` section contains all string literal constants used in the program. Each string is
registered during IR generation via `register_string()` and assigned a unique integer ID.

### Format

```asm
.section .rodata
.LC0:
  .string "hello world\n"
.LC1:
  .string "error: %d\n"
```

Each string entry emits:
1. A label `.LCN` where N is the string's ID.
2. A `.string` directive containing the escaped string content.

### Escape Sequences

The codegen handles these escape sequences when writing string data:

| Character | Output |
|-----------|--------|
| Newline (0x0A) | `\n` |
| Tab (0x09) | `\t` |
| Null (0x00) | `\0` |
| Double quote (0x22) | `\"` |
| Backslash (0x5C) | `\\` |

All other bytes are emitted as-is.

### String References in Code

In the IR, string literals become `IR_ADDR_GLOBAL` instructions referencing the `.LCN` label:

```asm
lea r15, [rip+.LC0]         ; load address of string constant
```

The `.string` assembler directive automatically appends a null terminator.

## .data -- Initialized Globals

The `.data` section contains global variables that have an explicit initialization value. A
global variable appears here when `IRGlobal.is_initialized == 1` and `IRGlobal.is_reachable == 1`.

### Format

```asm
.data
.globl my_counter
my_counter:
  .quad 42
.globl my_flag
my_flag:
  .byte 1
```

Each initialized global emits:
1. A `.globl` directive making the symbol visible to the linker.
2. A label with the global's assembly name.
3. A size-appropriate data directive.

### Data Directives by Size

| Size (bytes) | Directive | Example |
|-------------|-----------|---------|
| 8 | `.quad` | `.quad 42` |
| 4 | `.long` | `.long 100` |
| 2 | `.short` | `.short 256` |
| 1 | `.byte` | `.byte 1` |
| Other | `.zero` | `.zero 32` (for structs/arrays) |

For sizes that do not match 1, 2, 4, or 8 (e.g., structs or arrays), the `.zero` directive is
used, which zero-fills the specified number of bytes. This means struct globals with non-zero
initializers are not currently supported -- only scalar types can have meaningful `init_value`
in the `.data` section.

### Initialization Values

The `init_value` field on `IRGlobal` is a single `i64` computed by `eval_const_expr()` during IR
generation. This function evaluates compile-time constant expressions involving:
- Integer literals (`NK_NUM`)
- Addition (`NK_ADD`)
- Subtraction (`NK_SUB`)
- Multiplication (`NK_MUL`)
- Division (`NK_DIV`)

Non-constant expressions (function calls, variable references) cause a compile error.

## .bss -- Uninitialized Globals

The `.bss` section contains global variables without an explicit initializer. These are
zero-initialized by the operating system's program loader. A global appears here when
`IRGlobal.is_initialized == 0` and `IRGlobal.is_reachable == 1`.

### Format

```asm
.bss
.globl buffer
buffer:
  .zero 1024
.globl counter
counter:
  .zero 8
```

Every uninitialized global uses `.zero` regardless of its type, since the section is
zero-initialized by default. The `.zero` directive specifies the number of bytes to reserve.

### .bss vs .data

Using `.bss` for uninitialized globals is a significant size optimization. Variables in `.bss`
do not occupy space in the executable file -- the section header just records how many bytes to
allocate. A program with large uninitialized arrays (e.g., `let is [65536]u8 as buf;` at global
scope) benefits from this: the array takes zero bytes in the ELF file but is allocated and
zeroed at load time.

## Reachability Filtering

Both `.data` and `.bss` sections only include globals where `is_reachable == 1`. This flag is
set during the IR phase's `mark_reachable` pass, which traverses all `IR_ADDR_GLOBAL`
instructions in reachable functions and marks the referenced globals.

When compiling with `-c` (library mode, no main function), all globals are marked reachable
unconditionally.

## Symbol Visibility

All globals are emitted with `.globl` directives, making them visible to the linker for
cross-object-file references. This is necessary for multi-object linking where one `.cst` file
declares a global and another references it via `extern`.

## Mutable vs Immutable Globals

At the Caustic language level, globals can be declared `with mut` (mutable) or `with imut`
(immutable). However, the codegen does not distinguish between them -- both end up in `.data`
(if initialized) or `.bss` (if not). The `imut` modifier is enforced by the semantic analysis
phase, which rejects assignments to immutable globals. The codegen has no knowledge of
mutability.

## Example: Complete Data Sections

For this Caustic source:

```cst
let is i64 as count with mut = 0;
let is i32 as max_size with imut = 1024;
let is [256]u8 as buffer with mut;
```

The codegen produces:

```asm
.data
.globl count
count:
  .quad 0
.globl max_size
max_size:
  .long 1024

.bss
.globl buffer
buffer:
  .zero 256
```

# Compiler Overview

Caustic is a self-hosted native x86_64 Linux compiler. The entire toolchain -- compiler, assembler, and linker -- is written in Caustic itself, with zero dependencies on libc, LLVM, or any external runtime. Programs execute via direct Linux syscalls.

## Toolchain Components

The full toolchain consists of three separate binaries:

| Tool | Binary | Input | Output | Description |
|------|--------|-------|--------|-------------|
| Compiler | `caustic` | `.cst` source | `.s` assembly | Compiles Caustic source to x86_64 assembly |
| Assembler | `caustic-as` | `.s` assembly | `.o` ELF object | Assembles x86_64 into relocatable ELF objects |
| Linker | `caustic-ld` | `.o` object(s) | ELF executable | Links objects into a static or dynamic executable |

A typical build invocation:

```cst
// program.cst -> program.cst.s -> program.cst.s.o -> program
```

```bash
./caustic program.cst
./caustic-as program.cst.s
./caustic-ld program.cst.s.o -o program
./program
```

## Compiler Phases

The compiler (`caustic`) transforms `.cst` source through six sequential phases:

| # | Phase | Directory | Files | Input | Output |
|---|-------|-----------|-------|-------|--------|
| 1 | **Lexer** | `src/lexer/` | `lexer.cst`, `tokens.cst`, `util.cst` | Raw source bytes (`*u8`) | `TokenList` (flat array of `Token`) |
| 2 | **Parser** | `src/parser/` | `top.cst`, `stmt.cst`, `expr.cst`, `type.cst`, `ast.cst`, `dump.cst`, `util.cst` | `TokenList` | AST (linked list of `Node`) |
| 3 | **Semantic** | `src/semantic/` | `walk.cst`, `types.cst`, `scope.cst`, `modules.cst`, `generics.cst`, `defs.cst` | Untyped AST | Typed AST (nodes annotated with `*Type`) |
| 4 | **IR** | `src/ir/` | `gen.cst`, `defs.cst` | Typed AST | `IRProgram` (virtual-register IR) |
| 5 | **Codegen** | `src/codegen/` | `alloc.cst`, `emit.cst` | `IRProgram` | x86_64 assembly text |
| 6 | **Output** | `src/main.cst` | `main.cst`, `io.cst` | Assembly text | `.s` file written to disk |

### Phase 1: Lexer (`src/lexer/`)

| File | Responsibility |
|------|----------------|
| `tokens.cst` | Defines the `Token` struct (32 bytes: kind, ptr, len, line, col, int_val), `TokenList` container, and all 77 token kind constants (TK_EOF through TK_XOR_EQ). Also contains `keyword_lookup()` which dispatches by string length. |
| `lexer.cst` | Character-by-character scanner. Reads raw `*u8` source, produces a `TokenList`. Handles string/char literals with escape sequences, integer and float literals, single and multi-character operators, and comments. |
| `util.cst` | Character classification helpers (is_digit, is_alpha, is_alnum, etc.) and character constants. |

### Phase 2: Parser (`src/parser/`)

| File | Responsibility |
|------|----------------|
| `ast.cst` | Defines the `Node` struct (50 node kinds: NK_NUM through NK_FN_PTR), the `Type` struct (18 type kinds: TY_VOID through TY_ENUM), singleton type pointers (type_i32, type_i64, etc.), and constructors (`new_node`, `new_type`). |
| `top.cst` | Top-level parsing entry point. Parses `fn`, `struct`, `enum`, `use`, `impl`, `extern fn`, and global `let` declarations. Entry function: `parse_file()`. |
| `stmt.cst` | Statement parsing: `let`, `return`, `if`/`else`, `while`, `for`, `do`/`while`, `match`, `break`, `continue`, `defer`, `asm`, expression statements. |
| `expr.cst` | Expression parsing with precedence climbing for binary operators. Handles unary operators, function calls, member access, indexing, `cast`, `sizeof`, `syscall`, `fn_ptr`, and literals. |
| `type.cst` | Type annotation parsing: primitives, pointers (`*T`), arrays (`[N]T`), struct/enum references, and generic type arguments (`Type gen T, U`). |
| `dump.cst` | Debug AST printer, activated with `-debugparser`. Walks the AST and prints a human-readable tree. |
| `util.cst` | Parser utilities: token consumption, peek, expect, and error reporting helpers. |

### Phase 3: Semantic Analysis (`src/semantic/`)

| File | Responsibility |
|------|----------------|
| `walk.cst` | Main semantic walker. The `walk()` function visits every node in a large switch on `node.kind`, resolving names, checking types, and annotating each node's `.ty` field. Also handles module loading (calls lexer + parser for imported files). Entry function: `analyze()`. |
| `types.cst` | Type checking and coercion logic. Validates assignments, binary operations, return types. Resolves struct field types and enum variant types. Contains `lookup_struct()` and `lookup_enum()`. |
| `scope.cst` | Scope stack management. Each scope holds a linked list of `Variable` entries. Functions push/pop scopes. Name resolution walks the scope chain upward. |
| `modules.cst` | Module resolution and caching. Tracks loaded modules to avoid re-parsing. Handles file I/O for reading imported `.cst` files. |
| `generics.cst` | Generic instantiation via monomorphization. When a generic function or struct is used with concrete type arguments, clones the AST template, substitutes types, mangles names (e.g., `max_i32`, `Pair_i32_i64`), and appends the instantiation for semantic + IR processing. |
| `defs.cst` | Data structure definitions for the semantic phase: `Variable`, `Scope`, `Function`, `Module`, `StructDef`, `EnumDef`, `GenericTemplate`, `GenericInstance`. All use `*u8` casts for cross-module struct references. |

### Phase 4: IR Generation (`src/ir/`)

| File | Responsibility |
|------|----------------|
| `defs.cst` | IR data structures: `IRProgram`, `IRFunction`, `IRInst`, `IRGlobal`, `StringEntry`, `Operand`. Defines 46 IR opcodes (IR_IMM through IR_FNEG) and operand types (OP_NONE, OP_VREG, OP_IMM, OP_LABEL). |
| `gen.cst` | IR generation. Walks the typed AST and emits IR instructions into `IRFunction` chains. Each expression evaluates to a virtual register number. Handles control flow (labels + conditional jumps), function calls (System V ABI argument setup), struct operations, defer expansion, and float operations. Entry function: `gen_ir()`. |

### Phase 5: Code Generation (`src/codegen/`)

| File | Responsibility |
|------|----------------|
| `alloc.cst` | Linear scan register allocator. Builds `LiveInterval` for each virtual register, sorts by start position, then assigns physical registers (rbx, r12, r13 as callee-saved) or spills to stack. Uses `AllocCtx` to track allocation state per function. |
| `emit.cst` | x86_64 assembly emitter. Iterates each `IRInst` and emits corresponding Intel-syntax assembly. Manages a buffered output writer (4MB buffer). Generates `.text`, `.data`, and `.rodata` sections. Handles the System V AMD64 calling convention (rdi, rsi, rdx, rcx, r8, r9 for integer args; xmm0-xmm7 for floats). Entry function: `codegen()`. |

### Entry Point (`src/main.cst`)

Orchestrates the full pipeline:

1. Parses CLI arguments (source filename, `-c` flag for library compilation)
2. Initializes a 64MB heap via `mem.gheapinit()`
3. Reads the source file
4. Calls `lex.tokenize()` to produce tokens
5. Calls `ast.types_init()` to create singleton type objects
6. Calls `top.parse_file()` to produce the AST
7. Calls `sem.analyze()` for semantic analysis
8. Calls `irg.gen_ir()` to produce IR
9. Opens the output `.s` file
10. Calls `cg.codegen()` to emit assembly
11. Writes and closes the output file

## Assembler (`caustic-as`)

Located in `caustic-assembler/`. Transforms x86_64 assembly (`.s`) into ELF relocatable object files (`.o`).

| File | Responsibility |
|------|----------------|
| `lexer.cst` | Assembly tokenizer. Recognizes instructions, registers, labels, directives, immediates, and memory operands. |
| `encoder.cst` | x86_64 instruction encoder. Handles REX prefixes, ModR/M bytes, SIB bytes, immediates, and displacement encoding. Two-pass design: pass 1 collects symbol addresses, pass 2 encodes final machine code. |
| `elf.cst` | ELF object file writer. Produces `.text`, `.data`, `.rodata`, `.bss`, `.symtab`, `.strtab`, and `.rela.text` sections with proper ELF64 headers. |
| `main.cst` | CLI entry point. Reads input `.s` file, runs the assembly pipeline, writes the `.o` output. |

## Linker (`caustic-ld`)

Located in `caustic-linker/`. Combines one or more `.o` files into a final ELF executable.

| File | Responsibility |
|------|----------------|
| `elf_reader.cst` | ELF object file reader. Parses ELF64 headers, section headers, symbol tables, and relocation entries from `.o` inputs. |
| `linker.cst` | Core linking logic. Merges `.text`, `.data`, `.rodata`, and `.bss` sections from multiple objects. Resolves global symbols across objects. Applies relocations (R_X86_64_PC32, R_X86_64_32S, R_X86_64_PLT32, etc.). |
| `elf_writer.cst` | ELF executable writer. Produces static executables (2 PT_LOAD segments: R+X for code, R+W for data) or dynamic executables (4 program headers with PT_INTERP, PT_DYNAMIC, PLT/GOT). Generates the `_start` entry stub. |
| `main.cst` | CLI entry point. Parses arguments (`-o output`, `-lc` for dynamic linking), reads input objects, runs linking, writes the executable. |

## Standard Library (`std/`)

| File | Description |
|------|-------------|
| `linux.cst` | Raw Linux syscall wrappers: `read`, `write`, `open`, `close`, `mmap`, `munmap`, `exit`, `lseek`, and constants (O_RDONLY, STDOUT, STDERR, SEEK_END, etc.). |
| `mem.cst` | Heap allocator. Uses `mmap` for initial allocation. Free-list with coalescing. Functions: `gheapinit()`, `galloc()`, `gfree()`, `memcpy()`, `memcmp()`, `memset()`. |
| `string.cst` | Dynamic string type (`String` struct with ptr/len/cap). Operations: create, append, append_char, substring, compare, to_cstr. |
| `io.cst` | Buffered I/O. Functions: `write_str()`, `write_int()`, `write_n()`, `printf()`-style formatting, line reading. |
| `types.cst` | Type conversion utilities. |
| `compatcffi.cst` | C FFI compatibility helpers for extern function interop. |

## Data Flow Summary

```
Source (.cst)
    |
    v
[Lexer] -- TokenList (flat array of Token structs)
    |
    v
[Parser] -- Node* (linked list, each node has .kind, .lhs, .rhs, .body, etc.)
    |
    v
[Semantic] -- Node* (same tree, now with .ty set on every node, names resolved)
    |
    v
[IR Gen] -- IRProgram { functions: IRFunction*, globals: IRGlobal*, strings: StringEntry* }
    |           Each IRFunction has a chain of IRInst with virtual register operands
    v
[Codegen] -- x86_64 assembly text (Intel syntax, buffered write)
    |
    v
[caustic-as] -- ELF .o (relocatable object with .text, .data, .rodata, .symtab, .rela.text)
    |
    v
[caustic-ld] -- ELF executable (static or dynamic, directly runnable)
```

# Compiler Overview

Caustic is a self-hosted native x86_64 Linux compiler. The entire toolchain -- compiler, assembler, and linker -- is written in Caustic itself, with zero dependencies on libc, LLVM, or any external runtime. Programs execute via direct Linux syscalls.

## Toolchain Components

The full toolchain consists of four binaries:

| Tool | Binary | Input | Output | Description |
|------|--------|-------|--------|-------------|
| Compiler | `caustic` | `.cst` source | `.s` assembly | Compiles Caustic source to x86_64 assembly (with `-o`: full pipeline to executable) |
| Assembler | `caustic-as` | `.s` assembly | `.o` ELF object | Assembles x86_64 into relocatable ELF objects |
| Linker | `caustic-ld` | `.o` object(s) | ELF executable | Links objects into a static or dynamic executable |
| Build System | `caustic-mk` | `Causticfile` | executables | Orchestrates compilation via declarative project files |

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
| `tokens.cst` | Defines the `Token` struct (32 bytes: kind, ptr, len, line, col, int_val), `TokenList` container, and all 80 token kind constants (TK_EOF through TK_CALL). Also contains `keyword_lookup()` which dispatches by string length. |
| `lexer.cst` | Character-by-character scanner. Reads raw `*u8` source, produces a `TokenList`. Handles string/char literals with escape sequences, integer and float literals, single and multi-character operators, and comments. |
| `util.cst` | Character classification helpers (is_digit, is_alpha, is_alnum, etc.) and character constants. |

### Phase 2: Parser (`src/parser/`)

| File | Responsibility |
|------|----------------|
| `ast.cst` | Defines the `Node` struct (53 node kinds: NK_NUM through NK_CALL_INDIRECT), the `Type` struct (19 type kinds: TY_VOID through TY_FN), singleton type pointers (type_i32, type_i64, etc.), and constructors (`new_node`, `new_type`). |
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
| `defs.cst` | IR data structures: `IRProgram`, `IRFunction`, `IRInst`, `IRGlobal`, `StringEntry`, `Operand`. Defines 48 IR opcodes (IR_IMM through IR_CALL_INDIRECT) and operand types (OP_NONE, OP_VREG, OP_IMM, OP_LABEL). |
| `gen.cst` | IR generation context, emit helpers (emit_imm, emit_binary, emit_call, etc.), loop/defer stacks, string registry. |
| `gen_expr.cst` | Expression IR generation: address computation, function calls (System V ABI), load/store, struct operations, float operations. |
| `gen_stmt.cst` | Statement IR generation: control flow, defer expansion, match/case. |
| `gen_program.cst` | Program-level IR generation: function iteration, global variables, reachability. Entry: `gen_ir()`. |
| `opt.cst` | Optimization pipeline orchestration (-O1): addr_fusion, mem2reg, const prop, store-load fwd, fold imm, LICM, strength reduction, DCE, inlining. |
| `ssa.cst` | SSA construction: mem2reg pass with phi nodes, dominance frontiers, vreg renaming. |
| `cfg.cst` | Control flow graph: basic blocks, successors, predecessors. |
| `dom.cst` | Dominator tree computation and dominance frontiers. |
| `debug.cst` | IR debug printer (activated with `-debugir`). |

### Phase 5: Code Generation (`src/codegen/`)

| File | Responsibility |
|------|----------------|
| `alloc_core.cst` | Register allocator core. At -O0: linear scan with 10 allocatable registers, merge sort, MOV coalescing hints, active set tracking. At -O1: graph coloring (Iterative Register Coalescing, George & Appel 1996). Uses `AllocCtx` to track allocation state per function. |
| `alloc_gc.cst` | Graph coloring register allocator for -O1. Bit matrix interference graph, George coalescing criterion, optimistic Briggs coloring, loop-depth-weighted spill costs. |
| `emit.cst` | x86_64 instruction selection. Load/store with register cache (16-entry, targeted invalidation). IMM+STORE fusion, stack increment fusion, addressing mode matching ([base + index*scale]). |
| `emit_inst.cst` | x86_64 instruction emission for each IR opcode. |
| `emit_driver.cst` | Codegen orchestration: build intervals, allocate registers, compute stack layout, emit prologue/epilogue, iterate instructions. Entry function: `codegen()`. |
| `emit_output.cst` | Buffered assembly output writer (4MB buffer). Register name tables (64/32/16/8-bit). |
| `peephole.cst` | Post-emission peephole optimizer: MOV+movsxd fusion, MOV chain bypass, dead MOV elimination. |

### Entry Point (`src/main.cst`)

Orchestrates the full pipeline:

1. Parses CLI arguments (16 flags: `-c`, `-o`, `-O0`/`-O1`, `--profile`, `--cache`, `--max-ram`, `--path`, `-l<lib>`, `--emit-*`, `-debug*`)
2. Estimates heap size from source + imports, initializes heap via `mem.gheapinit()`
3. For each input file: lex → parse → semantic → IR → optimize (-O1) → codegen → assemble (in-process via caustic-as)
4. If `-o` specified: link all objects into final executable (in-process via caustic-ld)
5. Supports caching (token/AST/IR) via `--cache` for fast rebuilds

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
| `linux.cst` | Raw Linux syscall wrappers (40+): read, write, open, close, mmap, exit, socket, bind, listen, etc. |
| `mem.cst` | Memory management hub. Delegates to submodules: freelist (O(n), coalescing), bins (O(1), slab-based), arena (O(1), bump), pool (O(1), fixed-size), lifo (O(1), stack). Global API: `gheapinit()`, `galloc()`, `gfree()`, `memcpy()`, `memset()`, `memcmp()`. |
| `string.cst` | Dynamic string type (`String` with ptr/len/cap). Operations: concat, find, substring, eq, starts_with, ends_with, contains, char_at, parse_int, trim, split, replace, repeat. |
| `io.cst` | Buffered Reader/Writer, `printf()` with VaList, file ops (read_file, write_file, copy_file), directory ops (list_dir, is_dir). |
| `slice.cst` | Generic dynamic array (`Slice gen T`). Methods: push, get, set, pop, free, clear, reverse, insert, remove_at, swap, clone, reserve, truncate. |
| `types.cst` | `Option gen T { Some; None; }` and `Result gen T, E { Ok; Err; }`. |
| `map.cst` | Hash maps: MapI64 (i64->i64, splitmix64) and MapStr (*u8->i64, FNV-1a). Open addressing, linear probing, iterator API. |
| `math.cst` | Integer math: abs, min, max, clamp, pow, gcd, lcm, sign, div_ceil, is_power_of_two, align_up/down. |
| `sort.cst` | Sorting: quicksort, heapsort, mergesort, insertion, bubble. All with fn_ptr comparators, i32/i64 variants, asc/desc. |
| `random.cst` | xoshiro256** PRNG. Rng struct, rejection sampling range, shuffle, fill. Global convenience functions. |
| `net.cst` | TCP/UDP networking: socket creation, bind/listen/accept/connect, send/recv, poll, socket options. |
| `time.cst` | Monotonic/wall clock, sleep (s/ms/us/ns), elapsed measurement. |
| `env.cst` | Command-line arguments (argc/argv) and environment variables (getenv). |
| `arena.cst` | Bump allocator: O(1) alloc, bulk free via reset/destroy. Save/restore checkpoints. |
| `compatcffi.cst` | C FFI compatibility helpers for struct passing and extern function interop. |

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

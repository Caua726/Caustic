# Compilation Pipeline

This document describes the complete path from Caustic source code to a running
executable. The pipeline consists of six compiler phases followed by the
toolchain's own assembler and linker stages.

## Full Pipeline Diagram

```
  source.cst
      |
      | (raw bytes: *u8, file_size: i64)
      v
  +--------+
  | Lexer  |  src/lexer/
  +--------+
      |
      | TokenList { data: *u8, count: i32, cap: i32 }
      |   array of Token { kind, ptr, len, line, col, int_val }
      v
  +---------+
  | Parser  |  src/parser/
  +---------+
      |
      | *Node  (linked list via .next)
      |   each Node has .kind, .lhs, .rhs, .body, .params, .args, .ty, ...
      |   Type structs for annotations (.ty is initially null or unresolved)
      v
  +----------+
  | Semantic |  src/semantic/
  +----------+
      |
      | *Node  (same linked list, mutated in place)
      |   .ty is now set on every node (resolved *Type pointers)
      |   .var_offset set on variables (stack position)
      |   .stack_size set on functions (total stack frame)
      |   .asm_name_ptr/.asm_name_len set (assembly-level names)
      |   generic templates instantiated and appended to AST
      v
  +--------+
  | IR Gen |  src/ir/
  +--------+
      |
      | IRProgram {
      |   functions: *IRFunction  (linked list)
      |   main_func: *IRFunction  (pointer to main)
      |   globals:   *IRGlobal    (linked list)
      |   strings:   *StringEntry (linked list)
      | }
      |
      | Each IRFunction {
      |   instructions: *IRInst  (linked list of virtual-register ops)
      |   vreg_count, label_count, stack_size, num_args, ...
      | }
      v
  +---------+
  | Codegen |  src/codegen/
  +---------+
      |
      | target assembly text
      |   x86_64: Intel syntax
      |   AArch64: GNU-like scalar syntax
      |   .text / .data / .rodata sections
      |   function labels, instructions, directives
      v
  source.cst.s
      |
      v
  +------------+
  | caustic-as |  caustic-assembler/
  +------------+
      |
      | ELF64 relocatable object
      |   .text (machine code)
      |   .data (initialized globals)
      |   .rodata (string literals, float constants)
      |   .bss (zero-initialized globals)
      |   .symtab + .strtab (symbol table)
      |   .rela.text (relocations)
      v
  source.cst.s.o
      |
      v
  +------------+
  | caustic-ld |  caustic-linker/
  +------------+
      |
      | ELF64 executable
      |   PT_LOAD segments (R+X code, R+W data)
      |   _start entry stub -> main()
      |   optionally: PT_INTERP, PT_DYNAMIC, PLT/GOT for -lc
      v
  executable (directly runnable)
```

## Phase Interactions

### Source to Tokens

The compiler entry point (`src/main.cst`) reads the source file into memory using raw `open`/`read` syscalls, producing a `*u8` buffer and a byte count. This buffer is passed to `lex.tokenize()`, which returns a `TokenList` -- a growable flat array of `Token` structs.

```cst
let is tok.TokenList as tokens = lex.tokenize(src, file_size, filename);
```

The `TokenList` is the only data that flows from lexer to parser. Source bytes are not copied; tokens hold pointers (`ptr`) back into the original source buffer.

### Tokens to AST

The parser consumes the `TokenList` by index, advancing through tokens. It produces a linked list of `Node` structs connected via `.next` pointers.

```cst
let is *ast.Node as root = top.parse_file(&tokens, src, file_size, filename);
```

The parser also creates `Type` structs for type annotations, but does not resolve them -- struct fields, enum variants, and generic parameters are left unresolved at this stage.

### AST to Typed AST

Semantic analysis walks the AST in place, mutating nodes. It does not produce a new tree. After `sem.analyze()` returns, every node has its `.ty` field set to a valid `*Type`, variable offsets are computed, and all generic instantiations have been appended to the AST.

```cst
sem.analyze(cast(*u8, root), filename, fname_len);
```

The semantic phase may invoke the lexer and parser recursively when processing `use` (module import) declarations, loading and analyzing imported files on demand.

### Typed AST to IR

IR generation walks the typed AST and produces an `IRProgram` containing linked lists of `IRFunction`, `IRGlobal`, and `StringEntry` objects. Each function's body becomes a chain of `IRInst` nodes operating on virtual registers (numbered 0 through N).

```cst
let is *ird.IRProgram as prog = irg.gen_ir(cast(*u8, root), no_main);
```

The `no_main` flag (set by `-c`) allows compilation of library modules that have no `main` function.

### IR to Assembly

Code generation takes the `IRProgram` and an output file descriptor, then
dispatches through `src/codegen/backend.cst`. The x86_64 backend uses register
allocation and Intel-syntax emission. The initial AArch64 backend uses fixed
8-byte stack homes for scalar vregs and emits GNU-like AArch64 assembly.

```cst
cg.codegen(cast(*u8, prog), cast(i32, out_fd));
```

Output is buffered (4MB buffer) and flushed via `write` syscalls.

### Assembly to Object

The assembler (`caustic-as`) is a separate binary. It reads the `.s` file, tokenizes assembly syntax, runs a two-pass algorithm (symbol collection, then instruction encoding), and writes an ELF64 `.o` file.

```bash
./caustic-as source.cst.s    # default x86_64; produces source.cst.s.o
./caustic-as --target=linux-aarch64 source.cst.s
```

### Object to Executable

The linker (`caustic-ld`) reads one or more `.o` files, merges their sections, resolves cross-object symbol references, applies relocations, and writes the final ELF executable.

```bash
./caustic-ld source.cst.s.o -o program    # static executable
./caustic-ld main.o lib.o -o program       # multi-object
./caustic-ld main.o -lc -o program         # dynamic linking with libc
./caustic-ld --target=linux-aarch64 main.o lib.o -o program-aarch64
```

Dynamic/shared linking remains specific to the x86_64 Linux path; AArch64
currently supports static single- and multi-object executables.

## Key Data Structures Between Phases

| Boundary | Data Structure | Defined In |
|----------|---------------|------------|
| Source -> Lexer | `*u8` raw bytes + `i64` size | N/A |
| Lexer -> Parser | `TokenList` containing `Token[count]` | `src/lexer/tokens.cst` |
| Parser -> Semantic | `*Node` linked list + `*Type` annotations | `src/parser/ast.cst` |
| Semantic -> IR | `*Node` linked list (mutated in place) | `src/parser/ast.cst` |
| IR -> Codegen | `*IRProgram` with `IRFunction` / `IRInst` chains | `src/ir/defs.cst` |
| Codegen -> Disk | Assembly text written to file descriptor | N/A |
| Disk -> caustic-as | `.s` file on disk | N/A |
| caustic-as -> Disk | ELF `.o` file on disk | N/A |
| Disk -> caustic-ld | `.o` file(s) on disk | N/A |
| caustic-ld -> Disk | ELF executable on disk | N/A |

## Compiler Flags

| Flag | Effect |
|------|--------|
| (none) | Compile source, requires `main` function |
| `-c` | Compile only (library mode), no `main` required, all functions marked reachable |
| `-debuglexer` | Print tokenization output and exit |
| `-debugparser` | Print AST and exit |
| `-debugir` | Print IR and exit |

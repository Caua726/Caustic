# Caustic Documentation

Caustic is a self-hosted native x86_64 Linux compiler. The entire toolchain — compiler, assembler, linker, build system — is written in Caustic itself. No LLVM, no libc, no runtime dependencies.

## Guide

Learn the language from scratch. Each page covers a topic with examples.

- [Getting Started](guide/getting-started.md) — install, hello world, pipeline
- [Variables](guide/variables.md) — declaration, mutability, globals, shadowing
- [Types](guide/types.md) — integers, unsigned, floats, bool, char, void, string, type aliases, hex/binary/octal literals
- [Functions](guide/functions.md) — declaration, parameters, return values, recursion, variadic
- [Control Flow](guide/control-flow.md) — if/else, while, for, do-while, break/continue, match, destructuring
- [Operators](guide/operators.md) — arithmetic, comparison, logical, bitwise, compound assignment, precedence
- [Structs](guide/structs.md) — declaration, field access, packed layout, pointers, return
- [Enums](guide/enums.md) — simple enums, tagged unions, construction, match/case/else, layout
- [Generics](guide/generics.md) — syntax, functions, structs, enums, monomorphization
- [Memory](guide/memory.md) — pointers, address-of, dereferencing, arrays, heap, stack vs heap
- [Modules](guide/modules.md) — use, aliases, only imports, submodules, extern fn, multi-file
- [Impl Blocks](guide/impl.md) — methods, associated functions, desugaring, generic impl
- [Advanced](guide/advanced.md) — cast, sizeof, asm, syscalls, defer, function pointers, call(), FFI, string escapes
- [Compiler Flags](guide/compiler-flags.md) — -c, -o, -O0/-O1, --profile, --cache, --max-ram, all 16 flags
- [Build System](guide/build-system.md) — caustic-mk, Causticfile, targets, scripts, dependencies

## Reference

Deep dive into the compiler internals.

- [Overview](reference/overview.md) — compiler map, files, responsibilities

### Compilation Pipeline

- [Full Pipeline](reference/pipeline/compilation-pipeline.md) — source → tokens → AST → IR → asm → ELF
- [Source to Tokens](reference/pipeline/source-to-tokens.md)
- [Tokens to AST](reference/pipeline/tokens-to-ast.md)
- [AST to Typed AST](reference/pipeline/ast-to-typed-ast.md)
- [Typed AST to IR](reference/pipeline/typed-ast-to-ir.md)
- [IR to Assembly](reference/pipeline/ir-to-assembly.md)
- [Assembly to Object](reference/pipeline/assembly-to-object.md)
- [Object to Executable](reference/pipeline/object-to-executable.md)

### Lexer

- [Overview](reference/lexer/overview.md) — input/output
- [Token Types](reference/lexer/token-types.md) — all 80 token types
- [Keywords](reference/lexer/keywords.md) — 31 reserved words
- [Literals](reference/lexer/literals.md) — integer, float, string, char
- [Operators & Punctuation](reference/lexer/operators-and-punctuation.md)

### Parser

- [Overview](reference/parser/overview.md) — recursive descent
- [Node Kinds](reference/parser/node-kinds.md) — all 53 node kinds
- [Expressions](reference/parser/expressions.md) — precedence climbing
- [Statements](reference/parser/statements.md)
- [Declarations](reference/parser/declarations.md) — fn, struct, enum, let, use, impl
- [Type Parsing](reference/parser/type-parsing.md)

### Semantic Analysis

- [Overview](reference/semantic/overview.md)
- [Scopes](reference/semantic/scopes.md)
- [Type Checking](reference/semantic/type-checking.md)
- [Type Inference](reference/semantic/type-inference.md)
- [Symbol Resolution](reference/semantic/symbol-resolution.md)
- [Module Caching](reference/semantic/module-caching.md)
- [Generic Instantiation](reference/semantic/generic-instantiation.md)
- [Struct Resolution](reference/semantic/struct-resolution.md)

### Type System

- [Primitive Types](reference/type-system/primitive-types.md)
- [Pointer Types](reference/type-system/pointer-types.md)
- [Array Types](reference/type-system/array-types.md)
- [Struct Types](reference/type-system/struct-types.md)
- [Enum Types](reference/type-system/enum-types.md)
- [Function Types](reference/type-system/function-types.md) — TY_FN, typed fn_ptr
- [Type Sizes](reference/type-system/type-sizes.md)
- [Type Compatibility](reference/type-system/type-compatibility.md)

### IR

- [Overview](reference/ir/overview.md) — design, vregs, operands
- [Opcodes](reference/ir/opcodes.md) — all 48 operations
- [Operand Types](reference/ir/operand-types.md)
- [Data Structures](reference/ir/data-structures.md)
- [Control Flow IR](reference/ir/control-flow-ir.md)
- [Function Calls IR](reference/ir/function-calls-ir.md)
- [Memory Ops IR](reference/ir/memory-ops-ir.md)
- [Float Ops IR](reference/ir/float-ops-ir.md)

### Codegen

- [Overview](reference/codegen/overview.md)
- [Register Allocation](reference/codegen/register-allocation.md) — linear scan (-O0) + graph coloring (-O1)
- [Available Registers](reference/codegen/available-registers.md) — 10 allocatable registers
- [Optimizations](reference/codegen/optimizations.md) — -O1 pipeline (10 passes)
- [Instruction Emission](reference/codegen/instruction-emission.md)
- [Function Prologue](reference/codegen/function-prologue.md)
- [Function Epilogue](reference/codegen/function-epilogue.md)
- [Calling Convention](reference/codegen/calling-convention.md)
- [SRET](reference/codegen/sret.md)
- [Stack Layout](reference/codegen/stack-layout.md)
- [Float Codegen](reference/codegen/float-codegen.md)
- [Data Sections](reference/codegen/data-sections.md)

### Assembler (caustic-as)

- [Overview](reference/assembler/overview.md)
- [Tokenization](reference/assembler/tokenization.md)
- [Instruction Encoding](reference/assembler/instruction-encoding.md)
- [x86 Instructions](reference/assembler/x86-instructions.md)
- [ELF Object Format](reference/assembler/elf-object-format.md)
- [Relocations](reference/assembler/relocations.md)
- [Symbol Table](reference/assembler/symbol-table.md)

### Linker (caustic-ld)

- [Overview](reference/linker/overview.md)
- [ELF Reading](reference/linker/elf-reading.md)
- [Section Merging](reference/linker/section-merging.md)
- [Symbol Resolution](reference/linker/symbol-resolution.md)
- [Relocation Processing](reference/linker/relocation-processing.md)
- [Static Linking](reference/linker/static-linking.md)
- [Dynamic Linking](reference/linker/dynamic-linking.md)
- [Start Stub](reference/linker/start-stub.md)
- [ELF Executable Format](reference/linker/elf-executable-format.md)
- [Multi-Object](reference/linker/multi-object.md)

### Build System (caustic-mk)

- [Causticfile Format](reference/build-system/causticfile.md) — targets, scripts, dependencies
- [Commands](reference/build-system/commands.md) — build, run, test, clean, init

### Standard Library

- [mem.cst](reference/stdlib/mem.md) — memory management (5 allocators)
- [io.cst](reference/stdlib/io.md) — buffered I/O, printf, file ops
- [string.cst](reference/stdlib/string.md) — dynamic strings
- [linux.cst](reference/stdlib/linux.md) — syscall wrappers
- [types.cst](reference/stdlib/types.md) — Option, Result
- [slice.cst](reference/stdlib/slice.md) — generic dynamic array
- [map.cst](reference/stdlib/map.md) — hash maps (MapI64, MapStr)
- [math.cst](reference/stdlib/math.md) — integer math
- [sort.cst](reference/stdlib/sort.md) — sorting algorithms
- [random.cst](reference/stdlib/random.md) — xoshiro256** PRNG
- [net.cst](reference/stdlib/net.md) — TCP/UDP networking
- [time.cst](reference/stdlib/time.md) — clock, sleep, elapsed
- [env.cst](reference/stdlib/env.md) — argc/argv, getenv
- [arena.cst](reference/stdlib/arena.md) — bump allocator
- [compatcffi.cst](reference/stdlib/compatcffi.md) — C interop types

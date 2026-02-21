# Caustic Documentation

Caustic is a self-hosted native x86_64 Linux compiler. The entire toolchain — compiler, assembler, linker — is written in Caustic itself. No LLVM, no libc, no runtime dependencies.

## Guide

Learn the language from scratch.

- [Getting Started](guide/getting-started.md) — install, hello world, pipeline
- [Hello World](guide/hello-world.md) — first program, compile, run
- [Compiler Flags](guide/compiler-flags.md) — `-c`, `-debuglexer`, `-debugparser`, `-debugir`
- [Error Messages](guide/error-messages.md) — common errors and how to fix them

### Variables

- [Declaration](guide/variables/declaration.md) — `let is TYPE as name`
- [Mutability](guide/variables/mutability.md) — `with mut`, `with imut`
- [Globals](guide/variables/globals.md) — global variables, data section
- [Shadowing](guide/variables/shadowing.md) — shadowing rules
- [Initialization](guide/variables/initialization.md) — default values, required initialization

### Types

- [Integers](guide/types/integers.md) — `i8`, `i16`, `i32`, `i64`
- [Unsigned](guide/types/unsigned.md) — `u8`, `u16`, `u32`, `u64`
- [Floats](guide/types/floats.md) — `f32`, `f64`, SSE
- [Bool](guide/types/bool.md) — `bool`, `true`/`false`
- [Char](guide/types/char.md) — `char`, `'A'`
- [Void](guide/types/void.md) — `void`
- [String Type](guide/types/string-type.md) — `string`, `"..."`
- [Type Coercion](guide/types/type-coercion.md) — narrowing, widening
- [Negative Literals](guide/types/negative-literals.md) — workarounds for `imut` globals
- [Type Aliases](guide/types/type-aliases.md) — `type Name = ExistingType`

### Functions

- [Declaration](guide/functions/declaration.md) — `fn name(params) as RetType`
- [Parameters](guide/functions/parameters.md) — by value, by pointer
- [Return Values](guide/functions/return-values.md) — `return`, struct return (SRET)
- [Recursion](guide/functions/recursion.md) — recursive functions
- [Variadic](guide/functions/variadic.md) — variadic functions, `...`

### Operators

- [Arithmetic](guide/operators/arithmetic.md) — `+`, `-`, `*`, `/`, `%`
- [Comparison](guide/operators/comparison.md) — `==`, `!=`, `<`, `<=`, `>`, `>=`
- [Logical](guide/operators/logical.md) — `&&`, `||`, `!`
- [Bitwise](guide/operators/bitwise.md) — `&`, `|`, `^`, `~`, `<<`, `>>`
- [Compound Assignment](guide/operators/compound-assignment.md) — `+=`, `-=`, `*=`, `/=`, etc
- [Short-Circuit](guide/operators/short-circuit.md) — `&&` and `||` evaluation
- [Operator Precedence](guide/operators/operator-precedence.md) — precedence table

### Control Flow

- [If/Else](guide/control-flow/if-else.md) — `if`, `else`, `else if`
- [While](guide/control-flow/while.md) — `while` loop
- [For](guide/control-flow/for.md) — `for` loop
- [Do-While](guide/control-flow/do-while.md) — `do { } while ()`
- [Break/Continue](guide/control-flow/break-continue.md) — loop control
- [Match](guide/control-flow/match.md) — pattern matching
- [Destructuring](guide/control-flow/destructuring.md) — enum data extraction

### Structs

- [Declaration](guide/structs/declaration.md) — `struct Name { fields }`
- [Field Access](guide/structs/field-access.md) — `s.field`, pointer access
- [Packed Layout](guide/structs/packed-layout.md) — no padding, `sizeof`
- [Nested Structs](guide/structs/nested-structs.md) — structs inside structs
- [Struct Pointers](guide/structs/struct-pointers.md) — `*Struct`, heap allocation
- [Struct Return](guide/structs/struct-return.md) — SRET, large structs

### Enums

- [Simple Enums](guide/enums/simple-enums.md) — enums without data
- [Tagged Unions](guide/enums/tagged-unions.md) — variants with data
- [Construction](guide/enums/construction.md) — `Enum.Variant`, `Enum.Variant(data)`
- [Enum Layout](guide/enums/enum-layout.md) — tag + max size, memory layout

### Generics

- [Syntax](guide/generics/syntax.md) — `gen T`, `gen T, U`
- [Functions](guide/generics/functions.md) — `fn name gen T (...)`
- [Structs](guide/generics/structs.md) — `struct Name gen T { ... }`
- [Enums](guide/generics/enums.md) — `enum Name gen T { ... }`
- [Monomorphization](guide/generics/monomorphization.md) — code generation, name mangling
- [Constraints](guide/generics/constraints.md) — current limitations

### Impl Blocks

- [Methods](guide/impl/methods.md) — `fn method(self as *Type)`
- [Associated Functions](guide/impl/associated-functions.md) — `fn new(...)`, no `self`
- [Desugaring](guide/impl/desugaring.md) — how methods become top-level functions
- [Generic Impl](guide/impl/generic-impl.md) — `impl Name gen T { ... }`
- [Method Call Syntax](guide/impl/method-call-syntax.md) — `p.method()` → `Type_method(&p)`

### Memory

- [Pointers](guide/memory/pointers.md) — `*T`, null
- [Address-Of](guide/memory/address-of.md) — `&x`
- [Dereferencing](guide/memory/dereferencing.md) — `*ptr`
- [Pointer Arithmetic](guide/memory/pointer-arithmetic.md) — `ptr + offset`
- [Arrays](guide/memory/arrays.md) — `[N]T`, indexing
- [Array of Structs](guide/memory/array-of-structs.md) — struct arrays
- [Heap Allocation](guide/memory/heap-allocation.md) — `galloc`, `gfree`
- [Stack vs Heap](guide/memory/stack-vs-heap.md) — when to use each

### Modules

- [Use Statement](guide/modules/use-statement.md) — `use "path.cst" as alias`
- [Aliases](guide/modules/aliases.md) — `alias.func()`, `alias.CONST`
- [Relative Paths](guide/modules/relative-paths.md) — path resolution
- [Circular Imports](guide/modules/circular-imports.md) — limitations
- [Multi-File Projects](guide/modules/multi-file-projects.md) — `-c`, linking
- [Extern Fn](guide/modules/extern-fn.md) — cross-object declarations

### Advanced

- [Cast](guide/advanced/cast.md) — `cast(Type, expr)`
- [Sizeof](guide/advanced/sizeof.md) — `sizeof(Type)`
- [Inline Assembly](guide/advanced/inline-asm.md) — `asm("...")`
- [Syscalls](guide/advanced/syscalls.md) — `syscall(nr, ...)`
- [Defer](guide/advanced/defer.md) — syntax, LIFO, scope
- [Defer Patterns](guide/advanced/defer-patterns.md) — alloc/free, open/close
- [Function Pointers](guide/advanced/function-pointers.md) — `fn_ptr(name)`
- [FFI](guide/advanced/ffi.md) — `extern fn`, linking with libc
- [FFI Struct Passing](guide/advanced/ffi-struct-passing.md) — System V ABI
- [C Interop Types](guide/advanced/compatcffi.md) — CStruct, CUnion
- [String Escapes](guide/advanced/string-escapes.md) — `\n`, `\t`, `\\`, `\0`
- [Char Literals](guide/advanced/char-literals.md) — `'A'`, encoding

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
- [Token Types](reference/lexer/token-types.md) — all 77 token types
- [Keywords](reference/lexer/keywords.md) — reserved words
- [Literals](reference/lexer/literals.md) — integer, float, string, char
- [Operators & Punctuation](reference/lexer/operators-and-punctuation.md)

### Parser

- [Overview](reference/parser/overview.md) — recursive descent
- [Node Kinds](reference/parser/node-kinds.md) — all 50 node kinds
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
- [Type Sizes](reference/type-system/type-sizes.md)
- [Type Compatibility](reference/type-system/type-compatibility.md)

### IR

- [Overview](reference/ir/overview.md) — design, vregs, operands
- [Opcodes](reference/ir/opcodes.md) — all 46 operations
- [Operand Types](reference/ir/operand-types.md)
- [Data Structures](reference/ir/data-structures.md)
- [Control Flow IR](reference/ir/control-flow-ir.md)
- [Function Calls IR](reference/ir/function-calls-ir.md)
- [Memory Ops IR](reference/ir/memory-ops-ir.md)
- [Float Ops IR](reference/ir/float-ops-ir.md)

### Codegen

- [Overview](reference/codegen/overview.md)
- [Register Allocation](reference/codegen/register-allocation.md)
- [Available Registers](reference/codegen/available-registers.md)
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

### Standard Library

- [mem.cst](reference/stdlib/mem.md) — heap allocator
- [io.cst](reference/stdlib/io.md) — buffered I/O, printf
- [string.cst](reference/stdlib/string.md) — dynamic strings
- [linux.cst](reference/stdlib/linux.md) — syscall wrappers
- [types.cst](reference/stdlib/types.md) — Option, Result
- [slice.cst](reference/stdlib/slice.md) — generic dynamic array
- [compatcffi.cst](reference/stdlib/compatcffi.md) — C interop types

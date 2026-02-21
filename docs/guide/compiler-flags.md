# Compiler Flags

## Basic Compilation

```bash
./caustic program.cst
```

Compiles `program.cst` and generates `program.cst.s` (x86_64 assembly). Requires a `main` function in the source.

## Library Mode (-c)

```bash
./caustic -c mylib.cst
```

Compile without requiring a `main` function. Use this for library files that will be linked with other objects. All functions are marked as globally visible for cross-object linking.

## Debug Flags

### -debuglexer

```bash
./caustic -debuglexer program.cst
```

Prints every token produced by the lexer. Useful for diagnosing tokenization issues (e.g., operators being split incorrectly, string literal problems).

### -debugparser

```bash
./caustic -debugparser program.cst
```

Prints the AST (Abstract Syntax Tree) after parsing. Shows the structure of the parsed program, including node kinds, types, and nesting.

### -debugir

```bash
./caustic -debugir program.cst
```

Prints the IR (Intermediate Representation) with virtual registers. Shows the lowered operations before register allocation and assembly generation.

## Multi-File Compilation

To compile a program split across multiple files:

```bash
# 1. Compile the library (no main needed)
./caustic -c utils.cst

# 2. Compile the main program
./caustic main.cst

# 3. Assemble both
./caustic-as utils.cst.s
./caustic-as main.cst.s

# 4. Link all objects together
./caustic-ld utils.cst.s.o main.cst.s.o -o program
```

Use `extern fn` declarations in the main file to reference functions defined in other objects:

```cst
// main.cst
extern fn add(a as i32, b as i32) as i32;

fn main() as i32 {
    return add(10, 20);
}
```

```cst
// utils.cst (compiled with -c)
fn add(a as i32, b as i32) as i32 {
    return a + b;
}
```

## Assembler and Linker

The assembler and linker have minimal flags:

```bash
# Assembler: input.s -> input.s.o
./caustic-as input.s

# Linker: one or more .o files -> executable
./caustic-ld input.s.o -o output
./caustic-ld a.s.o b.s.o c.s.o -o program
```

## Quick Test One-Liner

For rapid iteration, compile, assemble, link, run, and print the exit code in one command:

```bash
./caustic test.cst && ./caustic-as test.cst.s && ./caustic-ld test.cst.s.o -o /tmp/t && /tmp/t; echo "Exit: $?"
```

# Multi-Object Linking

caustic-ld supports linking any number of ELF relocatable object files into a single
executable. This allows splitting a Caustic program into multiple source files with separate
compilation.

## When to Use Multiple Objects

- Splitting large programs into logical modules (a library file + a main file)
- Reusing compiled code across multiple programs without recompilation
- Isolating independently compiled components that communicate through defined interfaces

## Compilation Flags

### -c (library mode)

The Caustic compiler's `-c` flag compiles a source file without requiring a `main` function.
All functions in the file are treated as reachable and included in the output object:

```bash
./caustic -c lib.cst         # compiles lib.cst → lib.cst.s (no main required)
./caustic main.cst           # compiles main.cst → main.cst.s (main required)
```

Without `-c`, the compiler performs reachability analysis from main and may omit unused functions.
With `-c`, all defined functions are emitted.

### .globl Directives

The Caustic codegen emits `.globl` for every function, making all symbols visible for
cross-object linking:

```asm
.globl add
add:
    ; function body
```

This ensures that functions defined in one object can be referenced by relocations in another.
Local (file-scope) symbols do not get `.globl` and are not visible across object boundaries.

## Build Workflow

```bash
# 1. Compile each source file
./caustic -c lib.cst           # generates lib.cst.s
./caustic main.cst             # generates main.cst.s

# 2. Assemble each file
./caustic-as lib.cst.s         # generates lib.cst.s.o
./caustic-as main.cst.s        # generates main.cst.s.o

# 3. Link all objects together
./caustic-ld lib.cst.s.o main.cst.s.o -o program

# 4. Run
./program
```

## extern fn Declarations

To call a function defined in another .o file from Caustic source code, declare it with
`extern fn`. This provides the type signature without a body. The compiler emits an undefined
symbol reference; the linker resolves it from the other object.

### In lib.cst

```cst
fn add(a as i64, b as i64) as i64 {
    return a + b;
}

fn multiply(a as i64, b as i64) as i64 {
    return a * b;
}
```

### In main.cst

```cst
extern fn add(a as i64, b as i64) as i64;
extern fn multiply(a as i64, b as i64) as i64;

fn main() as i64 {
    let is i64 as result = add(3, 4);
    return result;
}
```

### Link

```bash
./caustic-ld lib.cst.s.o main.cst.s.o -o program
./program
echo $?    # 7
```

## Symbol Resolution Across Objects

When the linker processes `main.cst.s.o`, it finds an undefined reference to `add`. It looks
up `add` in the global symbol table built from all input objects. `lib.cst.s.o` defines `add`,
so the reference resolves. The relocation in main.o's .text is patched with the final virtual
address of the `add` function.

The order of objects on the command line does not affect symbol resolution (all objects are
read before resolution begins). However, convention puts library objects before main objects
to match dependency order.

## Data Sharing Between Objects

Global variables can also be shared between objects using `extern`:

```cst
// In globals.cst
let is i64 as global with mut = 0;

// In main.cst (extern variable — not yet supported as syntax, use accessor functions)
extern fn get_global() as i64;
extern fn set_global(v as i64) as void;
```

Caustic does not currently have `extern let` syntax. Cross-module data sharing is done through
accessor functions.

## Struct Types Across Objects

Caustic does not support importing struct type definitions from other .cst files for use in
`extern fn` signatures. Workarounds:

1. Pass and return primitives (i64, *u8) instead of structs.
2. Use `*u8` pointers and cast at call sites.
3. Duplicate the struct definition in both files (they must have identical field layout).

## Multiple Library Objects

Any number of objects can be linked together:

```bash
./caustic-ld math.o strings.o io.o main.o -o program
```

The linker processes them all in order, merges their sections, resolves all cross-references,
and applies relocations. There is no concept of "archive libraries" (.a files) — every object
must be listed explicitly.

## Circular Dependencies

caustic-ld supports circular symbol references between objects. If object A calls a function
in object B, and object B calls a function in object A, both directions are resolved because
all symbols are collected before any relocations are applied.

## Makefile Example

```makefile
OBJS = lib.cst.s.o main.cst.s.o

program: $(OBJS)
	./caustic-ld $(OBJS) -o program

lib.cst.s.o: lib.cst.s
	./caustic-as lib.cst.s

lib.cst.s: lib.cst
	./caustic -c lib.cst

main.cst.s.o: main.cst.s
	./caustic-as main.cst.s

main.cst.s: main.cst
	./caustic main.cst

clean:
	rm -f *.s *.o program
```

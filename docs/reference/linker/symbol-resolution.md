# Symbol Resolution

Symbol resolution is the process of matching every undefined symbol reference in the input
objects to a definition. caustic-ld performs this after merging sections and before applying
relocations. Implementation is in `caustic-linker/linker.cst`.

## Symbol Binding Types

ELF symbols carry a binding attribute that controls resolution behavior:

| Binding   | st_info value | Meaning                                              |
|-----------|---------------|------------------------------------------------------|
| STB_LOCAL | 0             | Visible only within the defining object file         |
| STB_GLOBAL| 1             | Visible to all object files, participates in linking |
| STB_WEAK  | 2             | Like global but lower precedence (not used by caustic)|

caustic-ld only participates in resolution for STB_GLOBAL symbols. LOCAL symbols are tracked
for relocation but are never used to satisfy references from other objects.

## Global Symbol Table

After reading all input objects, caustic-ld builds a flat global symbol table. For each input
object, every STB_GLOBAL symbol with a defined section index (st_shndx != SHN_UNDEF) is
inserted with its final virtual address (adjusted for section merging offsets).

```
global symbol table entry:
  name          — null-terminated string
  address       — final virtual address in the output binary
  defined       — whether a definition was found
  object_index  — which input file defined this symbol
```

## Resolution Process

For each undefined reference (st_shndx == SHN_UNDEF) encountered in any input object, the
linker looks up the symbol name in the global symbol table:

1. If found and defined: resolution succeeds. The relocation that references this symbol will
   use the resolved virtual address.

2. If not found in any object: check dynamic library imports (dynamic mode only). If the symbol
   is in a linked shared library (.so), a PLT entry is generated and the symbol resolves to the
   PLT stub address.

3. If not found anywhere: link error — undefined symbol.

## Duplicate Symbol Error

If two input objects both define the same global symbol, caustic-ld reports an error:

```
error: duplicate symbol 'my_function' defined in lib.o and main.o
```

This mirrors standard linker behavior. There is no support for weak symbols or COMDAT groups.

## Special Symbols

caustic-ld defines several synthetic symbols that are always available:

| Symbol  | Value                               |
|---------|-------------------------------------|
| _start  | Entry point stub address (see start-stub.md) |

The Caustic compiler emits `main` as a regular global function. caustic-ld does not create
`main` itself — the _start stub calls whatever symbol is named `main`.

## Symbol Visibility for Cross-Object Linking

The Caustic compiler emits `.globl` directives for all functions, making them available for
cross-object resolution:

```asm
.globl my_function
my_function:
    ...
```

This means all Caustic-compiled functions are STB_GLOBAL unless explicitly made local. When
using `-c` (library mode), all functions in the file are included regardless of reachability,
because the linker cannot know which ones the caller will use.

## extern fn in Caustic Source

The `extern fn` declaration allows a Caustic source file to reference a function defined in
another object file without providing a body:

```cst
extern fn add(a as i64, b as i64) as i64;

fn main() as i64 {
    return add(1, 2);
}
```

The compiler emits an undefined symbol reference for `add`. The linker resolves it by finding
the definition in the other linked object.

## Dynamic Symbol Resolution

When `-lc` or another `-l` flag is passed, the linker also searches dynamic library exports.
Symbols resolved to shared libraries are not given fixed addresses at link time. Instead:

1. A PLT entry is created for the symbol (a small stub that jumps through the GOT).
2. A GOT slot is reserved for the runtime address.
3. A .rela.plt entry is emitted so the dynamic linker fills the GOT slot at startup.

See `dynamic-linking.md` for the full PLT/GOT mechanism.

## Resolution Order

1. Scan all input objects and add all defined globals to the global table.
2. Check for duplicates during insertion.
3. For each undefined reference, look up in global table.
4. For any still-undefined references, check dynamic libraries (if any -l flags given).
5. Any remaining undefined references are fatal errors.

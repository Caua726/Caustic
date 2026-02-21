# Multi-File Projects

For larger projects, compile files separately and link the resulting object files together.

## Workflow

**1. Compile each file**

Use `-c` for library files (no `main` function required). The main file is compiled without `-c`.

```sh
./caustic -c lib.cst        # produces lib.cst.s
./caustic main.cst           # produces main.cst.s (requires main)
```

**2. Assemble each file**

```sh
./caustic-as lib.cst.s       # produces lib.cst.s.o
./caustic-as main.cst.s      # produces main.cst.s.o
```

**3. Link together**

```sh
./caustic-ld lib.cst.s.o main.cst.s.o -o program
./program
```

## The `-c` flag

- Skips the requirement for a `main` function.
- Marks all functions as reachable (nothing is dead-code eliminated).
- All functions are emitted with `.globl` visibility so other object files can reference them.

## Cross-object references

Use `extern fn` to declare functions defined in another object file.

```cst
// lib.cst — compiled with -c
fn helper(x as i32) as i32 {
    return x * 2;
}
```

```cst
// main.cst
extern fn helper(x as i32) as i32;

fn main() as i32 {
    return helper(21);
}
```

## Complete example

```sh
# Write the files
# lib.cst:  fn add(a as i64, b as i64) as i64 { return a + b; }
# main.cst: extern fn add(a as i64, b as i64) as i64;
#           fn main() as i32 { return cast(i32, add(30, 12)); }

# Build
./caustic -c lib.cst && ./caustic main.cst
./caustic-as lib.cst.s && ./caustic-as main.cst.s
./caustic-ld lib.cst.s.o main.cst.s.o -o program
./program; echo "Exit: $?"
# Exit: 42
```

## When to use multi-file vs `use`

| Approach | When to use |
|----------|-------------|
| `use "file.cst" as alias;` | Single compilation unit. The imported file is compiled together with the main file. |
| `-c` + `extern fn` + linking | Separate compilation units. Each file is compiled independently. Useful for large projects or mixing with other languages. |

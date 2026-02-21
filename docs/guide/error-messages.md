# Error Messages

## Common Errors

### "Error: Global Heap not init"

You called a standard library function that allocates memory (e.g., `galloc`, `io.printf`, string operations) without initializing the heap first.

Fix: call `mem.gheapinit(size)` at the start of `main`:

```cst
use "std/mem.cst" as mem;

fn main() as i32 {
    mem.gheapinit(1048576);  // 1 MB heap
    // now you can use galloc, io.printf, etc.
    return 0;
}
```

### Missing main function

```
error: no main function found
```

Every program needs a `fn main() as i32` entry point. If you are compiling a library file that does not have `main`, use the `-c` flag:

```bash
./caustic -c mylib.cst
```

### Type mismatch

```
error: type mismatch: expected i32, got i64
```

Caustic enforces strict typing. Common causes:
- Assigning a value of the wrong type to a variable
- Passing arguments of the wrong type to a function
- Returning the wrong type from a function
- Using integer literals where float is expected (use `10.0` instead of `10` for f64)

```cst
// Wrong: literal defaults to i64
let is f64 as x = 10;

// Correct: use float literal
let is f64 as x = 10.0;
```

### Unknown identifier

```
error: unknown identifier 'foo'
```

The variable or function `foo` was not declared in the current scope or any parent scope. Check for:
- Typos in the name
- Missing `use` import for module functions
- Variable declared inside a block that is no longer in scope

### Field not found

```
error: field 'bar' not found on struct 'Foo'
```

You tried to access a struct field that does not exist. Check the struct definition for the correct field name:

```cst
struct Point {
    x as i32;
    y as i32;
}

fn main() as i32 {
    let is Point as p;
    p.x = 10;
    p.z = 20;  // error: field 'z' not found on struct 'Point'
    return 0;
}
```

### Undeclared module function

```
error: unknown function 'mod.func'
```

The function does not exist in the imported module, or the module was not imported. Verify the `use` statement and function name:

```cst
use "std/io.cst" as io;
io.printf("works\n");
```

## Error Position Reporting

All compiler errors include the file name, line number, column number, and the source line with a caret (`^`) pointing to the exact location of the error:

```
/tmp/test.cst:3:5: Erro semantico: type mismatch
    let is i32 as x = "hello";
    ^
```

This applies to lexer, parser, and semantic errors. The position information helps quickly locate the problem in your code.

## Warnings

The compiler emits warnings for certain conditions that do not prevent compilation:

### Unused variables

```
/tmp/test.cst:2:5: Aviso: variavel 'x' nao utilizada
    let is i32 as x = 42;
    ^
```

A variable that is declared but never read produces a warning. To suppress this warning, prefix the variable name with an underscore:

```cst
let is i32 as _unused = compute_side_effect();
```

## Debugging Tips

When you hit an error you do not understand, use the debug flags to inspect what the compiler sees at each stage:

### 1. Check tokenization

```bash
./caustic -debuglexer problem.cst
```

Look for unexpected tokens. This catches issues like:
- String literals not being closed
- Operators being tokenized incorrectly
- Keywords conflicting with identifiers (`gen` and `fn` are reserved)

### 2. Check the AST

```bash
./caustic -debugparser problem.cst
```

Verify the parsed structure matches what you intended. This catches:
- Operator precedence issues
- Incorrect nesting of expressions
- Missing or extra arguments

### 3. Check the IR

```bash
./caustic -debugir problem.cst
```

Inspect the generated intermediate representation. This shows:
- How expressions are lowered to operations
- Virtual register assignments
- Function call sequences

### General tips

- **Start small**: Reduce your program to the smallest version that reproduces the error.
- **Check types**: Most errors come from type mismatches. Caustic is strictly typed with no implicit conversions.
- **Check syscall numbers**: Since there is no libc, you need the correct x86_64 Linux syscall numbers (write=1, read=0, open=2, close=3, mmap=9, exit=60).
- **Exit code range**: `main` returns `i32` but the exit code is truncated to 0-255 by Linux.
- **Float literals**: `f32` and `f64` variables require float literals (`3.14`, `0.0`), not integer literals.

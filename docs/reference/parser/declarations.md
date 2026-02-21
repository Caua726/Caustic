# Declaration Parsing

Declarations are the top-level constructs of a Caustic program. The parser's top-level loop dispatches on the current token to parse each declaration form.

## Function Declaration

Syntax:
```
fn name(params) as RetType { body }
fn name gen T (params) as RetType { body }
fn name gen T, U (params) as RetType { body }
```

Examples:
```cst
fn add(a as i64, b as i64) as i64 {
    return a + b;
}

fn max gen T (a as T, b as T) as T {
    if (a > b) {
        return a;
    }
    return b;
}

fn greet() as void {
    syscall(1, 1, "hello\n", 6);
}
```

Parsing steps:
1. Consume `TK_FN`.
2. Consume `TK_IDENT` as the function name.
3. If next token is `TK_GEN`, parse generic parameters:
   a. Consume `TK_GEN`.
   b. Parse comma-separated list of type parameter identifiers (e.g., `T`, `T, U`).
4. Consume `TK_LPAREN`.
5. Parse parameter list:
   a. Each parameter: `name as Type` (identifier, `TK_AS`, type).
   b. Parameters separated by `TK_COMMA`.
   c. Variadic `...` may appear as the last parameter.
6. Consume `TK_RPAREN`.
7. Consume `TK_AS`.
8. Parse the return type.
9. Parse the function body block `{ ... }`.

Creates `NK_FN` node with:
- `name`: function name
- `params`: linked list of parameter nodes
- `ret_type`: return type
- `body`: linked list of body statements
- `generic_params`: list of generic type parameter names (or null)
- `generic_param_count`: number of generic parameters

### Parameter Syntax

Each parameter uses the form `name as Type`:

```cst
fn example(x as i32, buf as *u8, len as u64) as void { ... }
```

This is distinct from most languages that use `name: Type` or `Type name`. The `as` keyword binds the name to its type.

## Struct Declaration

Syntax:
```
struct Name { fields }
struct Name gen T { fields }
struct Name gen T, U { fields }
```

Examples:
```cst
struct Point {
    x as i32;
    y as i32;
}

struct Pair gen T, U {
    first as T;
    second as U;
}
```

Parsing steps:
1. Consume `TK_STRUCT`.
2. Consume `TK_IDENT` as the struct name.
3. If next token is `TK_GEN`, parse generic parameters.
4. Consume `TK_LBRACE`.
5. Parse fields in a loop:
   a. Each field: `name as Type;` (identifier, `TK_AS`, type, semicolon).
   b. Fields are linked together.
6. Consume `TK_RBRACE`.

The resulting node is added to the top-level AST. Struct types are registered during semantic analysis for use in variable declarations and function signatures.

### Struct Layout

Caustic structs are **packed** with no alignment padding. Fields are laid out sequentially in declaration order. This means:

```cst
struct Mixed {
    a as i32;   // offset 0, 4 bytes
    b as i64;   // offset 4, 8 bytes  (no padding!)
    c as i32;   // offset 12, 4 bytes
}
// Total size: 20 bytes (not 24)
```

This is important when interfacing with external code that expects aligned structs.

## Enum Declaration

Syntax:
```
enum Name { variants }
enum Name gen T { variants }
```

Examples:
```cst
enum Color {
    case Red;
    case Green;
    case Blue;
}

enum Option gen T {
    case Some(T);
    case None;
}
```

Parsing steps:
1. Consume `TK_ENUM`.
2. Consume `TK_IDENT` as the enum name.
3. If next token is `TK_GEN`, parse generic parameters.
4. Consume `TK_LBRACE`.
5. Parse variants in a loop:
   a. Consume `TK_CASE`.
   b. Consume `TK_IDENT` as the variant name.
   c. If followed by `TK_LPAREN`, parse associated data types.
   d. Consume `TK_SEMICOLON`.
6. Consume `TK_RBRACE`.

Enum variants are assigned integer discriminant values starting from 0.

## Global Variable Declaration

Syntax:
```
let is Type as name with mut = expr;
let is Type as name with imut = expr;
```

Examples:
```cst
let is i64 as counter with mut = 0;
let is *u8 as VERSION with imut = "1.0.0";
let is i32 as MAX_SIZE with imut = 1024;
```

Parsing steps:
1. Consume `TK_LET`.
2. Consume `TK_IS`.
3. Parse the type.
4. Consume `TK_AS`.
5. Consume `TK_IDENT` as the variable name.
6. Consume `TK_WITH`.
7. Consume `TK_IDENT` (must be `mut` or `imut`):
   - `mut`: mutable global, stored in `.data` section.
   - `imut`: immutable global, stored in `.rodata` section.
8. Consume `TK_ASSIGN`.
9. Parse the initializer expression (must be a constant expression).
10. Consume `TK_SEMICOLON`.

Global declarations require:
- The `with mut` or `with imut` qualifier (local variables do not use `with`).
- A constant initializer expression. Complex runtime expressions are not allowed.

### Limitations

- No negative immutable globals with literal syntax. `let is i32 as X with imut = 0 - 1;` fails because `0 - 1` is not a constant expression. Use positive sentinel values instead.

## Use (Module Import)

Syntax:
```
use "path/to/module.cst" as alias;
```

Examples:
```cst
use "std/mem.cst" as mem;
use "std/io.cst" as io;
use "utils/helper.cst" as helper;
```

Parsing steps:
1. Consume `TK_USE`.
2. Consume `TK_STRING` as the module path.
3. Consume `TK_AS`.
4. Consume `TK_IDENT` as the module alias.
5. Consume `TK_SEMICOLON`.

Creates `NK_USE` node with:
- `module_path`: the string path to the source file
- `module_alias`: the alias identifier

The module system uses the alias for all references to the imported module's symbols:

```cst
use "std/mem.cst" as mem;
let is *u8 as p = mem.galloc(1024);
mem.gfree(p);
```

Module caching ensures each file is parsed and analyzed only once, even if imported by multiple files.

### Alias Restrictions

The alias cannot be a keyword. In particular, `gen` and `fn` are reserved and cannot be used as module aliases. Use alternatives like `gn` or `fref`.

## Impl Block

Syntax:
```
impl TypeName { methods }
impl TypeName gen T { methods }
```

Examples:
```cst
impl Point {
    fn new(x as i32, y as i32) as Point {
        let is Point as p;
        p.x = x;
        p.y = y;
        return p;
    }

    fn sum(self as *Point) as i32 {
        return self.x + self.y;
    }
}

impl Wrapper gen T {
    fn get(self as *Wrapper gen T) as T {
        return self.value;
    }
}
```

Parsing steps:
1. Consume `TK_IMPL`.
2. Consume `TK_IDENT` as the type name.
3. If next token is `TK_GEN`, parse generic parameters.
4. Consume `TK_LBRACE`.
5. Parse method declarations in a loop:
   a. Each method is a full function declaration (`fn name(params) as RetType { body }`).
   b. The parser renames each method from `name` to `TypeName_name` (name mangling).
   c. For generic impls, generic parameters are copied to each method.
6. Consume `TK_RBRACE`.

### Desugaring

The parser converts impl methods into top-level functions at parse time:

```cst
impl Point {
    fn sum(self as *Point) as i32 { ... }
}
// Becomes:
fn Point_sum(self as *Point) as i32 { ... }
```

This means:
- **Instance methods**: Functions with a `self` parameter (e.g., `self as *Point`). Called via `p.sum()`, which the semantic phase transforms to `Point_sum(&p)`.
- **Associated functions**: Functions without `self`. Called via `Point.new(1, 2)`, which becomes `Point_new(1, 2)`.

The semantic analysis phase resolves `.method()` calls by checking whether the left-hand side of the dot is a type name (associated function) or a variable (instance method).

## Extern Function Declaration

Syntax:
```
extern fn name(params) as RetType;
```

Examples:
```cst
extern fn printf(fmt as *u8, ...) as i32;
extern fn malloc(size as u64) as *u8;
extern fn helper_func(x as i64) as i64;
```

Parsing steps:
1. Consume `TK_EXTERN`.
2. Consume `TK_FN`.
3. Consume `TK_IDENT` as the function name.
4. Consume `TK_LPAREN`.
5. Parse parameter list (same as regular function parameters).
6. Consume `TK_RPAREN`.
7. Consume `TK_AS`.
8. Parse the return type.
9. Consume `TK_SEMICOLON` (no body).

Creates `NK_FN` node with `is_extern` set to true and no body. The function is declared in the symbol table with its signature but no implementation.

### Use Cases

- **Cross-object linking**: When compiling with `-c` (compile-only mode), other object files provide the implementation. The `extern fn` declaration lets the compiler type-check calls without requiring the function body.
- **Dynamic linking**: When linking against shared libraries (e.g., `-lc` for libc), `extern fn` declares the foreign function's signature.

### Variadic Extern Functions

Extern functions may use `...` as the last parameter to indicate variadic arguments, following the C calling convention:

```cst
extern fn printf(fmt as *u8, ...) as i32;
```

## Declaration Order

The top-level parser loop processes declarations in source order. However, Caustic performs multi-pass analysis:

1. **First pass**: Register all struct and enum types (names and shapes).
2. **Second pass**: Resolve struct field types (which may reference other structs).
3. **Third pass**: Register all function signatures.
4. **Fourth pass**: Analyze function bodies.

This means forward references are supported. A function can call another function declared later in the file, and structs can reference other structs declared later.

# Type Parsing

Types appear in variable declarations (`let is Type as name`), function parameters (`param as Type`), function return types (`fn f() as Type`), cast expressions (`cast(Type, expr)`), and sizeof expressions (`sizeof(Type)`).

## Type Kind Table

The `Type` struct uses a `kind` field to identify the type category. There are 18 type kinds:

| Kind | Value | Description | Key Fields |
|------|-------|-------------|------------|
| `TY_VOID` | 0 | Void type (no value) | — |
| `TY_I8` | 1 | Signed 8-bit integer | size=1 |
| `TY_I16` | 2 | Signed 16-bit integer | size=2 |
| `TY_I32` | 3 | Signed 32-bit integer | size=4 |
| `TY_I64` | 4 | Signed 64-bit integer | size=8 |
| `TY_U8` | 5 | Unsigned 8-bit integer | size=1 |
| `TY_U16` | 6 | Unsigned 16-bit integer | size=2 |
| `TY_U32` | 7 | Unsigned 32-bit integer | size=4 |
| `TY_U64` | 8 | Unsigned 64-bit integer | size=8 |
| `TY_F32` | 9 | 32-bit floating point | size=4 |
| `TY_F64` | 10 | 64-bit floating point | size=8 |
| `TY_BOOL` | 11 | Boolean (true/false) | size=1 |
| `TY_CHAR` | 12 | Character (8-bit) | size=1 |
| `TY_STRING` | 13 | String literal type | size=8 (pointer) |
| `TY_PTR` | 14 | Pointer to another type | base, size=8 |
| `TY_ARRAY` | 15 | Fixed-size array | base, array_len |
| `TY_STRUCT` | 16 | Struct type | name, fields, size |
| `TY_ENUM` | 17 | Enum type | name, variants |

## Primitive Types

Primitive types are recognized by matching the identifier against built-in type names:

| Keyword | Type Kind | Size (bytes) | Description |
|---------|-----------|--------------|-------------|
| `i8` | `TY_I8` | 1 | Signed 8-bit integer (-128 to 127) |
| `i16` | `TY_I16` | 2 | Signed 16-bit integer |
| `i32` | `TY_I32` | 4 | Signed 32-bit integer |
| `i64` | `TY_I64` | 8 | Signed 64-bit integer |
| `u8` | `TY_U8` | 1 | Unsigned 8-bit integer (0 to 255) |
| `u16` | `TY_U16` | 2 | Unsigned 16-bit integer |
| `u32` | `TY_U32` | 4 | Unsigned 32-bit integer |
| `u64` | `TY_U64` | 8 | Unsigned 64-bit integer |
| `f32` | `TY_F32` | 4 | IEEE 754 single-precision float |
| `f64` | `TY_F64` | 8 | IEEE 754 double-precision float |
| `bool` | `TY_BOOL` | 1 | Boolean (0 or 1) |
| `char` | `TY_CHAR` | 1 | Character (ASCII byte) |
| `string` | `TY_STRING` | 8 | String literal (pointer to static data) |
| `void` | `TY_VOID` | 0 | No value (return type only) |

The compiler uses **global singleton** `Type` objects for all primitive types (e.g., `type_i32`, `type_i64`). Pointer equality is used throughout the compiler to check type identity, so it is critical that all references to a primitive type point to the same singleton.

### Parsing

When the parser encounters a type position, it checks the current `TK_IDENT` token against the primitive type names. If a match is found, the corresponding singleton is returned directly without allocating a new `Type` object.

## Pointer Types

Syntax: `*T` where `T` is any type.

```cst
*u8          // pointer to u8
*i64         // pointer to i64
*Point       // pointer to struct Point
**u8         // pointer to pointer to u8
*[10]i32     // pointer to array of 10 i32s
```

Parsing:
1. Consume `TK_STAR`.
2. Recursively parse the base type `T`.
3. Create a `Type` with `kind=TY_PTR`, `base=T`, `size=8`.

Pointer types are always 8 bytes on x86_64. Multiple levels of indirection are supported by recursive parsing.

## Array Types

Syntax: `[N]T` where `N` is an integer literal and `T` is the element type.

```cst
[10]i32      // array of 10 i32 values
[256]u8      // byte buffer of 256 bytes
[64]char     // character array
[4]*u8       // array of 4 pointers
```

Parsing:
1. Consume `TK_LBRACKET`.
2. Consume `TK_INTEGER`, store as `array_len`.
3. Consume `TK_RBRACKET`.
4. Parse the element type `T`.
5. Create a `Type` with `kind=TY_ARRAY`, `base=T`, `array_len=N`.

The total size of an array type is `N * sizeof(T)`. Arrays are allocated inline (on the stack or within structs), not as heap pointers.

### Limitations

- Array length must be a compile-time integer literal. Variable-length arrays are not supported.
- No multi-dimensional array syntax. Nested arrays like `[3][4]i32` must be written as `[3][4]i32` (array of arrays), parsed recursively.

## Named Types (Struct and Enum)

When the parser encounters an identifier that does not match a primitive type, it is treated as a named type reference (struct or enum).

```cst
Point        // struct Point
Color        // enum Color
MyStruct     // any user-defined type
```

Parsing:
1. Consume `TK_IDENT`.
2. Create a `Type` with `kind=TY_STRUCT` (or `TY_ENUM`), `name` set to the identifier text.

At parse time, the type is unresolved. The semantic analysis phase resolves the name to an actual struct or enum definition and fills in the `fields`, `size`, and other metadata.

## Generic Types

Syntax: `Name gen T1, T2, ...` where `T1`, `T2` are concrete type arguments.

```cst
Pair gen i32, i64         // Pair<i32, i64>
Option gen *u8            // Option<*u8>
Wrapper gen Point         // Wrapper<Point>
```

Parsing:
1. Parse the base type name as a `TK_IDENT`.
2. If the next token is `TK_GEN`:
   a. Consume `TK_GEN`.
   b. Parse a comma-separated list of type arguments. Each argument is a full type (can be primitive, pointer, array, or another named/generic type).
3. Create a `Type` with the base name and `generic_args` set to the parsed type list.

### Name Mangling

During semantic analysis, generic types are monomorphized. The mangled name concatenates the base name with each type argument, separated by underscores:

| Generic Type | Mangled Name |
|-------------|--------------|
| `Pair gen i32, i64` | `Pair_i32_i64` |
| `Option gen *u8` | `Option_pu8` (pointer prefix) |
| `Wrapper gen i32` | `Wrapper_i32` |

## Module-Qualified Types

Syntax: `alias.TypeName` where `alias` is a module alias from a `use` declaration.

```cst
use "std/string.cst" as str;
let is str.String as s;
```

Parsing:
1. Consume `TK_IDENT` (the module alias).
2. Consume `TK_DOT`.
3. Consume `TK_IDENT` (the type name within the module).
4. Create a `Type` with `name` set to the qualified name and `module_name` referencing the alias.

The semantic phase resolves the module alias to find the actual struct or enum definition in the imported module.

### Limitations

Cross-module struct types used directly as struct fields can cause issues. The recommended workaround is to use `*u8` pointers with accessor functions that cast:

```cst
// Instead of:
// struct Container { val as mod.StructType; }  // may fail

// Use:
struct Container { val_ptr as *u8; }  // pointer workaround
```

## Type in Context

Types appear in several syntactic positions, and the parser calls the same type-parsing function for all of them:

### Variable Declaration
```cst
let is [64]u8 as buffer;
//     ^^^^^  type position
```

### Function Parameter
```cst
fn process(data as *u8, len as u64) as void { ... }
//                 ^^^        ^^^       ^^^^
//                 type       type      return type
```

### Cast Expression
```cst
cast(*u8, address)
//   ^^^  type position
```

### Sizeof Expression
```cst
sizeof(Point)
//     ^^^^^  type position
```

### Generic Type Argument
```cst
let is Pair gen i32, *u8 as p;
//              ^^^  ^^^  type arguments
```

## Type Size Summary

| Type | Size (bytes) |
|------|-------------|
| `void` | 0 |
| `i8`, `u8`, `bool`, `char` | 1 |
| `i16`, `u16` | 2 |
| `i32`, `u32`, `f32` | 4 |
| `i64`, `u64`, `f64`, `string`, all pointers | 8 |
| `[N]T` | N * sizeof(T) |
| structs | sum of all field sizes (packed, no padding) |

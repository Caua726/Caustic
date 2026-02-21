# Type Aliases

Type aliases create a new name for an existing type.

## Syntax

```cst
type Name = ExistingType;
```

## Examples

```cst
type Byte = u8;
type Size = i64;
type StringPtr = *u8;

fn main() as i32 {
    let is Byte as b = cast(Byte, 42);
    let is Size as n = 1024;
    return cast(i32, b);
}
```

## Behavior

A type alias is purely a compile-time name substitution. The alias and the original type are identical — they have the same size, alignment, and behavior. No runtime cost.

## Scope

Type aliases declared at the top level are visible throughout the file and in any file that imports the module:

```cst
// types.cst
type Byte = u8;
```

```cst
// main.cst
use "types.cst" as t;

fn main() as i32 {
    let is t.Byte as b = cast(t.Byte, 10);
    return cast(i32, b);
}
```

## Limitations

- Type aliases cannot be recursive (`type A = *A` does not work).
- Generic type aliases are not supported. Use a generic struct instead.

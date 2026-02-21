# Array Types

Arrays in Caustic are fixed-size, inline collections of elements of a single type. They are stack-allocated for local variables and use contiguous memory with no overhead beyond the element data.

## Type Kind

Arrays use `TY_ARRAY` (kind value 15). The `Type` struct stores:

```
Type {
    kind: TY_ARRAY (15)
    base: *Type      -- element type
    array_len: i64   -- number of elements
    size: i64        -- array_len * base.size
}
```

## Declaration Syntax

Array types are written as `[N]T` where `N` is a compile-time constant and `T` is the element type:

```cst
let is [10]i32 as numbers;       // 10 i32 elements, 40 bytes
let is [256]u8 as buffer;        // 256 bytes
let is [4]Point as points;       // 4 Point structs, inline
```

The size `N` must be a positive integer literal. Variable-length arrays are not supported.

## Memory Layout

Arrays are stored as contiguous blocks of elements with no length prefix, padding, or metadata:

```
[4]i32 layout (16 bytes total):
  offset 0:  element 0  (4 bytes)
  offset 4:  element 1  (4 bytes)
  offset 8:  element 2  (4 bytes)
  offset 12: element 3  (4 bytes)
```

For local variables, the array is allocated on the stack. For global variables, it is allocated in the data section.

## Indexing

Array elements are accessed with bracket syntax. The index must be an integer expression. The result type is the array's element type:

```cst
let is [10]i32 as arr;
arr[0] = 42;                     // write element 0
let is i32 as val = arr[3];      // read element 3

let is i32 as idx = 5;
arr[idx] = 100;                  // variable index
```

### No Bounds Checking

Array indexing does not perform bounds checking at compile time or runtime. Accessing out-of-bounds indices results in undefined behavior (reading/writing arbitrary stack or heap memory):

```cst
let is [10]i32 as arr;
arr[99] = 1;    // compiles without error, corrupts memory at runtime
```

## Element Offset Calculation

The byte offset of element `i` is computed as:

```
offset = i * sizeof(element_type)
```

Since structs are packed, an array of structs also uses packed layout:

```cst
struct Pair { a as i32; b as i8; }
// sizeof(Pair) = 5

let is [3]Pair as pairs;
// pairs[0] at offset 0  (5 bytes)
// pairs[1] at offset 5  (5 bytes)
// pairs[2] at offset 10 (5 bytes)
// total size = 15 bytes
```

## Passing Arrays

Arrays are passed by reference (pointer) to functions. The array decays to a pointer to its first element:

```cst
fn sum(arr as *i32, len as i64) as i64 {
    let is i64 as total = 0;
    for let is i64 as i = 0; i < len; i = i + 1 {
        total = total + cast(i64, *(cast(*i32, cast(i64, arr) + i * 4)));
    }
    return total;
}

fn main() as i32 {
    let is [5]i32 as data;
    data[0] = 1;
    data[1] = 2;
    data[2] = 3;
    data[3] = 4;
    data[4] = 5;
    let is i64 as s = sum(&data[0], 5);
    return cast(i32, s);
}
```

## Arrays of Arrays

Multi-dimensional arrays can be declared by nesting array types:

```cst
let is [3][4]i32 as matrix;   // 3 rows of 4 i32 elements
// total size = 3 * 4 * 4 = 48 bytes
```

Access uses chained indexing:

```cst
matrix[1][2] = 42;
```

## Size

The total size of an array is `N * sizeof(T)`:

```cst
sizeof([10]i32)    // 40  (10 * 4)
sizeof([256]u8)    // 256 (256 * 1)
sizeof([4]Point)   // depends on Point's packed size
```

## Limitations

- **Fixed size only**: The array length must be a compile-time constant. For dynamic arrays, use heap allocation via `std/mem.cst` and pointer arithmetic.
- **No array literals**: Arrays cannot be initialized with a literal syntax like `{1, 2, 3}`. Elements must be assigned individually.
- **No length intrinsic**: There is no built-in way to query an array's length at runtime. The length is known only at compile time.

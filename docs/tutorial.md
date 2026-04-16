# Caustic Tutorial

A step-by-step guide to learning Caustic, from first program to advanced features.

Caustic is a systems programming language that compiles directly to x86_64 Linux
executables. No runtime, no garbage collector, no hidden magic. You write code,
the compiler turns it into machine instructions, and Linux runs them. That is it.

If you know Python or JavaScript but have never touched C or assembly, this
tutorial will get you comfortable with Caustic and systems programming concepts
along the way.

---

## 1. Basics

### Your first program

```cst
use "std/io.cst" as io;

fn main() as i32 {
    io.printf("Hello, world!\n");
    return 0;
}
```

Save this as `hello.cst`, then build and run:

```bash
./caustic hello.cst -o hello
./hello
```

Every Caustic program starts at `fn main() as i32`. The return value becomes the
process exit code (0 means success, anything else means error).

### Variables

Variables are declared with `let is TYPE as NAME = VALUE;`:

```cst
use "std/io.cst" as io;

fn main() as i32 {
    let is i32 as age = 25;
    let is i64 as big_number = 1000000;
    let is bool as alive = true;

    io.printf("Age: %d\n", cast(i64, age));
    io.printf("Big: %d\n", big_number);
    return 0;
}
```

The syntax reads like a sentence: "let this is an i32, as age, equal to 25."
The type comes before the name because Caustic wants you to always know what
type you are working with. No guessing, no inference.

By default, variables are immutable. If you need to change a variable after
declaring it, add `with mut`:

```cst
let is i32 as counter with mut = 0;
counter = counter + 1;  // this works

let is i32 as fixed = 10;
// fixed = 20;  // ERROR: cannot assign to immutable variable
```

### Types

Caustic has explicit numeric types with defined sizes:

| Type | Size | Range |
|------|------|-------|
| `i8`, `i16`, `i32`, `i64` | 1, 2, 4, 8 bytes | Signed integers |
| `u8`, `u16`, `u32`, `u64` | 1, 2, 4, 8 bytes | Unsigned integers |
| `f32`, `f64` | 4, 8 bytes | Floating point |
| `bool` | 1 byte | `true` or `false` |
| `char` | 1 byte | A single character |
| `*u8` | 8 bytes | Pointer to bytes (used for strings) |

A **pointer** (`*T`) is just an address in memory where data lives. Think of it
like a house number: the pointer itself is the number, and the data is what is
inside the house. `*u8` is the most common pointer type -- it points to raw
bytes, and string literals in Caustic are `*u8`.

### Printing

Caustic provides several ways to print. The most convenient is `io.printf`,
which works like C's printf:

```cst
use "std/io.cst" as io;

fn main() as i32 {
    io.printf("A number: %d\n", 42);
    io.printf("A string: %s\n", "hello");

    // For simple cases, there are also:
    io.write_str(1, "Direct string output\n");
    io.write_int(1, 123);
    io.print_int(42);  // prints to stdout

    return 0;
}
```

The `%d` format prints integers, `%s` prints null-terminated strings, and `%x`
prints hexadecimal. The `1` in `write_str` and `write_int` is the file
descriptor for stdout (standard output).

### Functions

```cst
use "std/io.cst" as io;

fn add(a as i32, b as i32) as i32 {
    return a + b;
}

fn greet(name as *u8) as void {
    io.printf("Hello, %s!\n", name);
}

fn main() as i32 {
    let is i32 as result = add(10, 20);
    io.printf("10 + 20 = %d\n", cast(i64, result));

    greet("Caustic");
    return 0;
}
```

Functions declare parameter types after each parameter name (`a as i32`) and the
return type after the parameter list (`as i32`). Use `as void` when a function
does not return anything.

### Control flow

```cst
use "std/io.cst" as io;

fn classify(n as i32) as void {
    if (n > 0) {
        io.printf("%d is positive\n", cast(i64, n));
    } else if (n < 0) {
        io.printf("%d is negative\n", cast(i64, n));
    } else {
        io.printf("zero\n");
    }
}

fn countdown() as void {
    let is i32 as i with mut = 5;
    while (i > 0) {
        io.printf("%d...\n", cast(i64, i));
        i = i - 1;
    }
    io.printf("Go!\n");
}

fn sum_range() as void {
    let is i32 as total with mut = 0;
    for (let is i32 as i = 0; i < 10; i = i + 1) {
        total = total + i;
    }
    io.printf("Sum 0..9 = %d\n", cast(i64, total));
}

fn main() as i32 {
    classify(42);
    classify(0);
    countdown();
    sum_range();
    return 0;
}
```

The `for` loop uses C-style syntax: `for (INIT; CONDITION; STEP) { ... }`.
Note that Caustic has no `++` or `+=` operators -- you write `i = i + 1`.

---

## 2. Strings and Arrays

### String literals

In Caustic, a string literal like `"hello"` is a pointer to bytes (`*u8`). The
bytes are stored in the program's read-only data section, null-terminated (a zero
byte at the end marks where the string stops). This is how strings work at the
machine level -- no hidden objects, no reference counting.

```cst
use "std/io.cst" as io;

fn main() as i32 {
    let is *u8 as greeting = "Hello!";
    io.printf("%s\n", greeting);

    // You can index into strings like arrays
    let is u8 as first = greeting[0];  // 'H' = 72
    io.printf("First byte: %d\n", cast(i64, first));

    return 0;
}
```

### Arrays

Arrays have a fixed size known at compile time:

```cst
use "std/io.cst" as io;

fn main() as i32 {
    let is [5]i32 as numbers;
    numbers[0] = 10;
    numbers[1] = 20;
    numbers[2] = 30;
    numbers[3] = 40;
    numbers[4] = 50;

    let is i32 as i with mut = 0;
    while (i < 5) {
        io.printf("numbers[%d] = %d\n", cast(i64, i), cast(i64, numbers[i]));
        i = i + 1;
    }
    return 0;
}
```

The syntax `[5]i32` means "an array of 5 i32 values." Arrays live on the stack
(local memory), so they are fast and automatically cleaned up when the function
returns.

### The string module

For working with dynamic strings (concatenation, searching, etc.), use the
string module:

```cst
use "std/io.cst" as io;
use "std/string.cst" as str;

fn main() as i32 {
    let is str.String as hello = str.String("Hello");
    let is str.String as world = str.String(" World");
    let is str.String as msg = str.concat(hello, world);

    io.printf("Message: %s\n", msg.ptr);
    io.printf("Length: %d\n", cast(i64, msg.len));

    // Find a substring
    let is str.String as needle = str.String("World");
    let is i32 as pos = str.find(msg, needle);
    io.printf("\"World\" found at index: %d\n", cast(i64, pos));

    return 0;
}
```

A `str.String` is a struct with three fields: `ptr` (pointer to the bytes),
`len` (current length), and `cap` (allocated capacity). When you need to pass it
to `printf`, use `.ptr` to get the raw `*u8`.

---

## 3. Structs

Structs group related data together.

```cst
use "std/io.cst" as io;

struct Point {
    x as i32;
    y as i32;
}

fn print_point(p as Point) as void {
    io.printf("(%d, %d)", cast(i64, p.x), cast(i64, p.y));
}

fn main() as i32 {
    let is Point as p;
    p.x = 10;
    p.y = 20;

    io.printf("Point: ");
    print_point(p);
    io.printf("\n");

    return 0;
}
```

### Passing by pointer

When you pass a struct to a function, it gets copied. If you want the function
to modify the original, pass a pointer:

```cst
use "std/io.cst" as io;

struct Point {
    x as i32;
    y as i32;
}

fn move_right(p as *Point, amount as i32) as void {
    p.x = p.x + amount;  // modifies the original
}

fn main() as i32 {
    let is Point as p;
    p.x = 5;
    p.y = 10;

    move_right(&p, 3);  // &p takes the address of p
    io.printf("After move: (%d, %d)\n", cast(i64, p.x), cast(i64, p.y));
    // prints: After move: (8, 10)

    return 0;
}
```

The `&` operator takes the address of a variable (creates a pointer to it), and
a `*Point` parameter means "pointer to a Point." When you access fields through
a pointer (`p.x`), Caustic automatically dereferences it for you.

### Packed layout

Caustic structs are packed -- there is no padding between fields. A struct with
an `i32` and an `i64` takes exactly 12 bytes, not 16. This gives you precise
control over memory layout.

---

## 4. Enums and Match

Enums define a type that can be one of several variants. Variants can optionally
carry data, making them tagged unions.

```cst
use "std/io.cst" as io;

// Simple enum (no data)
enum Color {
    Red;
    Green;
    Blue;
}

// Tagged union (variants carry data)
enum Shape {
    Circle as i32;      // radius
    Rectangle as i64;   // encodes width and height
    None;
}

fn describe_color(c as Color) as void {
    match Color (c) {
        case Red   { io.printf("Red\n"); }
        case Green { io.printf("Green\n"); }
        case Blue  { io.printf("Blue\n"); }
    }
}

fn circle_area(s as Shape) as i64 {
    match Shape (s) {
        case Circle(r) {
            return cast(i64, r) * cast(i64, r) * 3;
        }
        case Rectangle(data) {
            return data;
        }
        else {
            return 0;
        }
    }
    return 0;
}

fn main() as i32 {
    let is Color as c = Color.Red;
    describe_color(c);

    let is Shape as s = Shape.Circle(10);
    io.printf("Area: %d\n", circle_area(s));

    // Comparison works too
    if (c == Color.Red) {
        io.printf("It is red!\n");
    }

    return 0;
}
```

The `match` statement is how you inspect which variant an enum holds. For tagged
unions, `case Circle(r)` destructures the variant and binds the inner value to
`r`. The `else` branch catches anything not explicitly matched.

---

## 5. Modules

Every `.cst` file can be used as a module. You import with `use`:

```cst
use "std/io.cst" as io;
use "std/linux.cst" as linux;
use "std/string.cst" as str;
use "std/math.cst" as math;
```

Then access anything from that module through the alias:

```cst
io.printf("Hello\n");
let is i64 as bigger = math.max(10, 20);
```

### Selective imports

If a module has submodules, you can import only the ones you need:

```cst
use "std/mem.cst" only bins, arena as mem;
// Now you can use: mem.bins.bins_new(), mem.arena.create()
// But not: mem.freelist, mem.pool, etc.
```

### Creating your own module

Any `.cst` file with functions is a module. Say you create `utils.cst`:

```cst
// utils.cst
fn double(n as i32) as i32 {
    return n * 2;
}

fn is_even(n as i32) as bool {
    return n % 2 == 0;
}
```

Then in your main file:

```cst
use "utils.cst" as utils;
use "std/io.cst" as io;

fn main() as i32 {
    io.printf("double(5) = %d\n", cast(i64, utils.double(5)));
    return 0;
}
```

Modules are compiled once and cached, so importing the same file in multiple
places does not slow anything down.

---

## 6. Memory Management

This is where Caustic differs most from Python or JavaScript. There is no garbage
collector. When you need memory beyond what the stack provides, you allocate it
yourself and free it when you are done.

Why? Programs that control their memory are faster and more predictable. There
are no pauses while a GC scans your heap, no surprise allocations, no hidden
overhead. You know exactly when memory is allocated and freed.

### The allocator

```cst
use "std/io.cst" as io;
use "std/mem.cst" as mem;

let is mem.bins.Bins as heap with mut;

fn main() as i32 {
    // Initialize the allocator with a 1MB pool
    heap = mem.bins.bins_new(1048576);

    // Allocate 256 bytes
    let is *u8 as buffer = mem.bins.bins_alloc(&heap, 256);

    // Use the buffer...
    buffer[0] = 72;  // 'H'
    buffer[1] = 105; // 'i'
    buffer[2] = 0;   // null terminator
    io.printf("%s\n", buffer);

    // Free it when done
    mem.bins.bins_free(&heap, buffer);

    return 0;
}
```

The pattern is: `bins_new` creates an allocator, `bins_alloc` gives you memory,
`bins_free` returns it. The bins allocator is O(1) for both allocation and
freeing -- it uses size-class slabs internally.

### Defer

Forgetting to free memory is a common bug. `defer` helps by scheduling a
function call to run automatically before the function returns:

```cst
use "std/mem.cst" as mem;

let is mem.bins.Bins as heap with mut;

fn work() as i32 {
    let is *u8 as data = mem.bins.bins_alloc(&heap, 1024);
    defer mem.bins.bins_free(&heap, data);

    // ... do work with data ...
    // No matter where you return, bins_free will be called.

    if (data[0] == 0) {
        return 1;  // bins_free runs here
    }
    return 0;      // bins_free runs here too
}
```

Multiple defers execute in LIFO order (last registered, first executed), so
resources are released in the reverse order of acquisition.

### The five allocators

Caustic's `std/mem.cst` provides five allocators, each optimized for different
patterns:

| Allocator | Best for | Speed |
|-----------|----------|-------|
| `bins` | General purpose, mixed sizes | O(1) alloc and free |
| `arena` | Bulk allocation, free everything at once | O(1) alloc, bulk free |
| `pool` | Many objects of the same size | O(1) alloc and free |
| `lifo` | Stack-like allocation (last in, first out) | O(1) alloc and free |
| `freelist` | Simple, no size classes needed | O(n) alloc, O(1) free |

For most programs, `bins` is the right choice.

---

## 7. Pointers In Depth

Pointers are central to systems programming. In Caustic, they are explicit and
transparent -- there is no hidden indirection.

### The basics

```cst
use "std/io.cst" as io;

fn main() as i32 {
    let is i32 as x with mut = 42;

    let is *i32 as ptr = &x;   // & takes the address
    io.printf("Value: %d\n", cast(i64, *ptr));  // * dereferences

    *ptr = 100;                // write through the pointer
    io.printf("Now x = %d\n", cast(i64, x));  // prints 100

    return 0;
}
```

- `&x` creates a pointer to `x` (its memory address)
- `*ptr` reads the value at that address
- `*ptr = 100` writes to that address, changing `x`

### Type casting

Sometimes you need to convert between pointer types or between pointers and
integers:

```cst
let is *u8 as raw = mem.bins.bins_alloc(&heap, 64);
let is *i32 as int_ptr = cast(*i32, raw);  // reinterpret as *i32
int_ptr[0] = 42;
int_ptr[1] = 100;
```

`cast(TargetType, value)` performs type conversions. For pointers, it
reinterprets the address without changing it -- you are just telling the compiler
to treat the bytes differently.

### Array indexing

Both arrays and pointers support indexing with `[i]`:

```cst
let is [4]i32 as arr;
arr[0] = 10;       // array indexing

let is *i32 as p = &arr[0];
let is i32 as val = p[2];  // pointer indexing (same as arr[2])
```

### Why raw pointers?

Languages like Python hide pointers behind objects, references, and garbage
collectors. That convenience costs performance and predictability. In Caustic,
when you write `p.x`, you know exactly what the machine does: load a value from
memory at a specific offset. No vtable lookup, no reference counting, no
indirection. You see the cost of every operation.

---

## 8. Impl Blocks (Methods)

Impl blocks let you attach functions to structs, giving you method call syntax.

```cst
use "std/io.cst" as io;

struct Point {
    x as i32;
    y as i32;
}

impl Point {
    // Associated function (no self) -- works like a constructor
    fn new(x as i32, y as i32) as Point {
        let is Point as p;
        p.x = x;
        p.y = y;
        return p;
    }

    // Method (takes self as *Point)
    fn sum(self as *Point) as i32 {
        return self.x + self.y;
    }

    fn scale(self as *Point, factor as i32) as void {
        self.x = self.x * factor;
        self.y = self.y * factor;
    }
}

fn main() as i32 {
    // Associated function -- called on the type
    let is Point as p = Point.new(3, 4);

    // Methods -- called on an instance
    io.printf("Sum: %d\n", cast(i64, p.sum()));

    p.scale(10);
    io.printf("After scale: (%d, %d)\n", cast(i64, p.x), cast(i64, p.y));

    return 0;
}
```

Under the hood, `p.sum()` is just syntactic sugar for `Point_sum(&p)`. The
compiler rewrites method calls into regular function calls with the address of
the struct as the first argument. No vtables, no dynamic dispatch -- just a
normal function call.

**Associated functions** (no `self` parameter) are called on the type name:
`Point.new(3, 4)`. These are typically used as constructors.

**Methods** (with `self as *Point`) are called on instances: `p.sum()`. The
compiler automatically passes `&p` as the first argument.

---

## 9. Generics

Generics let you write functions and structs that work with any type.

### Generic functions

```cst
use "std/io.cst" as io;

fn max gen T (a as T, b as T) as T {
    if (a > b) { return a; }
    return b;
}

fn main() as i32 {
    let is i32 as a = max gen i32 (10, 20);
    let is i64 as b = max gen i64 (100, 42);

    io.printf("max i32: %d\n", cast(i64, a));
    io.printf("max i64: %d\n", b);

    return 0;
}
```

The `gen T` after the function name declares a type parameter. When you call
`max gen i32 (10, 20)`, the compiler generates a specialized version of `max`
that works specifically with `i32`. This is called monomorphization.

### Generic structs

```cst
use "std/io.cst" as io;

struct Pair gen T, U {
    first as T;
    second as U;
}

fn main() as i32 {
    let is Pair gen i32, i64 as p;
    p.first = 42;
    p.second = 1000;

    io.printf("Pair: (%d, %d)\n", cast(i64, p.first), p.second);

    return 0;
}
```

### Generic swap

A practical example -- swapping two values of any type:

```cst
use "std/io.cst" as io;

fn swap gen T (a as *T, b as *T) as void {
    let is T as tmp = *a;
    *a = *b;
    *b = tmp;
}

fn main() as i32 {
    let is i32 as x with mut = 10;
    let is i32 as y with mut = 20;

    io.printf("Before: x=%d, y=%d\n", cast(i64, x), cast(i64, y));
    swap gen i32 (&x, &y);
    io.printf("After:  x=%d, y=%d\n", cast(i64, x), cast(i64, y));

    return 0;
}
```

### Zero runtime cost

Because generics are monomorphized at compile time, `max gen i32` and
`max gen i64` become two separate functions in the final binary. There is no
type erasure, no boxing, no interface dispatch. The generated code is identical
to what you would write by hand for each specific type.

### Using the standard library's generics

The standard library uses generics extensively. For example, `Slice gen T` is a
dynamic array:

```cst
use "std/io.cst" as io;
use "std/slice.cst" as slice;

fn main() as i32 {
    let is slice.Slice gen i32 as nums = slice.slice_new gen i32 (8);

    nums.push(10);
    nums.push(20);
    nums.push(30);

    let is i32 as i with mut = 0;
    while (i < nums.len) {
        io.printf("nums[%d] = %d\n", cast(i64, i), cast(i64, nums.get(i)));
        i = i + 1;
    }

    return 0;
}
```

---

## 10. What's Next

You now know enough Caustic to write real programs. Here is where to go from
here:

- **Language reference**: `docs/language.md` -- complete syntax and semantics
- **Standard library docs**: `docs/stdlib/` -- documentation for every module
- **Examples**: `examples/` in the repo -- working programs covering linked
  lists, sorting, file I/O, networking, and more
- **Editor support**: Caustic has an LSP server (`caustic-lsp`) and a VS Code
  extension for syntax highlighting, hover docs, and go-to-definition

### Building and running

```bash
# Compile and run in one step
./caustic program.cst -o program && ./program

# Compile only (no main required, for library files)
./caustic -c library.cst

# Optimized build
./caustic -O1 program.cst -o program

# Debug: see tokens, AST, or IR
./caustic -debuglexer program.cst
./caustic -debugparser program.cst
./caustic -debugir program.cst
```

### Key things to remember

1. **Types are explicit.** You always know what type a variable is.
2. **Memory is manual.** Allocate with `bins_alloc`, free with `bins_free`, use
   `defer` to avoid leaks.
3. **No hidden costs.** Every operation maps directly to machine instructions.
4. **Structs are packed.** No padding between fields.
5. **Generics are free.** Monomorphization means zero runtime overhead.

Welcome to Caustic. Build something.

## _module
Caustic Language Reference

Complete guide to the Caustic programming language syntax and features.

Caustic is a systems programming language that compiles directly to x86_64
Linux executables. No libc, no runtime, no garbage collector. Just your code
and the kernel.
---
## let
let — Declare a variable

Syntax:
  let is TYPE as NAME = VALUE;
  let is TYPE as NAME with mut = VALUE;
  let is TYPE as NAME with imut = VALUE;
  let is TYPE as NAME with section(".name") = VALUE;
  let is TYPE as NAME with mut with section(".name") = VALUE;

By default, variables are immutable. Use `with mut` to allow reassignment,
or `with imut` for compile-time constants (global only).
Use `with section(".name")` to place the variable in a custom ELF section.
The `with` clauses are chainable in any order.

Examples:
  let is i32 as x = 42;
  let is *u8 as name = "hello";
  let is i32 as counter with mut = 0;
  let is [64]u8 as buffer;
  let is i64 as entry with section(".drivers_init") = 99;

Types go before the name, values after. Every variable must have a type.
---
## fn
fn — Declare a function

Syntax:
  fn name(param as TYPE, ...) as RETURN_TYPE {
      ...
  }
  fn name(param as TYPE) as RETURN_TYPE with section(".name") {
      ...
  }

Parameters use `as` for type annotation. Return type comes after the
parameter list. Use `void` if the function returns nothing.
Use `with section(".name")` to place the function in a custom ELF section.

Examples:
  fn add(a as i32, b as i32) as i32 {
      return a + b;
  }

  fn greet(name as *u8) as void {
      io.write_str(linux.STDOUT, name);
  }

  fn early_init() as void with section(".init_text") {
      // placed in .init_text section
  }

Functions are the only way to define reusable code. They can be called
before they are defined (forward references work within the same file).
---
## struct
struct — Define a data structure

Syntax:
  struct Name {
      field as TYPE;
      field2 as TYPE;
  }

Structs are value types with packed layout (no padding between fields).
Create instances with the constructor syntax:

Examples:
  struct Point {
      x as i32;
      y as i32;
  }

  let is Point as p = Point(10, 20);
  let is i32 as sum = p.x + p.y;

Fields are accessed with dot notation. Pass by pointer for mutation:
  fn move(p as *Point, dx as i32) as void {
      p.x = p.x + dx;
  }
  move(&p, 5);

Important: Caustic structs are packed. sizeof(Point) = 8, not 16.
Mixing field sizes (i32 + i64) may not match C struct layout.
---
## enum
enum — Define a tagged union

Syntax:
  enum Name {
      Variant;
      Variant(TYPE);
      Variant(TYPE, TYPE);
  }

Enums are tagged unions. Each variant can optionally carry data.
Use `match` to destructure.

Examples:
  enum Shape {
      Circle(f64);
      Rect(f64, f64);
      None;
  }

  let is Shape as s = Shape.Circle(5.0);

  match (s) {
      case Circle(r) { return 3.14 * r * r; }
      case Rect(w, h) { return w * h; }
      case None { return 0.0; }
  }
---
## match
match — Pattern match on enums

Syntax:
  match (expr) {
      case Variant { ... }
      case Variant(binding) { ... }
  }

Each case destructures the enum variant and binds its data to local
variables. All variants must be covered.

Example:
  match (result) {
      case Ok(val) { io.write_int(linux.STDOUT, val); }
      case Err(msg) { io.write_str(linux.STDERR, msg); }
  }
---
## use
use — Import a module

Syntax:
  use "path/to/module.cst" as alias;
  use "path/to/module.cst" only sub1, sub2 as alias;

Imports make another file's functions, structs, and globals available
under the given alias.

Examples:
  use "../std/io.cst" as io;
  use "../std/mem.cst" only bins, core as mem;

  io.write_str(linux.STDOUT, "hello\n");
  mem.bins.bins_alloc(&b, 64);

The `only` clause imports specific submodules from a multi-file module
like mem (which has bins, core, arena, pool, lifo).
---
## impl
impl — Implement methods for a type

Syntax:
  impl TypeName {
      fn method(self as *TypeName, ...) as TYPE { ... }
      fn assoc_fn(...) as TYPE { ... }
  }

Methods that take `self` as first parameter are called with dot syntax.
Functions without `self` are associated functions (called with Type.func).

Example:
  impl Point {
      fn new(x as i32, y as i32) as Point {
          return Point(x, y);
      }
      fn magnitude(self as *Point) as i32 {
          return self.x * self.x + self.y * self.y;
      }
  }

  let is Point as p = Point.new(3, 4);
  let is i32 as mag = p.magnitude();
---
## if
if / else — Conditional branching

Syntax:
  if (condition) { ... }
  if (condition) { ... } else { ... }
  if (condition) { ... } else if (condition) { ... } else { ... }

Conditions must be in parentheses. Braces are required.

Example:
  if (x > 0) {
      io.write_str(linux.STDOUT, "positive\n");
  } else if (x == 0) {
      io.write_str(linux.STDOUT, "zero\n");
  } else {
      io.write_str(linux.STDOUT, "negative\n");
  }
---
## while
while — Loop while condition is true

Syntax:
  while (condition) { ... }

Example:
  let is i32 as i with mut = 0;
  while (i < 10) {
      io.write_int(linux.STDOUT, cast(i64, i));
      i = i + 1;
  }
---
## for
for — C-style for loop

Syntax:
  for (init; condition; step) { ... }

Example:
  for (let is i32 as i with mut = 0; i < 10; i = i + 1) {
      io.write_int(linux.STDOUT, cast(i64, i));
  }
---
## return
return — Return a value from a function

Syntax:
  return EXPR;
  return;

The return value type must match the function's declared return type.
Deferred calls execute after the return expression is evaluated but
before the function actually returns.
---
## break
break — Exit the current loop

Used inside `while` and `for` loops to exit immediately.

Example:
  while (1 == 1) {
      let is i32 as ch = io.read(&reader);
      if (ch < 0) { break; }
  }
---
## continue
continue — Skip to next loop iteration

Skips the rest of the loop body and goes back to the condition check.
---
## cast
cast — Type conversion

Syntax:
  cast(TARGET_TYPE, expression)

Converts between numeric types, pointers, and integers. No runtime
checks — it's a direct bit reinterpretation or truncation/extension.

Examples:
  let is i32 as x = cast(i32, some_i64);
  let is *u8 as ptr = cast(*u8, address);
  let is i64 as addr = cast(i64, some_ptr);
---
## sizeof
sizeof — Get the size of a type in bytes

Syntax:
  sizeof(TYPE)

Returns the size in bytes as i64. Useful for memory allocation.

Examples:
  let is i64 as sz = sizeof(Point);      // 8 (two i32 fields, packed)
  let is *u8 as buf = mem.galloc(sizeof(Point) * 100);
---
## defer
defer — Schedule a cleanup call

Syntax:
  defer function_call(args);

The deferred call runs automatically before every `return` in the
current scope. Multiple defers execute in LIFO order (last first).

Example:
  fn process() as i32 {
      let is *u8 as buf = mem.galloc(4096);
      defer mem.gfree(buf);
      // ... use buf ...
      return 0;   // gfree(buf) called automatically
  }

Rules:
- Only function calls can be deferred (not expressions)
- The return value is evaluated before defers run
- Defers inside if/while/for are scoped to that block
---
## type
type — Create a type alias

Syntax:
  type NewName = ExistingType;

Creates an alternative name for a type. Does not create a new type.

Examples:
  type Byte = u8;
  type Size = i64;
  type Callback = *u8;
---
## section
with section — Place a symbol in a custom ELF section

Syntax:
  let is TYPE as NAME with section(".section_name") = VALUE;
  fn name() as TYPE with section(".section_name") { ... }

By default, globals go in `.data` or `.bss` and functions go in `.text`.
`with section(".name")` overrides this, placing the symbol in a named
ELF section. When multiple .o files have the same section name, the linker
merges them into one contiguous block.

This is essential for building driver frameworks, init tables, plugin
registries, and other patterns where you need auto-discovery at link time.

The `with` clause chains with `mut` and `imut`:
  let is i64 as x with mut with section(".mydata") = 0;

Examples:
  // Each driver file registers itself:
  let is i64 as ahci_entry with section(".drivers_init") = 1;
  let is i64 as nvme_entry with section(".drivers_init") = 2;

  // After linking, both entries are contiguous in .drivers_init

Rules:
- Only works on global variables and top-level functions
- Section name must be a string literal
- One section per declaration (no duplicate `with section`)
---
## gen
gen — Generic type parameters

Syntax:
  fn name gen T (param as T) as T { ... }
  struct Name gen T { field as T; }

Generics are monomorphized at compile time — each instantiation creates
a specialized copy. Use `gen TYPE` at the call site.

Examples:
  fn max gen T (a as T, b as T) as T {
      if (a > b) { return a; }
      return b;
  }

  let is i32 as m = max gen i32 (3, 7);

  struct Pair gen T, U {
      first as T;
      second as U;
  }

  let is Pair gen i32, *u8 as p = Pair gen i32, *u8 (42, "hello");
---
## extern
extern — Declare external function

Syntax:
  extern fn name(params) as TYPE;

Declares a function that is defined in another object file (for
multi-file compilation) or in a shared library (dynamic linking).

Example:
  extern fn compute(x as i64) as i64;
  let is i64 as result = compute(42);
---
## syscall
syscall — Direct Linux system call

Syntax:
  syscall(NUMBER, arg1, arg2, ...);

Invokes a Linux kernel system call directly. Up to 6 arguments.
Returns the syscall result in rax.

Example:
  // write(1, "hello\n", 6)
  syscall(1, 1, "hello\n", 6);
---
## asm
asm — Inline assembly

Syntax:
  asm("instruction\n");

Inserts raw x86_64 assembly into the output. Use \n to separate
multiple instructions.

Example:
  asm("mov rax, 1\n");
  asm("xor rdi, rdi\n");
---
## fn_ptr
fn_ptr — Get a function pointer

Syntax:
  let is *u8 as p = fn_ptr(function_name);
  let is *u8 as p = fn_ptr(module.function);
  let is *u8 as p = fn_ptr(func gen TYPE);

Returns a typed function pointer that can be called with `call()`.

Example:
  fn double(x as i32) as i32 { return x * 2; }
  let is *u8 as fp = fn_ptr(double);
  let is i32 as result = call(fp, 5);  // returns 10
---
## call
call — Call through a function pointer

Syntax:
  call(fn_ptr_value, arg1, arg2, ...)

Invokes a function through a pointer obtained from `fn_ptr()`.
Type-checked at compile time.
---
## Types
Caustic Primitive Types

Integer types (signed):
  i8    — 1 byte  (-128 to 127)
  i16   — 2 bytes (-32768 to 32767)
  i32   — 4 bytes (-2^31 to 2^31-1)
  i64   — 8 bytes (-2^63 to 2^63-1)

Integer types (unsigned):
  u8    — 1 byte  (0 to 255)
  u16   — 2 bytes (0 to 65535)
  u32   — 4 bytes (0 to 2^32-1)
  u64   — 8 bytes (0 to 2^64-1)

Floating point:
  f32   — 4 bytes (single precision)
  f64   — 8 bytes (double precision)

Other:
  bool   — true (1) or false (0)
  char   — single character
  void   — no value (function returns nothing)
  string — null-terminated string literal (*u8)

Pointer types:
  *T    — pointer to T (e.g. *u8, *Point, **u8)

Array types:
  [N]T  — fixed-size array of N elements of type T
---
## Operators
Caustic Operators

Arithmetic:
  +   addition
  -   subtraction / negation
  *   multiplication
  /   division (integer division truncates toward zero)
  %   modulo (remainder)

Comparison (return 1 or 0):
  ==  equal
  !=  not equal
  <   less than
  >   greater than
  <=  less or equal
  >=  greater or equal

Logical:
  &&  logical AND
  ||  logical OR
  !   logical NOT

Bitwise:
  &   bitwise AND / address-of (prefix)
  |   bitwise OR
  ^   bitwise XOR
  ~   bitwise NOT
  <<  left shift
  >>  right shift

Assignment:
  =   assign

Pointer:
  *   dereference (prefix)
  &   address-of (prefix)
  []  array index
  .   member access
---

# Caustic

Compilator x86_64 (Linux) write in C (for now),
without LLVM, without runtime

## Build & Run

```sh
$ make
```

Or manually:

```sh
$ make
$ ./caustic main.cst
$ gcc -no-pie main.cst.s -o main
$ ./main
```

## Capabilities

### Core

- [x] **Native Code Generation** 
- [x] **Direct Syscalls**
- [x] **Pointers, Arrays, Structs**
- [x] **Global Variables**
- [x] **String Literals**
- [x] **Module System**
- [x] **Functions**
- [-] **Standard Library**
  - [x] **Memory Management**
  - [X] **Dynamic Strings**
  - [-] **IO Library**
- [-] **Language Extensions**
  - [x] **Generics**
  - [x] **Enums & Pattern Matching**
- [ ] **Compiler Evolution**
  - [ ] **Optimization**
  - [ ] **Self Hosting**

### Not important, but just to note

- [ ] **Defer**
  - ```cst
    defer free(ptr);
    ```
    this will generate a stack of function calls to be executed when the function returns
- [ ] **Assembler**
  - i need to develop a way to transform the generated assembly code into machine code without nasm or gcc
- [ ] **Linker**
  - i need to develop a way to link the generated machine code into a runnable executable
- [ ] **FFI / C Interop**
  - i need to develop a way to call C functions from Caustic

### Syntax Dump

#### Entry Point
```cst
fn main() as i32 {
    return 0;
}
```

#### Variables & Globals
```cst
// Stack (Local)
let is i32 as x = 10;
let is *u8 as ptr;
let is [64]u8 as buffer;

// Data Section (Global)
let is i32 as global_count with mut = 0;
```

#### Structs & Layout
```cst
struct Vec3 {
    x as i32;
    y as i32;
    z as i32;
}

let is Vec3 as v;
v.x = 10;
let is i32 as size = sizeof(Vec3); // 12
```

#### Enums & Pattern Matching
```cst
// Simple enum
enum Color { Red; Green; Blue; }

// Tagged union (with data)
enum Shape { Circle as i32; Rect as i32, i32; None; }

// Generic enum
enum Option gen T { Some as T; None; }

// Construction
let is Color as c = Color.Blue;
let is Shape as s = Shape.Rect(10, 20);
let is Option gen i32 as x = Option gen i32 .Some(42);

// Pattern matching
match Color (c) {
    case Red { return 1; }
    case Green { return 2; }
    case Blue { return 3; }
}

// Destructuring
match Shape (s) {
    case Rect(w, h) { return w + h; }
    case Circle(r) { return r; }
    else { return 0; }
}
```

#### Flow Control
```cst
if (1 == 1) {
    // ...
}

while (1) {
    if (x > 10) { break; }
    continue;
}

// Short-circuit logic
if (ptr != 0 && *ptr == 10) { ... }
```

#### Low Level / Unsafe
```cst
// Syscall 1 (Write) to FD 1 (Stdout)
syscall(1, 1, "Hello World\n", 12);

// Direct Assembly
asm("mov rax, 1\n");

// Bitwise & Casting
let is i32 as align = (size + 15) & ~15;
let is *u8 as raw_ptr = cast(*u8, 0x7fffffff);
let is i32 as shifted = 1 << 4;
```

#### Modules
```cst
use "std/mem.cst";

fn main() as i32 {
    let is *u8 as mem = alloc(1024);
    return 0;
}
```
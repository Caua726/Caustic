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
  - [ ] **Dynamic Strings**
  - [ ] **IO Library**
- [ ] **Language Extensions**
  - [ ] **FFI / C Interop**
  - [ ] **Generics**
  - [ ] **Enums & Pattern Matching** 
- [ ] **Compiler Evolution**
  - [ ] **Optimization**
  - [ ] **Self Hosting**

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
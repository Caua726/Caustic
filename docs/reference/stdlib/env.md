# std/env.cst -- Environment

Command-line argument access and environment variable lookup. Requires a one-time `init()` call at the start of `main` to capture `argc`, `argv`, and `envp` from the process stack.

## Dependencies

```cst
use "linux.cst" as linux;
use "string.cst" as str;
use "mem.cst" as mem;
```

## Import

```cst
use "std/env.cst" as env;
```

## Functions

### Initialization

| Function | Signature | Description |
|----------|-----------|-------------|
| `init` | `fn init(argc as i64, argv as *u8) as void` | Initialize the module. Must be called before any other `env` function. Computes `envp` from `argv` automatically |

### Argument Access

| Function | Signature | Description |
|----------|-----------|-------------|
| `argc` | `fn argc() as i64` | Number of command-line arguments |
| `argv` | `fn argv(index as i64) as *u8` | Get argument by index as raw `*u8`. Returns null if out of bounds |
| `arg` | `fn arg(index as i64) as str.String` | Get argument by index as heap-allocated `String`. Returns empty String if out of bounds |
| `program` | `fn program() as *u8` | Get program name (`argv[0]`) |

### Environment Variables

| Function | Signature | Description |
|----------|-----------|-------------|
| `getenv` | `fn getenv(name as *u8) as *u8` | Look up environment variable by name. Returns pointer to value (after `=`) or null if not found |
| `getenv_string` | `fn getenv_string(name as *u8) as str.String` | Look up environment variable as heap-allocated `String`. Returns empty String if not found |
| `env_count` | `fn env_count() as i64` | Number of environment variables |
| `env_entry` | `fn env_entry(index as i64) as *u8` | Get raw envp entry by index (returns `"KEY=VALUE"` string) |

## Notes

- `init()` computes `envp` by walking past `argv`: `envp = argv + (argc + 1) * 8`. This follows the Linux process stack layout where the environment pointer array sits immediately after the null-terminated argv array.
- `getenv()` returns a pointer into the original environment block -- it does not allocate. The pointer is valid for the lifetime of the process.
- `getenv_string()` copies the value onto the heap via `str.String()`. The caller should free it when done.
- `arg()` also copies onto the heap. For read-only access without allocation, use `argv()` instead.

## Example

```cst
use "std/mem.cst" as mem;
use "std/env.cst" as env;
use "std/io.cst" as io;

fn main(argc as i64, argv as *u8) as i32 {
    mem.gheapinit(65536);
    env.init(argc, argv);

    // Print program name
    io.printf("program: %s\n", env.program());

    // Print all arguments
    let is i64 as i with mut = 0;
    while (i < env.argc()) {
        io.printf("argv[%d] = %s\n", i, env.argv(i));
        i = i + 1;
    }

    // Look up an environment variable
    let is *u8 as home = env.getenv("HOME");
    if (cast(i64, home) != 0) {
        io.printf("HOME = %s\n", home);
    }

    // Count environment variables
    io.printf("env count: %d\n", env.env_count());

    return 0;
}
```

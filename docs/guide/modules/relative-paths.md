# Relative Paths

Module paths in `use` statements are resolved relative to the file that contains them.

## Resolution rules

The string in `use "path.cst" as alias;` is treated as a filesystem path relative to the directory of the current source file.

```cst
// File: project/main.cst
use "lib/math.cst" as math;       // resolves to project/lib/math.cst
use "std/mem.cst" as mem;         // resolves to project/std/mem.cst
```

```cst
// File: project/lib/math.cst
use "../std/mem.cst" as mem;      // resolves to project/std/mem.cst
use "helpers.cst" as helpers;     // resolves to project/lib/helpers.cst
```

## Parent directory traversal

Use `..` to go up one or more levels.

```cst
use "../util.cst" as util;        // one level up
use "../../std/io.cst" as io;     // two levels up
```

## Typical project layout

```
project/
  main.cst              use "std/mem.cst" as mem;
  std/
    mem.cst
    io.cst              use "mem.cst" as mem;
    string.cst          use "mem.cst" as mem;
  lib/
    parser.cst          use "../std/io.cst" as io;
```

Files inside `std/` import siblings with just the filename. Files in other directories use `../` to reach `std/`.

## Notes

- There is no global module search path. All paths are strictly relative.
- The file extension `.cst` is always required in the path string.

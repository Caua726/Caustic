# caustic-mk — dependency resolution

## Overview

Extend the Causticfile format with `depend` for external Caustic libraries and `system` for native shared libraries. The `caustic-mk` build tool resolves dependencies automatically.

## New Causticfile syntax

### User project (game using SDL3)
```
name "meu-jogo"

target "jogo" {
    src "jogo.cst"
    out "jogo"
    depend "sdl3" in "https://github.com/Caua726/caustic-sdl3" tag "v0.1.0"
}
```

### Library project (caustic-sdl3)
```
name "sdl3"
system "SDL3"

target "sdl3" {
    src "sdl3.cst"
}
```

## Fields

### `depend "name" in "url" [tag "version"]`
- Declares a Caustic library dependency
- `name` — local alias used to find the library's .cst files
- `url` — git repository URL to clone from
- `tag` (optional) — git tag to checkout after clone. If omitted, uses HEAD. Pinning to a tag ensures reproducible builds.
- Goes inside a `target {}` block
- Replaces the existing `dep "name" { path "..." }` block syntax (the old syntax is removed)

### `system "libname"`
- Declares a native shared library dependency (maps to `-l<libname>` for the linker)
- Goes at the top level of the Causticfile (project-wide)
- When another project depends on this library, the `system` dependency is inherited

## Parser changes

The existing `Dep` struct in `caustic-maker/parser.cst` needs to be replaced:

```cst
// Old:
struct Dep { name_ptr; name_len; path_ptr; path_len; }

// New:
struct Dep {
    name_ptr as *u8;
    name_len as i32;
    url_ptr as *u8;
    url_len as i32;
    tag_ptr as *u8;    // null if no tag
    tag_len as i32;
}
```

Add `system_libs` array to the top-level project struct for `system "libname"` declarations.

## Resolution flow

When `caustic-mk` processes a target with `depend`:

1. **Check cache**: Look in `.caustic/deps/<name>/` for already-cloned repo
2. **Clone if missing**: `git clone --depth 1 <url> .caustic/deps/<name>/` (shallow clone for speed). If `tag` is specified: `git clone --depth 1 --branch <tag> <url> .caustic/deps/<name>/`
3. **Error handling**: If `git` is not found, print `Error: git is required for dependencies`. If clone fails (network error), print `Error: failed to clone <url>` and exit.
4. **Read dependency's Causticfile**: Parse `.caustic/deps/<name>/Causticfile`
5. **Inherit system libs**: If the dependency declares `system "SDL3"`, add `-lSDL3` to the linker flags
6. **Add include path**: Add `.caustic/deps/<name>/` to the module search path
7. **Recurse**: If the dependency has its own `depend` entries, resolve them too (depth limit: 8)

## Module resolution with dependencies

When the compiler encounters `use "sdl3.cst" as sdl;`, it searches:
1. Relative to source file (current behavior)
2. `<binary_dir>/../lib/caustic/` (current behavior)
3. `/usr/local/lib/caustic/` (current behavior)
4. **NEW**: paths passed via `--path` flag

The `caustic-mk` passes deps paths via `--path` flag (not env var — consistent with existing CLI flags like `-o`, `-c`, `-O1`):

```bash
caustic --path .caustic/deps/sdl3 jogo.cst
```

The compiler adds each `--path` directory to the module search list. Multiple `--path` flags are supported.

### Stdlib access from dependencies

Dependencies can import from `std/` (e.g. `use "std/compatcffi.cst" as cffi;`) because the compiler always has the stdlib path in its search list. The `--path` flag adds to the list, it does not replace it.

## Linker flags

`caustic-mk` collects all `system` declarations from the dependency tree and passes them as `-l` flags:

```bash
# What caustic-mk runs internally:
caustic --path .caustic/deps/sdl3 jogo.cst
caustic-as jogo.cst.s
caustic-ld jogo.cst.s.o -lSDL3 -o jogo
```

## Dependency cache

```
meu-jogo/
├── Causticfile
├── jogo.cst
└── .caustic/
    └── deps/
        └── sdl3/            # cloned from github
            ├── Causticfile
            ├── sdl3.cst
            ├── init.cst
            ├── video.cst
            └── ...
```

## Files to change

| File | Change |
|------|--------|
| `caustic-maker/parser.cst` | Replace `Dep` struct, parse `depend...in...tag`, parse `system` |
| `caustic-maker/executor.cst` | Git clone, read dep Causticfile, collect system libs, pass `--path` flags |
| `caustic-maker/main.cst` | Wire up dependency resolution before build |
| `src/main.cst` | Add `--path` flag support (append to module search list) |

## Validation

1. Create a test library repo with `system "c"` (libc)
2. Create a test project that depends on it with `depend "testlib" in "..." tag "v0.1"`
3. `caustic-mk` should clone, resolve, compile, and link successfully
4. Verify `-lc` is passed to `caustic-ld`
5. Verify `--path` flag works in isolation: `caustic --path /some/dir test.cst`

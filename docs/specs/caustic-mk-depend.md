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
    depend "sdl3" in "https://github.com/Caua726/caustic-sdl3"
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

### `depend "name" in "url"`
- Declares a Caustic library dependency
- `name` — local alias used to find the library's .cst files
- `url` — git repository URL to clone from
- Goes inside a `target {}` block

### `system "libname"`
- Declares a native shared library dependency (maps to `-l<libname>` for the linker)
- Goes at the top level of the Causticfile (project-wide)
- When another project depends on this library, the `system` dependency is inherited

## Resolution flow

When `caustic-mk` processes a target with `depend`:

1. **Check cache**: Look in `.caustic/deps/<name>/` for already-cloned repo
2. **Clone if missing**: `git clone <url> .caustic/deps/<name>/`
3. **Read dependency's Causticfile**: Parse `.caustic/deps/<name>/Causticfile`
4. **Inherit system libs**: If the dependency declares `system "SDL3"`, add `-lSDL3` to the linker flags
5. **Add include path**: Add `.caustic/deps/<name>/` to the module search path so `use "sdl3.cst"` resolves
6. **Recurse**: If the dependency has its own `depend` entries, resolve them too (depth limit: 8)

## Module resolution with dependencies

When the compiler encounters `use "sdl3.cst" as sdl;`, it searches:
1. Relative to source file (current behavior)
2. `<binary_dir>/../lib/caustic/` (current behavior)
3. `/usr/local/lib/caustic/` (current behavior)
4. **NEW**: `.caustic/deps/*/` directories

The `caustic-mk` passes the deps paths to the compiler. Implementation options:
- Environment variable: `CAUSTIC_PATH=.caustic/deps/sdl3:.caustic/deps/other`
- Command line flag: `caustic --path .caustic/deps/sdl3 jogo.cst`

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
| `caustic-maker/parser.cst` | Parse `depend` and `system` fields |
| `caustic-maker/executor.cst` | Git clone, read dep Causticfile, collect system libs, pass flags |
| `caustic-maker/main.cst` | Wire up dependency resolution before build |
| `src/main.cst` | Add `--path` flag or `CAUSTIC_PATH` env var support |

## Validation

1. Create a test library repo with `system "c"` (libc)
2. Create a test project that depends on it
3. `caustic-mk` should clone, resolve, compile, and link successfully
4. Verify `-lc` is passed to `caustic-ld`

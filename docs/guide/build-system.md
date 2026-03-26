# Build System (caustic-mk)

`caustic-mk` is Caustic's build system, written in Caustic itself. It reads a `Causticfile` and orchestrates compilation.

## Causticfile

Create a `Causticfile` in your project root:

```
name "myproject"
version "0.1.0"
author "Your Name"

target "myapp" {
    src "src/main.cst"
    out "build/myapp"
}

script "test" {
    "bash run_tests.sh"
}
```

### Fields

- **name, version, author, license** — project metadata (optional)
- **target "name" { ... }** — compilation target
  - `src "path"` — source file (required)
  - `out "path"` — output binary (defaults to `build/<name>`)
  - `flags "..."` — extra compiler flags (e.g., `"-c"`, `"-O1"`)
  - `depends "other"` — target dependency (built first)
  - `depend "name" in "url" tag "version"` — git dependency
- **system "libname"** — system library to link (e.g., `system "c"`)
- **script "name" { "cmd1" "cmd2" }** — shell commands

## Commands

### build

```sh
./caustic-mk build <target> [--continue]
```

Resolves dependencies, compiles the target, and produces the binary. Uses `--cache .caustic` for fast rebuilds. The `--continue` flag continues on dependency failure.

### run

```sh
./caustic-mk run <name>
```

Runs a script (if found) or the built target binary.

### test

```sh
./caustic-mk test
```

Shorthand for `./caustic-mk run test`.

### clean

```sh
./caustic-mk clean
```

Removes `.caustic/` (cache) and `build/` (outputs).

### init

```sh
./caustic-mk init
```

Interactive Causticfile creation — prompts for project name and generates a template.

## Dependencies

External dependencies are git-managed:

```
target "myapp" {
    src "src/main.cst"
    depend "mylib" in "https://github.com/user/mylib" tag "v1.0"
}
```

Dependencies are cloned to `.caustic/deps/<name>`. Tag pinning ensures reproducible builds. System libraries from dependency Causticfiles are collected transitively.

## How It Works

1. Parse `Causticfile`
2. Resolve target dependencies (recursive)
3. Clone git dependencies (if missing or tag changed)
4. Compile: `caustic <src> -o <tmp> --cache .caustic [--path deps] [-l libs]`
5. Atomic rename: `tmp` → final binary

All compilation uses `fork` + `execve` + `wait4` — no external tools required beyond `git` (for dependencies) and `/bin/sh` (for scripts).

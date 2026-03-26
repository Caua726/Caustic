# Causticfile Format

The `Causticfile` is the project manifest for `caustic-mk`, the Caustic build system. It declares project metadata, build targets, external dependencies, and scripts in a simple declarative format.

Source: `caustic-maker/parser.cst`

## Syntax

The Causticfile uses a line-oriented format with keywords, string literals, and brace-delimited blocks. Comments start with `//` and extend to the end of the line.

```
// This is a comment
keyword "value"
keyword "name" {
    key "value"
}
```

Identifiers can contain letters, digits, underscores, and hyphens. String values are enclosed in double quotes. Whitespace (spaces, tabs, newlines) is ignored between tokens.

## Top-level fields

| Field | Required | Description |
|-------|----------|-------------|
| `name` | No | Project name |
| `version` | No | Project version string |
| `author` | No | Author name |
| `license` | No | License identifier |
| `system` | No | System library to link (repeatable, max 16) |

```
name "myproject"
version "1.0.0"
author "Your Name"
license "MIT"
system "libfoo"
```

The `system` keyword declares a system library that will be passed as `-l<name>` to the linker. It can appear multiple times. System libraries declared in dependency Causticfiles are also collected automatically.

## Targets

A target defines a build unit -- a source file compiled to a binary.

```
target "name" {
    src "path/to/main.cst"
    out "path/to/output"
    flags "-O1"
    depends "other-target"
    depend "dep-name" in "https://github.com/user/repo" tag "v1.0.0"
}
```

| Field | Required | Description |
|-------|----------|-------------|
| `src` | Yes | Path to the main source file |
| `out` | No | Output binary path (defaults to `build/<name>`) |
| `flags` | No | Additional compiler flags (e.g., `"-O1"`) |
| `depends` | No | Name of another target that must be built first |
| `depend` | No | External dependency (repeatable) |

### External dependencies

The `depend` keyword inside a target block declares a git-based dependency:

```
depend "name" in "git-url" tag "version"
```

- `name` -- local directory name under `.caustic/deps/`
- `in "url"` -- git repository URL (required)
- `tag "version"` -- optional git tag/branch to pin

Dependencies are cloned with `--depth 1` into `.caustic/deps/<name>/`. If a tag is specified, the tag is recorded in `.caustic/deps/<name>/.caustic-tag`. When the tag changes, the dependency is re-cloned. Dependency directories are passed to the compiler via `--path`.

## Scripts

A script defines a sequence of shell commands to run.

```
script "name" {
    "command one"
    "command two"
    "command three"
}
```

Commands are executed sequentially using `/bin/sh -c`. If any command returns a non-zero exit code, the script stops immediately and reports the error. Commands are printed to stdout before execution.

## Limits

| Resource | Maximum |
|----------|---------|
| Targets | 32 |
| Scripts | 16 |
| Commands per script | 32 |
| System libraries | 16 |

## Example

The Caustic project's own `Causticfile`:

```
name "caustic"
version "0.1.0"
author "Caua"

target "caustic" {
    src "src/main.cst"
    out "caustic"
}

target "caustic-as" {
    src "caustic-assembler/main.cst"
    out "caustic-as"
}

target "caustic-ld" {
    src "caustic-linker/main.cst"
    out "caustic-ld"
}

target "caustic-mk" {
    src "caustic-maker/main.cst"
    out "caustic-mk"
}

// examples

target "hello" {
    src "examples/hello.cst"
    out "build/hello"
}

target "fibonacci" {
    src "examples/fibonacci.cst"
    out "build/fibonacci"
}

target "calculator" {
    src "examples/calculator.cst"
    out "build/calculator"
}

target "structs" {
    src "examples/structs.cst"
    out "build/structs"
}

target "strings" {
    src "examples/strings.cst"
    out "build/strings"
}

target "files" {
    src "examples/files.cst"
    out "build/files"
}

target "enums" {
    src "examples/enums.cst"
    out "build/enums"
}

target "linked_list" {
    src "examples/linked_list.cst"
    out "build/linked_list"
}

target "generics" {
    src "examples/generics.cst"
    out "build/generics"
}

target "ffi" {
    src "examples/ffi.cst"
    out "build/ffi"
}

target "sorting" {
    src "examples/sorting.cst"
    out "build/sorting"
}

target "random" {
    src "examples/random.cst"
    out "build/random"
}

script "test" {
    "bash tests_asm/test_linker.sh"
}

script "install" {
    "mkdir -p /usr/local/bin"
    "mkdir -p /usr/local/lib/caustic/std"
    "cp caustic /usr/local/bin/caustic"
    "cp caustic-as /usr/local/bin/caustic-as"
    "cp caustic-ld /usr/local/bin/caustic-ld"
    "cp std/*.cst /usr/local/lib/caustic/std/"
    "echo 'caustic installed to /usr/local'"
}

script "uninstall" {
    "rm -f /usr/local/bin/caustic /usr/local/bin/caustic-as /usr/local/bin/caustic-ld"
    "rm -rf /usr/local/lib/caustic"
    "echo 'caustic uninstalled'"
}

script "dist" {
    "mkdir -p caustic-x86_64-linux/bin caustic-x86_64-linux/lib/caustic/std"
    "cp caustic caustic-as caustic-ld caustic-x86_64-linux/bin/"
    "cp std/*.cst caustic-x86_64-linux/lib/caustic/std/"
    "tar czf caustic-x86_64-linux.tar.gz caustic-x86_64-linux"
    "rm -rf caustic-x86_64-linux"
    "echo 'Created caustic-x86_64-linux.tar.gz'"
}

script "clean" {
    "rm -rf .caustic build"
    "rm -f src/main.cst.s src/main.cst.s.o"
    "rm -f caustic-assembler/main.cst.s caustic-assembler/main.cst.s.o"
    "rm -f caustic-linker/main.cst.s caustic-linker/main.cst.s.o"
    "rm -f caustic-maker/main.cst.s caustic-maker/main.cst.s.o"
}
```

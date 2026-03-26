# caustic-mk Commands

`caustic-mk` is the Caustic build tool. It reads a `Causticfile` in the current directory and executes build, run, test, clean, or init operations.

Source files: `caustic-maker/main.cst`, `caustic-maker/executor.cst`

## Usage

```
caustic-mk <command> [args]
```

## Commands

### build

```
caustic-mk build <target> [--continue]
```

Compiles a target defined in the `Causticfile`. This is the primary command for building projects.

**Build process:**

1. **Resolve target dependencies.** If the target has a `depends` field naming another target, that target is built first (recursively). If a dependency build fails:
   - Without `--continue`: the entire build stops immediately
   - With `--continue`: the error is noted but the remaining targets are still attempted

2. **Resolve external dependencies.** If the target has `depend` entries, each is checked:
   - If `.caustic/deps/<name>/` does not exist, `git clone --depth 1` fetches it (with `--branch <tag>` if a tag is specified)
   - If a tag file (`.caustic-tag`) exists and the tag has changed, the dependency is removed and re-cloned
   - System libraries declared in dependency Causticfiles are collected for linking

3. **Compile.** Runs the full Caustic pipeline in a single command:
   ```
   caustic <src> -o <out>.tmp --cache .caustic [flags] [--path <dep-dirs>...] [-l<system-libs>...]
   ```
   The `--cache .caustic` flag enables incremental compilation caching. Each dependency directory is passed via `--path`. System libraries are passed as `-l` flags.

4. **Atomic rename.** The output is first written to `<out>.tmp`, then renamed to the final path using `rename(2)`. This prevents overwriting a running binary with a partially-written file. The old binary is deleted first via `unlink(2)`.

If the target has no `out` field, the output defaults to `build/<target-name>` and a warning is printed.

**Tool resolution:** `caustic-mk` looks for the `caustic` compiler binary first in the current directory (`./caustic`), then falls back to PATH resolution via `/bin/sh`.

**Example:**
```
$ caustic-mk build caustic
building target: caustic
  > ./caustic src/main.cst -o caustic.tmp --cache .caustic
  built: caustic
```

### run

```
caustic-mk run <name>
```

Runs a named script or target.

**Resolution order:**

1. **Scripts first.** Searches the `Causticfile` for a `script` block with the given name. If found, executes all commands in order. Each command is run via `/bin/sh -c` with a minimal PATH environment. If any command returns a non-zero exit code, the script stops immediately.

2. **Targets second.** If no matching script is found, searches for a `target` with the given name. If found, runs the target's output binary directly. If the target has an `out` field, that path is used; otherwise `build/<name>` is assumed.

3. **Error.** If neither a script nor a target matches, prints an error.

**Example:**
```
$ caustic-mk run install
  > mkdir -p /usr/local/bin
  > cp caustic /usr/local/bin/caustic
  ...
```

### test

```
caustic-mk test
```

Shorthand for `caustic-mk run test`. Looks for a script named `"test"` in the `Causticfile` and executes it. If no test script is defined, prints an error.

### clean

```
caustic-mk clean
```

Removes the `.caustic/` cache directory and the `build/` output directory. Uses recursive directory removal via `getdents64` and `unlink`/`rmdir` syscalls (no dependency on external tools like `rm`).

This command does **not** require a `Causticfile` to be present.

### init

```
caustic-mk init
```

Creates a new `Causticfile` in the current directory. Prompts for a project name on stdin, then generates a minimal Causticfile with:

```
name "<input>"
version "0.1.0"
author ""

target "<input>" {
    src "src/main.cst"
}
```

This command does **not** require an existing `Causticfile`.

## Exit codes

All commands return 0 on success and non-zero on failure. Build failures propagate the exit code from the compiler. Script commands propagate the exit code from the last failed shell command (extracted via `WEXITSTATUS`: `(status >> 8) & 0xFF`).

## Environment

Commands are executed via `fork(2)` + `execve(2)` of `/bin/sh -c "<command>"` with a minimal environment containing only:

```
PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin
```

The parent process's full environment is not inherited. This ensures reproducible builds but means commands that rely on custom environment variables (e.g., `$HOME`) will not work unless explicitly set in the command string.

## File system layout

```
project/
  Causticfile          # project manifest
  .caustic/            # build cache (created by build)
    deps/              # cloned external dependencies
      <dep-name>/
        .caustic-tag   # pinned tag version
  build/               # default output directory
```

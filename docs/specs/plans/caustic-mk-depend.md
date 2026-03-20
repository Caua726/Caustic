# caustic-mk depend Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add `depend "name" in "url" tag "version"` and `system "lib"` to caustic-mk for automatic dependency resolution.

**Architecture:** Extend Causticfile parser to recognize `depend` and `system` fields. Extend executor to clone repos, read their Causticfiles, inherit system libs, and pass `--path` flags to the compiler. Add `--path` flag to the compiler's module resolver.

**Tech Stack:** Caustic self-hosted compiler + build tool (.cst), git, x86_64 Linux

**Spec:** `docs/specs/caustic-mk-depend.md`

**Depends on:** Module system plan (must be implemented first)

---

## File Structure

| File | Responsibility | Action |
|------|---------------|--------|
| `caustic-maker/parser.cst` | Causticfile parsing | Modify: new Dep struct, parse depend/system |
| `caustic-maker/executor.cst` | Build execution | Modify: clone deps, collect system libs, pass --path |
| `src/main.cst` | Compiler entry point | Modify: add --path flag |

---

### Task 1: Add --path flag to compiler

**Files:**
- Modify: `src/main.cst`

- [ ] **Step 1:** In `parse_args()` in `src/main.cst`, add handling for `--path`:

```cst
} else if (str_eq(arg, arglen, "--path", 6) == 1) {
    if (i + 1 < argc) {
        i = i + 1;
        let is *u8 as path_arg = cast(*u8, *cast(*i64, cast(i64, argv) + cast(i64, i) * 8));
        // Add to search path list (store in global array)
        if (g_extra_path_count < 16) {
            g_extra_paths[g_extra_path_count] = cast(i64, path_arg);
            g_extra_path_count = g_extra_path_count + 1;
        }
    }
```

- [ ] **Step 2:** Add globals for extra paths:

```cst
let is [16]i64 as g_extra_paths with mut;
let is i32 as g_extra_path_count with mut = 0;
```

- [ ] **Step 3:** In the module resolution code (`src/semantic/scope.cst`, `resolve_module_path` or similar), add the extra paths as fallback after stdlib resolution. For each `--path` entry, try `path/filename.cst`.

- [ ] **Step 4:** Test:

```bash
mkdir -p /tmp/test_path_lib
echo 'fn helper_add(a as i32, b as i32) as i32 { return a + b; }' > /tmp/test_path_lib/helper.cst
echo 'use "helper.cst" as h; fn main() as i32 { return h.helper_add(3, 4); }' > /tmp/test_path.cst
./caustic --path /tmp/test_path_lib /tmp/test_path.cst && ./caustic-as /tmp/test_path.cst.s && ./caustic-ld /tmp/test_path.cst.s.o -o /tmp/tp && /tmp/tp; echo "Exit: $?"
# Expected: Exit: 7
```

- [ ] **Step 5:** Bootstrap gen4 and commit:

```bash
rm -rf .caustic
# bootstrap gen1-gen4...
git add src/main.cst src/semantic/scope.cst
git commit -m "compiler: add --path flag for extra module search paths"
```

---

### Task 2: Extend Causticfile parser for depend and system

**Files:**
- Modify: `caustic-maker/parser.cst`

- [ ] **Step 1:** Replace `Dep` struct:

```cst
struct Dep {
    name_ptr as *u8;
    name_len as i32;
    url_ptr as *u8;
    url_len as i32;
    tag_ptr as *u8;
    tag_len as i32;
    next as *u8;
}
```

- [ ] **Step 2:** Add system lib tracking to the project struct (or add global arrays):

```cst
let is [16]i64 as system_lib_ptrs with mut;
let is [16]i32 as system_lib_lens with mut;
let is i32 as system_lib_count with mut = 0;
```

- [ ] **Step 3:** Parse `system "libname"` at top level:

```cst
// In the main parse loop, when token is "system":
if (streq(tok, "system")) {
    advance();
    // expect string literal
    system_lib_ptrs[system_lib_count] = cast(i64, cur_string_ptr);
    system_lib_lens[system_lib_count] = cur_string_len;
    system_lib_count = system_lib_count + 1;
}
```

- [ ] **Step 4:** Parse `depend "name" in "url" [tag "version"]` inside target blocks:

```cst
// In target parse loop, when token is "depend":
if (streq(tok, "depend")) {
    advance();
    let is *Dep as dep = alloc_dep();
    dep.name_ptr = cur_string_ptr; dep.name_len = cur_string_len;
    advance();
    expect("in");
    dep.url_ptr = cur_string_ptr; dep.url_len = cur_string_len;
    advance();
    if (cur_is("tag")) {
        advance();
        dep.tag_ptr = cur_string_ptr; dep.tag_len = cur_string_len;
        advance();
    }
    // Add to target's dep list
}
```

- [ ] **Step 5:** Build caustic-mk and test parsing:

```bash
./caustic caustic-maker/main.cst && ./caustic-as caustic-maker/main.cst.s && ./caustic-ld caustic-maker/main.cst.s.o -o caustic-mk
```

- [ ] **Step 6:** Commit:

```bash
git add caustic-maker/parser.cst
git commit -m "caustic-mk: parse depend/system fields in Causticfile"
```

---

### Task 3: Implement dependency resolution in executor

**Files:**
- Modify: `caustic-maker/executor.cst`

- [ ] **Step 1:** Add `resolve_deps()` function that processes each `depend` entry:

```cst
fn resolve_deps(target as *Target) as void {
    let is *Dep as dep with mut = target.deps;
    while (cast(i64, dep) != 0) {
        // Check if .caustic/deps/<name>/ exists
        let is *u8 as dep_dir = build_dep_path(dep.name_ptr, dep.name_len);
        if (dir_exists(dep_dir) == 0) {
            // Clone
            let is *u8 as cmd with mut = build_clone_cmd(dep);
            run_cmd(cmd);
        }
        // Read dep's Causticfile for system libs
        let is *u8 as dep_cf = build_path(dep_dir, "Causticfile");
        parse_dep_causticfile(dep_cf);
        dep = cast(*Dep, dep.next);
    }
}
```

- [ ] **Step 2:** Build the git clone command:

```cst
fn build_clone_cmd(dep as *Dep) as *u8 {
    // "git clone --depth 1 [--branch tag] url .caustic/deps/name"
    // or just "git clone --depth 1 url .caustic/deps/name"
}
```

- [ ] **Step 3:** When building the compiler command, append `--path` flags for each dependency:

```cst
// Instead of just "caustic src.cst"
// Build: "caustic --path .caustic/deps/sdl3 --path .caustic/deps/other src.cst"
```

- [ ] **Step 4:** When building the linker command, append `-l` flags for each inherited system lib:

```cst
// "caustic-ld src.cst.s.o -lSDL3 -o output"
```

- [ ] **Step 5:** Test end-to-end with a mock dependency (local git repo):

```bash
# Create a test lib repo
mkdir -p /tmp/test-caustic-lib && cd /tmp/test-caustic-lib
git init
echo 'name "testlib"' > Causticfile
echo 'system "c"' >> Causticfile
echo 'fn test_add(a as i32, b as i32) as i32 { return a + b; }' > testlib.cst
git add -A && git commit -m "init"

# Create a test project
mkdir -p /tmp/test-caustic-proj && cd /tmp/test-caustic-proj
cat > Causticfile << 'EOF'
name "test"
target "test" {
    src "main.cst"
    out "test"
    depend "testlib" in "/tmp/test-caustic-lib"
}
EOF
echo 'use "testlib.cst" as tl; fn main() as i32 { return tl.test_add(3, 4); }' > main.cst

# Run caustic-mk
caustic-mk test
./test; echo "Exit: $?"
# Expected: Exit: 7
```

- [ ] **Step 6:** Commit:

```bash
git add caustic-maker/executor.cst
git commit -m "caustic-mk: dependency resolution with git clone and --path"
```

---

### Task 4: Error handling and edge cases

- [ ] **Step 1:** Add error messages:
- `git` not found: `"Error: git is required for dependencies"`
- Clone fails: `"Error: failed to clone <url>"`
- Dep Causticfile not found: `"Error: dependency '<name>' has no Causticfile"`
- Depth limit: `"Error: dependency depth limit exceeded (max 8)"`

- [ ] **Step 2:** Test error cases and commit:

```bash
git add caustic-maker/executor.cst
git commit -m "caustic-mk: dependency error handling"
```

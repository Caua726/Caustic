# Documentation Overhaul Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Fix all documentation errors, add missing content, consolidate guide, rewrite README — bringing docs from v0.0.1-era to v0.0.13 reality.

**Architecture:** Bottom-up approach in 6 phases: fix code-breaking errors (F1), fix wrong info (F2), add missing docs (F3), rewrite README (F5), update CLAUDE.md/index (F6), consolidate guide (F4). Each phase produces correct, committable documentation.

**Spec:** `docs/superpowers/specs/2026-03-25-docs-overhaul-design.md`

**Key source files to reference:**
- `src/parser/ast.cst` — 53 node kinds (NK_NUM=0..NK_CALL_INDIRECT=52), 19 type kinds (TY_VOID=0..TY_FN=18)
- `src/ir/defs.cst` — 48 IR opcodes (IR_IMM=0..IR_CALL_INDIRECT=47)
- `src/codegen/alloc_core.cst` — 10 allocatable registers, linear scan + graph coloring
- `src/main.cst` — 16 CLI flags
- `src/parser/stmt.cst` — match syntax: `match TypeName (expr) { case Variant { ... } else { ... } }`
- `std/*.cst` — 15 stdlib modules + 5 mem submodules

---

## Phase 1: Fix P0 — Code-breaking errors

### Task 1: Fix match syntax in enums guide

**Files:**
- Modify: `docs/guide/enums/simple-enums.md`
- Modify: `docs/guide/enums/tagged-unions.md`

- [ ] **Step 1: Read current files and the correct syntax from source**

Read `src/parser/stmt.cst` lines 130-200 to confirm match syntax. The correct syntax is:
```cst
match Color (c) {
    case Red { return 1; }
    case Green { return 2; }
    case Blue { return 3; }
}
```
NOT `Color.Red => { ... }`. The type name goes after `match`, expression in parens, `case VariantName`, and optional `else { ... }` default.

- [ ] **Step 2: Rewrite simple-enums.md match section**

Replace all match examples in `docs/guide/enums/simple-enums.md`. Every instance of `EnumName.Variant => { ... }` becomes `case Variant { ... }`. Add the type name after `match`. Add `else` clause example.

Correct example:
```cst
match Color (c) {
    case Red { return 1; }
    case Green { return 2; }
    case Blue { return 3; }
}
```

- [ ] **Step 3: Rewrite tagged-unions.md match/destructuring section**

Replace all match examples in `docs/guide/enums/tagged-unions.md`. Fix destructuring syntax too.

Correct example:
```cst
match Shape (s) {
    case Circle(r) { return r * r; }
    case Rect(w, h) { return w * h; }
    else { return 0; }
}
```

- [ ] **Step 4: Verify no other files have wrong match syntax**

Run: `grep -r "=>" docs/guide/enums/` — should return no results after fixes.
Run: `grep -rn "match.*{" docs/guide/ | head -20` — verify all match examples use `match TypeName (expr)` format.

- [ ] **Step 5: Commit**

```bash
git add docs/guide/enums/simple-enums.md docs/guide/enums/tagged-unions.md
git commit -m "docs: fix match syntax in enums guide (case, not =>)"
```

### Task 2: Fix function-pointers.md — document call()

**Files:**
- Modify: `docs/guide/advanced/function-pointers.md`

- [ ] **Step 1: Read current file and source for call() syntax**

Read `src/parser/expr.cst` lines 135-165 for `TK_CALL` handling. Read `src/semantic/walk.cst` lines 314-351 for NK_CALL_INDIRECT type checking. Read `std/sort.cst` lines 99-115 for real-world usage examples.

The `call()` syntax: `call(fn_ptr_expr, arg1, arg2, ...)` — creates NK_CALL_INDIRECT. If the fn_ptr has type TY_FN, args are type-checked. Otherwise returns i64.

- [ ] **Step 2: Remove the false claim and rewrite the file**

Remove the line "Caustic has **no syntax to call through function pointers**" and all surrounding text about this limitation.

Add new sections:

**Creating function pointers:**
```cst
let is *u8 as cb = fn_ptr(my_func);           // local function
let is *u8 as cb = fn_ptr(mod.func);           // module function
let is *u8 as cb = fn_ptr(max gen i32);        // generic function
let is *u8 as cb = fn_ptr(Point_sum);          // impl method
```

**Calling through function pointers:**
```cst
let is i64 as result = call(cb, arg1, arg2);
```

**Typed function pointers (TY_FN):**
```cst
// fn_ptr() returns a typed function pointer with parameter/return type info
// The compiler checks argument count and types at compile time
fn compare(a as i64, b as i64) as i32 {
    if (a < b) { return cast(i32, 0 - 1); }
    if (a > b) { return 1; }
    return 0;
}

fn sort(arr as *i64, count as i64, cmp as *u8) as void {
    // call() with typed fn_ptr — args checked at compile time
    if (call(cmp, arr[i], arr[j]) > 0) {
        // swap
    }
}

fn main() as i32 {
    sort(&arr, 10, fn_ptr(compare));
    return 0;
}
```

- [ ] **Step 3: Commit**

```bash
git add docs/guide/advanced/function-pointers.md
git commit -m "docs: document call() indirect function calls, remove false claim"
```

---

## Phase 2: Fix P1 — Wrong information

### Task 3: Update IR opcode count and list

**Files:**
- Modify: `docs/reference/ir/opcodes.md`
- Modify: `docs/reference/overview.md`

- [ ] **Step 1: Read source for exact opcodes**

Read `src/ir/defs.cst` lines 3-51. Confirm 48 opcodes: IR_IMM(0) through IR_CALL_INDIRECT(47). The two missing from docs are IR_VA_START(46) and IR_CALL_INDIRECT(47).

- [ ] **Step 2: Update opcodes.md**

Change count from 46 to 48. Add two entries at the end of the opcode table:

```
| 46 | IR_VA_START      | Initialize variadic argument list |
| 47 | IR_CALL_INDIRECT | Indirect function call via pointer |
```

Update the introductory text: "48 opcodes numbered 0 through 47".

- [ ] **Step 3: Update overview.md opcode reference**

Find the line that says "46 IR opcodes (IR_IMM through IR_FNEG)" and change to "48 IR opcodes (IR_IMM through IR_CALL_INDIRECT)".

- [ ] **Step 4: Commit**

```bash
git add docs/reference/ir/opcodes.md docs/reference/overview.md
git commit -m "docs: update IR opcode count 46 -> 48 (add VA_START, CALL_INDIRECT)"
```

### Task 4: Update node kind count

**Files:**
- Modify: `docs/reference/parser/node-kinds.md`
- Modify: `docs/reference/overview.md`

- [ ] **Step 1: Read source for exact node kinds**

Read `src/parser/ast.cst` lines 4-56. Confirm 53 node kinds: NK_NUM(0) through NK_CALL_INDIRECT(52). Missing from docs: NK_TYPE_ALIAS(51) and NK_CALL_INDIRECT(52).

- [ ] **Step 2: Update node-kinds.md**

Change count from 51 to 53. Add two entries:

```
| 51 | NK_TYPE_ALIAS    | Type alias declaration (`type Name = Type;`) |
| 52 | NK_CALL_INDIRECT | Indirect function call (`call(fn_ptr, args)`) |
```

Update intro: "53 node kinds, numbered from NK_NUM=0 through NK_CALL_INDIRECT=52".

- [ ] **Step 3: Update overview.md node kind reference**

Change the node kind count from 50/51 to 53. Update the range text.

- [ ] **Step 4: Update overview.md type kind count**

Change from 18 to 19. Add TY_FN(18) to the type kind list. Update the range: "19 type kinds: TY_VOID(0) through TY_FN(18)".

- [ ] **Step 5: Commit**

```bash
git add docs/reference/parser/node-kinds.md docs/reference/overview.md
git commit -m "docs: update node kinds 51->53, type kinds 18->19"
```

### Task 5: Rewrite register allocation docs

**Files:**
- Modify: `docs/reference/codegen/available-registers.md`
- Modify: `docs/reference/codegen/register-allocation.md`

- [ ] **Step 1: Read source for register allocation details**

Read `src/codegen/alloc_core.cst`:
- Lines 285-333 (`find_free_reg`): 10 registers in `allowed[]` array
- Lines 388-486 (`linear_scan_alloc`): MOV coalescing, merge sort, active set tracking
- Lines 193-235 (`merge_sort`): merge sort for intervals (NOT insertion sort)
- Lines 253-273 (`spans_call`): binary search for call positions

Read `src/codegen/alloc_gc.cst` for graph coloring (Iterative Register Coalescing).

- [ ] **Step 2: Rewrite available-registers.md**

Replace content with accurate register table:

**Callee-saved (available across calls):**
| Index | Register | Slot |
|-------|----------|------|
| 3 | rbx | 0 |
| 10 | r12 | 1 |
| 11 | r13 | 2 |
| 12 | r14 | 3 |
| 15 | r15 | 9 |

**Caller-saved (only between calls):**
| Index | Register | Slot |
|-------|----------|------|
| 4 | rsi | 7 |
| 5 | rdi | 8 |
| 6 | r8 | 4 |
| 7 | r9 | 5 |
| 8 | r10 | 6 |

**Reserved (never allocated):**
| Register | Purpose |
|----------|---------|
| rax | Scratch, return value, division |
| rcx | Shift count, 4th arg |
| rdx | Division remainder, 3rd arg |
| rsp | Stack pointer |
| rbp | Frame pointer |

Explain: caller-saved registers only assigned to intervals that do not span any call/SET_ARG instruction (checked via binary search in `spans_call()`).

- [ ] **Step 3: Rewrite register-allocation.md**

Two sections:

**-O0: Linear Scan**
- Build live intervals (merge sort by start, NOT insertion sort)
- MOV coalescing hints (prefer same register for MOV-connected vregs)
- Active set tracking (10-slot array, O(1) expire)
- Spill by lowest cost (cost = uses * 100 / span), tie-break by longest remaining range
- Binary search `spans_call()` for caller-saved exclusion

**-O1: Graph Coloring (Iterative Register Coalescing)**
- George & Appel 1996 algorithm
- Bit matrix interference graph
- George coalescing criterion
- Optimistic Briggs coloring
- Loop-depth-weighted spill costs

Update AllocCtx struct to include all current fields:
```
intervals, count, vreg_to_loc, stack_slots, next_spill, is_leaf,
arg_spill_base, aligned_stack_size, func, va_save_offset,
call_positions, call_count, active_idx, used_callee_mask
```

- [ ] **Step 4: Commit**

```bash
git add docs/reference/codegen/available-registers.md docs/reference/codegen/register-allocation.md
git commit -m "docs: rewrite regalloc docs (10 regs, linear scan + graph coloring)"
```

### Task 6: Rewrite compiler-flags.md

**Files:**
- Modify: `docs/guide/compiler-flags.md`

- [ ] **Step 1: Read all flags from source**

Read `src/main.cst` lines 221-284 (`parse_args`). All 16 flags:

| Flag | Description |
|------|-------------|
| `-c` | Compile only, no main required (for libraries) |
| `-o <path>` | Output path (triggers full pipeline: compile + assemble + link) |
| `-O0` | No optimization (default). Linear scan register allocator. |
| `-O1` | Full optimization: SSA, const prop, strength reduction, LICM, DCE, inlining, graph coloring regalloc, peephole |
| `-l<name>` | Link system library (e.g., `-lc` for libc). Passed to linker. |
| `--max-ram <MB>` | Limit heap memory (0 = unlimited) |
| `--profile` | Show per-phase timing breakdown |
| `--cache <dir>` | Cache directory for tokens/AST/IR (avoids re-parsing unchanged imports) |
| `--path <dir>` | Additional search path for imports (can use multiple times, max 16) |
| `--emit-interface` | Generate .csti interface file (implies -c) |
| `--emit-tokens` | Serialize token cache to .tokens file |
| `--emit-ast` | Serialize AST cache to .ast file |
| `--emit-ir` | Serialize IR cache to .ir file |
| `-debuglexer` | Debug tokenization output |
| `-debugparser` | Debug AST output |
| `-debugir` | Print IR instructions (before optimization at -O0, before+after at -O1) |

- [ ] **Step 2: Rewrite the file completely**

Replace entire content with all 16 flags organized in sections:

**Compilation:**
`-c`, `-o`, `-O0`, `-O1`, `-l<name>`

**Caching:**
`--cache`, `--emit-interface`, `--emit-tokens`, `--emit-ast`, `--emit-ir`

**Runtime:**
`--max-ram`, `--path`, `--profile`

**Debug:**
`-debuglexer`, `-debugparser`, `-debugir`

Include usage examples:
```bash
# Compile and run (full pipeline)
./caustic program.cst -o program && ./program

# Compile library (no main)
./caustic -c lib.cst -o lib.o

# Optimized build
./caustic -O1 program.cst -o program

# Profile compilation
./caustic --profile program.cst -o program

# With cache (fast rebuilds)
./caustic --cache .caustic program.cst -o program

# Dynamic linking
./caustic program.cst -o program -lc
```

- [ ] **Step 3: Commit**

```bash
git add docs/guide/compiler-flags.md
git commit -m "docs: rewrite compiler-flags with all 16 flags"
```

### Task 7: Update lexer and type-system reference docs

**Files:**
- Modify: `docs/reference/lexer/token-types.md`
- Modify: `docs/reference/lexer/keywords.md`
- Modify: `docs/reference/type-system/` (add TY_FN)

- [ ] **Step 1: Update token count**

Read `src/lexer/tokens.cst` for exact count. Update `token-types.md` intro text to reflect 80 token kinds (TK_EOF=0 through TK_CALL=79).

- [ ] **Step 2: Add missing keywords**

Check `keywords.md` for `only`, `call`, `type`. Add any that are missing with their token IDs.

- [ ] **Step 3: Add TY_FN documentation**

In the type-system directory, document the function type:

TY_FN (18): Function type for typed function pointers. Created by `fn_ptr()`. Fields:
- `base`: return type (*Type)
- `fields`: parameter types (packed *Type array)
- `field_count`: number of parameters
- `size`: always 8 (pointer size)

- [ ] **Step 4: Commit**

```bash
git add docs/reference/lexer/token-types.md docs/reference/lexer/keywords.md docs/reference/type-system/
git commit -m "docs: update token count to 80, add TY_FN, add missing keywords"
```

---

## Phase 3: Add missing documentation

### Task 8: Create stdlib reference docs (math, sort, map, random)

**Files:**
- Create: `docs/reference/stdlib/math.md`
- Create: `docs/reference/stdlib/sort.md`
- Create: `docs/reference/stdlib/map.md`
- Create: `docs/reference/stdlib/random.md`

- [ ] **Step 1: Write math.md**

Read `std/math.cst` (136 lines). Document all functions:

```markdown
# std/math.cst

Integer math utilities.

## Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| abs_i64 | `(n as i64) as i64` | Absolute value (INT64_MIN returns INT64_MAX) |
| abs_i32 | `(n as i32) as i32` | Absolute value |
| min_i64 | `(a as i64, b as i64) as i64` | Minimum of two values |
| max_i64 | `(a as i64, b as i64) as i64` | Maximum of two values |
| min_i32 | `(a as i32, b as i32) as i32` | Minimum |
| max_i32 | `(a as i32, b as i32) as i32` | Maximum |
| clamp_i64 | `(val as i64, lo as i64, hi as i64) as i64` | Clamp to range |
| clamp_i32 | `(val as i32, lo as i32, hi as i32) as i32` | Clamp to range |
| pow_i64 | `(base as i64, exp as i64) as i64` | Fast exponentiation (binary) |
| gcd | `(a as i64, b as i64) as i64` | Greatest common divisor |
| lcm | `(a as i64, b as i64) as i64` | Least common multiple |
| sign_i64 | `(n as i64) as i64` | Returns -1, 0, or 1 |
| sign_i32 | `(n as i32) as i32` | Returns -1, 0, or 1 |
| div_ceil | `(a as i64, b as i64) as i64` | Ceiling division |
| is_power_of_two | `(n as i64) as i64` | Returns 1 if power of 2 |
| align_up | `(val as i64, align as i64) as i64` | Align up (align must be power of 2) |
| align_down | `(val as i64, align as i64) as i64` | Align down |
```

- [ ] **Step 2: Write sort.md**

Read `std/sort.cst` (625 lines). Document algorithms and API:

5 algorithms (insertion, bubble, quicksort, heapsort, mergesort), each with i32/i64 and asc/desc variants. Custom comparator via fn_ptr. Generic `sort_i64(arr, count, cmp)` / `sort_i32(arr, count, cmp)`.

Include comparator example:
```cst
use "std/sort.cst" as sort;

fn my_cmp(a as i64, b as i64) as i32 {
    if (a < b) { return cast(i32, 0 - 1); }
    if (a > b) { return 1; }
    return 0;
}

sort.sort_i64(&arr, count, fn_ptr(my_cmp));
```

- [ ] **Step 3: Write map.md**

Read `std/map.cst` (482 lines). Document MapI64 and MapStr:

MapI64: `mapi64_new`, `mapi64_set`, `mapi64_get`, `mapi64_has`, `mapi64_del`, `mapi64_count`, `mapi64_clear`, `mapi64_free`, iterator (`mapi64_next`, `mapi64_key_at`, `mapi64_val_at`).

MapStr: same API with `mapstr_` prefix. Keys are `*u8` (not copied, caller must ensure lifetime).

Include iterator example:
```cst
let is i64 as pos with mut = 0;
while (map.mapi64_next(&m, &pos) == 1) {
    let is i64 as key = map.mapi64_key_at(&m, pos - 1);
    let is i64 as val = map.mapi64_val_at(&m, pos - 1);
}
```

- [ ] **Step 4: Write random.md**

Read `std/random.cst` (212 lines). Document Rng struct and all functions:

Rng struct (xoshiro256**), `rng_init` (seeded via getrandom syscall), `rng_seed`, `rng_next`, `rng_next_u8`, `rng_range` (rejection sampling, no modulo bias), `rng_bool`, `rng_shuffle_i64/i32`, `rng_fill_i64/i32`.

Global convenience: `seed`, `seed_with`, `next_i64`, `next_u8`, `range`, `range_max`, `next_bool`, `shuffle_i64/i32`, `fill_i64/i32`, `fill_bytes`.

- [ ] **Step 5: Commit**

```bash
git add docs/reference/stdlib/math.md docs/reference/stdlib/sort.md docs/reference/stdlib/map.md docs/reference/stdlib/random.md
git commit -m "docs: add stdlib reference for math, sort, map, random"
```

### Task 9: Create stdlib reference docs (net, time, env, arena)

**Files:**
- Create: `docs/reference/stdlib/net.md`
- Create: `docs/reference/stdlib/time.md`
- Create: `docs/reference/stdlib/env.md`
- Create: `docs/reference/stdlib/arena.md`

- [ ] **Step 1: Write net.md**

Read `std/net.cst` (425 lines). Document sections:

Constants (SOCKADDR_IN_SIZE, INADDR_ANY, INADDR_LOOPBACK, MSG_DONTWAIT, DEFAULT_BACKLOG).

Addr struct and functions: `ip4(a,b,c,d)`, `addr_init`, `addr_any`, `addr_loopback`, `addr_port`, `addr_ip`, `ip4_format`, `ip4_parse`.

TCP: `tcp_socket`, `tcp_listen`, `tcp_accept`, `tcp_connect`, `send`, `recv`, `send_all`, `recv_all`, `send_str`, `send_string`, `recv_string`, `close`.

UDP: `udp_socket`, `udp_bind`, `udp_send`, `udp_recv`, `udp_send_str`.

Poll: PollFd struct, `pollfd_init`, `pollfd_ready`, `net_poll`, `wait_readable`, `wait_writable`.

Socket options: `set_reuseaddr`, `set_reuseport`, `set_nodelay`, `set_keepalive`, `set_recv_timeout`, `set_send_timeout`, `set_nonblock`.

Include TCP server example:
```cst
use "std/net.cst" as net;
let is [16]u8 as buf;
let is net.Addr as addr = net.addr_any(&buf, 8080);
let is i64 as server = net.tcp_listen(addr);
let is i64 as client = net.tcp_accept(server, cast(*u8, 0));
net.send_str(client, "Hello\n");
net.close(client);
net.close(server);
```

- [ ] **Step 2: Write time.md**

Read `std/time.cst` (113 lines). Document all functions:

Current time: `now_ns`, `now_us`, `now_ms`, `now_s` (monotonic), `unix_time`, `unix_time_ms` (wall clock).

Sleep: `sleep_s`, `sleep_ms`, `sleep_us`, `sleep_ns`.

Elapsed: `elapsed_ns`, `elapsed_us`, `elapsed_ms`, `elapsed_s` (all take start_ns from `now_ns()`).

- [ ] **Step 3: Write env.md**

Read `std/env.cst` (138 lines). Document:

Setup: `init(argc, argv)` — must call at start of main.

Args: `argc()`, `argv(index)` (raw *u8), `arg(index)` (String copy), `program()` (argv[0]).

Environment: `getenv(name)` (*u8, returns null if not found), `getenv_string(name)` (String copy), `env_count()`, `env_entry(index)`.

Include setup example:
```cst
use "std/env.cst" as env;
use "std/mem.cst" as mem;

fn main(argc as i64, argv as *u8) as i32 {
    mem.gheapinit(65536);
    env.init(argc, argv);
    let is *u8 as home = env.getenv("HOME");
    return 0;
}
```

- [ ] **Step 4: Write arena.md**

Read `std/arena.cst` (130 lines). Document:

Arena struct, `create(size)`, `destroy`, `reset`, `alloc(n)` (8-byte aligned), `alloc_str(s)`, `alloc_zero(n)`, `save`/`restore` (checkpoint/rollback), `used`, `remaining`, `capacity`.

Note: `std/arena.cst` and `std/mem/arena.cst` are identical — both paths work. Import via `use "std/arena.cst" as arena;` or `use "std/mem.cst" as mem;` then `mem.arena.create(...)`.

- [ ] **Step 5: Commit**

```bash
git add docs/reference/stdlib/net.md docs/reference/stdlib/time.md docs/reference/stdlib/env.md docs/reference/stdlib/arena.md
git commit -m "docs: add stdlib reference for net, time, env, arena"
```

### Task 10: Create optimization reference doc

**Files:**
- Create: `docs/reference/codegen/optimizations.md`

- [ ] **Step 1: Read optimization source files**

Read `src/ir/opt.cst` for pipeline. Read `src/ir/opt_prop.cst`, `src/ir/opt_sr.cst`, `src/ir/opt_loop.cst`, `src/ir/opt_helpers.cst`, `src/ir/ssa.cst`, `src/ir/cfg.cst`, `src/ir/dom.cst`, `src/codegen/peephole.cst`.

- [ ] **Step 2: Write optimizations.md**

Document the -O1 pipeline (10 passes in order):

1. **addr_fusion** — Simplifies address expressions, converts stack slot references to direct IMM offsets for LOAD/STORE
2. **mem2reg (SSA)** — Promotes stack variables to virtual registers using SSA form. Builds CFG, computes dominators and dominance frontiers, inserts phi nodes, renames vregs. Only promotes slots with pure LOAD/STORE (no address-taken via IR_ADDR).
3. **const_copy_prop** — Propagates known constants and copies. Single-def guard prevents stale constants in loops after phi destruction.
4. **store_load_fwd** — Eliminates redundant loads from stack slots within basic blocks. Invalidates at branches (JMP/JZ/JNZ).
5. **fold_imm** — Folds known constant values into instruction operands with single-def safety guard.
6. **LICM** — Loop-invariant code motion for call-free loops. Multi-def guard prevents hoisting phi vregs.
7. **strength_reduction** — Replaces MUL by loop-invariant with pointer increments. Detects array indexing chains (base + iv*stride*8). Processes loops bottom-to-top.
8. **DCE** — Dead code elimination. Multi-def guard preserves phi back-edge copies.
9. **inlining** — Inlines small (<20 IR instructions) non-recursive, non-variadic functions.
10. **peephole** — Post-emission assembly optimization: MOV+movsxd fusion, MOV chain bypass, dead MOV elimination.

Also document the register cache (16-entry, tracks which physical register holds which spilled vreg, targeted invalidation).

- [ ] **Step 3: Commit**

```bash
git add docs/reference/codegen/optimizations.md
git commit -m "docs: add optimization reference (-O1 pipeline, 10 passes)"
```

### Task 11: Create build system reference docs

**Files:**
- Create: `docs/reference/build-system/causticfile.md`
- Create: `docs/reference/build-system/commands.md`

- [ ] **Step 1: Read caustic-mk source**

Read `caustic-maker/parser.cst` for Causticfile grammar. Read `caustic-maker/executor.cst` for command execution. Read the project's `Causticfile` for a real example.

- [ ] **Step 2: Write causticfile.md**

Document the Causticfile format:

```
name "string"
version "string"
author "string"
license "string"

target "name" {
    src "path/to/source.cst"
    out "path/to/binary"
    flags "compiler-flags"
    depends "other-target"
    depend "depname" in "git-url" tag "version"
}

system "libname"

script "name" {
    "shell command 1"
    "shell command 2"
}
```

Limits: max 32 targets, 16 scripts, 32 script commands. Comments: `//`.

- [ ] **Step 3: Write commands.md**

Document each command:

| Command | Usage | Description |
|---------|-------|-------------|
| build | `caustic-mk build <target> [--continue]` | Compile target, resolve dependencies first |
| run | `caustic-mk run <name>` | Run script or built target |
| test | `caustic-mk test` | Shorthand for `run "test"` |
| clean | `caustic-mk clean` | Remove `.caustic/` and `build/` |
| init | `caustic-mk init` | Interactive Causticfile creation |

Dependency resolution: `depend` entries are git-cloned to `.caustic/deps/<name>`. Tag pinning supported. System libs from dependency Causticfiles are collected transitively.

Build pipeline: `caustic <src> -o <tmp> --cache .caustic [--path .caustic/deps/*] [-l libs]` then atomic rename.

- [ ] **Step 4: Commit**

```bash
git add docs/reference/build-system/causticfile.md docs/reference/build-system/commands.md
git commit -m "docs: add caustic-mk reference (Causticfile format, commands)"
```

### Task 12: Update mem.md with submodules and delete stdlib-roadmap

**Files:**
- Modify: `docs/reference/stdlib/mem.md`
- Delete: `docs/stdlib-roadmap.md`

- [ ] **Step 1: Update mem.md**

Read `std/mem.cst` (78 lines — just a hub), then each submodule. Add a "Submodules" section:

| Submodule | File | Alloc | Free | Use Case |
|-----------|------|-------|------|----------|
| freelist | `mem/freelist.cst` | O(n) | O(n) | General purpose, address-ordered coalescing, 16-byte header |
| bins | `mem/bins.cst` | O(1) | O(1) | Fixed-size objects, slab-based, bitmap double-free detection, 8-byte header |
| arena | `mem/arena.cst` | O(1) | bulk | Bump allocator, no individual free, reset/destroy to reclaim |
| pool | `mem/pool.cst` | O(1) | O(1) | Fixed-size slots, optional bitmap debug mode |
| lifo | `mem/lifo.cst` | O(1) | O(1) LIFO | Stack-like, free must be in reverse order, 8-byte header |

Note: `std/arena.cst` and `std/mem/arena.cst` are the same implementation — both import paths work.

- [ ] **Step 2: Delete stdlib-roadmap.md**

All items on the roadmap are implemented. Delete the file.

- [ ] **Step 3: Commit**

```bash
git add docs/reference/stdlib/mem.md
git rm docs/stdlib-roadmap.md
git commit -m "docs: update mem.md with submodules, delete outdated stdlib-roadmap"
```

---

## Phase 5: Rewrite README.md

### Task 13: Rewrite README.md completely

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Read current README and gather data**

Read current `README.md`. Read v0.0.13 release notes for benchmark numbers. Read `Causticfile` for correct build commands. Read `std/` for complete module list.

v0.0.13 benchmark numbers (-O1):
| Test | Caustic -O1 | GCC -O0 | LuaJIT |
|------|-------------|---------|--------|
| fib(38) | 175ms | — | — |
| sieve(10M) | 61ms | — | — |
| sort(1M) | 261ms | — | — |
| matmul(64) x100 | 70ms | — | — |
| collatz(1M) | 185ms | — | — |
| **total** | **757ms** | — | — |

Use the v0.0.10 comparison table (most complete cross-compiler data):
| Compiler | Total |
|----------|-------|
| GCC -O2 | 242ms |
| GCC -O0 | 709ms |
| **Caustic -O1** | **768ms** |
| LuaJIT | 1081ms |

- [ ] **Step 2: Write the new README**

Structure:
1. Title + one-liner
2. Overview (3-4 bullets + hello world)
3. Features (grouped bullets: Language, Standard Library, Toolchain, Optimization)
4. Performance (compact comparison table)
5. Quick Start (install + caustic-mk + manual pipeline)
6. Standard Library (table with all 15 modules)
7. Build from Source (bootstrap caustic-mk)
8. Documentation (link to docs/)
9. License

Hello world example:
```cst
fn main() as i32 {
    syscall(1, 1, "Hello, World!\n", 14);
    return 0;
}
```

Build:
```bash
./caustic-mk build hello
./build/hello
```

Keep it balanced — impactful but not wall of text. No checkboxes. Real benchmark numbers from releases.

- [ ] **Step 3: Verify all links work**

Check that `docs/README.md` exists and the link `[docs/](docs/README.md)` is valid.

- [ ] **Step 4: Commit**

```bash
git add README.md
git commit -m "docs: rewrite README (features, benchmarks, caustic-mk, stdlib)"
```

---

## Phase 6: Update CLAUDE.md and docs/README.md

### Task 14: Update CLAUDE.md

**Files:**
- Modify: `CLAUDE.md`

- [ ] **Step 1: Update Build Commands section**

Replace `make` references with `caustic-mk`:

```bash
./caustic-mk build caustic       # Build the compiler
./caustic-mk build caustic-as    # Build the assembler
./caustic-mk build caustic-ld    # Build the linker
./caustic-mk build caustic-mk    # Build the build system itself
./caustic-mk test                # Run test suite
./caustic-mk clean               # Remove .caustic/ and build/
```

Keep the manual pipeline as alternative under "Compiler usage":
```bash
./caustic program.cst -o program && ./program
```

- [ ] **Step 2: Update Architecture table**

Update pipeline table counts: "80+ token types", "53 node kinds", "48 IR operations".

- [ ] **Step 3: Update Standard Library section**

Replace the 5-module list with all 15+ modules:
- `linux.cst` — Syscall wrappers
- `mem.cst` — Memory management hub (freelist, bins, arena, pool, lifo)
- `string.cst` — Dynamic strings
- `io.cst` — Buffered I/O, printf, file/directory operations
- `slice.cst` — Generic dynamic array
- `types.cst` — Option and Result enums
- `map.cst` — Hash maps (MapI64, MapStr)
- `math.cst` — Integer math (abs, min, max, pow, gcd, lcm, align)
- `sort.cst` — Sorting algorithms with fn_ptr comparators
- `random.cst` — xoshiro256** PRNG
- `net.cst` — TCP/UDP networking
- `time.cst` — Clock, sleep, elapsed measurement
- `env.cst` — Command-line args and environment variables
- `arena.cst` — Bump allocator
- `compatcffi.cst` — C FFI compatibility

- [ ] **Step 4: Update Language Syntax section**

Add `call()` example:
```cst
// Indirect function calls
let is *u8 as cmp = fn_ptr(my_compare);
let is i32 as result = call(cmp, a, b);
```

Add `only` imports:
```cst
use "std/mem.cst" only freelist, bins as mem;
```

- [ ] **Step 5: Commit**

```bash
git add CLAUDE.md
git commit -m "docs: update CLAUDE.md (caustic-mk, stdlib, flags, syntax)"
```

### Task 15: Update docs/README.md index

**Files:**
- Modify: `docs/README.md`

- [ ] **Step 1: Read current docs/README.md**

Read the file and note all section links.

- [ ] **Step 2: Restructure for consolidated guide**

Replace the ~80 granular guide links with ~15 consolidated links (these files don't exist yet — they'll be created in Phase 4, but the TOC should point to them):

```markdown
## Guide
- [Getting Started](guide/getting-started.md)
- [Variables](guide/variables.md)
- [Types](guide/types.md)
- [Functions](guide/functions.md)
- [Control Flow](guide/control-flow.md)
- [Operators](guide/operators.md)
- [Structs](guide/structs.md)
- [Enums](guide/enums.md)
- [Generics](guide/generics.md)
- [Memory](guide/memory.md)
- [Modules](guide/modules.md)
- [Impl Blocks](guide/impl.md)
- [Advanced](guide/advanced.md)
- [Compiler Flags](guide/compiler-flags.md)
- [Build System](guide/build-system.md)
```

- [ ] **Step 3: Add missing reference sections**

Add under Reference:
- Build System: `reference/build-system/causticfile.md`, `reference/build-system/commands.md`
- Codegen: add `reference/codegen/optimizations.md`
- Stdlib: add math, sort, map, random, net, time, env, arena

- [ ] **Step 4: Commit**

```bash
git add docs/README.md
git commit -m "docs: restructure docs/README.md TOC for consolidated guide"
```

---

## Phase 4: Consolidate guide

### Task 16: Consolidate guide — variables, types, functions

**Files:**
- Create: `docs/guide/variables.md` (merge from `variables/declaration.md`, `mutability.md`, `globals.md`, `shadowing.md`, `initialization.md`)
- Create: `docs/guide/types.md` (merge from `types/integers.md`, `unsigned.md`, `floats.md`, `bool.md`, `char.md`, `void.md`, `string-type.md`, `type-coercion.md`, `negative-literals.md` + ADD hex/binary/octal/underscores)
- Create: `docs/guide/functions.md` (merge from `functions/declaration.md`, `parameters.md`, `return-values.md`, `recursion.md`, `variadic.md`)

- [ ] **Step 1: Read all source files for variables/**

Read all 5 files in `docs/guide/variables/`. Concatenate content into sections within a single file. Remove redundant headers and transitions.

- [ ] **Step 2: Create variables.md**

Sections: Declaration, Mutability (`with mut` / `with imut`), Globals (data section), Shadowing, Initialization. Use `##` for each section.

- [ ] **Step 3: Read and create types.md**

Read all 9 files in `docs/guide/types/`. Merge into sections: Integers (i8-i64), Unsigned (u8-u64), Floats (f32, f64), Bool, Char, Void, String, Type Coercion, Negative Literals.

**ADD NEW:** Section on number literal formats:
```cst
let is i64 as dec = 1_000_000;    // decimal with underscores
let is i64 as hex = 0xFF_FF;      // hexadecimal
let is i64 as bin = 0b1010_1010;  // binary
let is i64 as oct = 0o755;        // octal
```

- [ ] **Step 4: Read and create functions.md**

Read all 5 files in `docs/guide/functions/`. Merge into sections: Declaration, Parameters, Return Values, Recursion, Variadic Functions.

- [ ] **Step 5: Commit**

```bash
git add docs/guide/variables.md docs/guide/types.md docs/guide/functions.md
git commit -m "docs: consolidate guide — variables, types, functions"
```

### Task 17: Consolidate guide — control-flow, operators, structs

**Files:**
- Create: `docs/guide/control-flow.md` (merge 7 files)
- Create: `docs/guide/operators.md` (merge 7 files)
- Create: `docs/guide/structs.md` (merge 6 files)

- [ ] **Step 1: Read and create control-flow.md**

Merge: if-else, while, for, do-while, break-continue, match, destructuring. Ensure match section uses correct syntax (`case Variant`, type name, `else` clause).

- [ ] **Step 2: Read and create operators.md**

Merge: arithmetic, comparison, logical, bitwise, compound-assignment, short-circuit, operator-precedence. Keep precedence table as final section.

- [ ] **Step 3: Read and create structs.md**

Merge: declaration, field-access, packed-layout, nested-structs, struct-pointers, struct-return.

- [ ] **Step 4: Commit**

```bash
git add docs/guide/control-flow.md docs/guide/operators.md docs/guide/structs.md
git commit -m "docs: consolidate guide — control-flow, operators, structs"
```

### Task 18: Consolidate guide — enums, generics, memory

**Files:**
- Create: `docs/guide/enums.md` (REWRITE + merge 4 files)
- Create: `docs/guide/generics.md` (merge 6 files)
- Create: `docs/guide/memory.md` (merge 8 files)

- [ ] **Step 1: Create enums.md (REWRITE)**

This is a rewrite, not just a merge. Use correct match syntax throughout. Include:
- Simple enums (`enum Color { Red; Green; Blue; }`)
- Tagged unions (`enum Shape { Circle as i32; Rect as i32, i32; None; }`)
- Generic enums (`enum Option gen T { Some as T; None; }`)
- Construction (`Color.Blue`, `Shape.Rect(10, 20)`, `Option gen i32 .Some(42)`)
- Pattern matching with correct syntax (`match TypeName (expr) { case Variant { ... } else { ... } }`)
- Destructuring (`case Rect(w, h) { ... }`)
- Enum layout (discriminant + max payload)

- [ ] **Step 2: Read and create generics.md**

Merge: syntax, functions, structs, enums, monomorphization, constraints.

- [ ] **Step 3: Read and create memory.md**

Merge: pointers, address-of, dereferencing, pointer-arithmetic, arrays, array-of-structs, heap-allocation, stack-vs-heap.

- [ ] **Step 4: Commit**

```bash
git add docs/guide/enums.md docs/guide/generics.md docs/guide/memory.md
git commit -m "docs: consolidate guide — enums (rewritten), generics, memory"
```

### Task 19: Consolidate guide — modules, impl, advanced

**Files:**
- Create: `docs/guide/modules.md` (merge 6 files + ADD only/submodules)
- Create: `docs/guide/impl.md` (merge 5 files)
- Create: `docs/guide/advanced.md` (merge 12 files + ADD call()/typed fn_ptr)

- [ ] **Step 1: Create modules.md**

Merge: use-statement, aliases, relative-paths, circular-imports, multi-file-projects, extern-fn.

**ADD NEW sections:**

`only` imports:
```cst
use "std/mem.cst" only freelist, bins as mem;
```

Submodule access:
```cst
use "std/mem.cst" as mem;
mem.bins.bins_new(4096);       // submodule function
mem.arena.create(65536);       // another submodule
```

- [ ] **Step 2: Create impl.md**

Merge: methods, associated-functions, desugaring, generic-impl, method-call-syntax.

- [ ] **Step 3: Create advanced.md**

Merge: cast, sizeof, inline-asm, syscalls, defer, defer-patterns, function-pointers, ffi, ffi-struct-passing, compatcffi, string-escapes, char-literals.

The function-pointers section must include `call()` and typed fn_ptr (already rewritten in Task 2 — carry that content forward).

**ADD NEW:** Block comments section:
```cst
/* This is a block comment */
/* Block comments
   can be nested /* like this */ */
```

- [ ] **Step 4: Commit**

```bash
git add docs/guide/modules.md docs/guide/impl.md docs/guide/advanced.md
git commit -m "docs: consolidate guide — modules (+only/submodules), impl, advanced (+call)"
```

### Task 20: Create getting-started.md and build-system.md, then delete old files

**Files:**
- Create: `docs/guide/getting-started.md`
- Create: `docs/guide/build-system.md`
- Delete: all files in `docs/guide/variables/`, `types/`, `functions/`, `control-flow/`, `operators/`, `structs/`, `enums/`, `generics/`, `memory/`, `modules/`, `impl/`, `advanced/`

- [ ] **Step 1: Create getting-started.md**

Quick start guide:
1. Install (tar + cp)
2. Hello world (write code, compile, run)
3. Using caustic-mk (Causticfile, build, run)
4. Manual pipeline (caustic → caustic-as → caustic-ld)
5. Next steps (links to guide sections)

- [ ] **Step 2: Create build-system.md**

User-facing caustic-mk guide (not reference — that's in reference/build-system/):
- What is caustic-mk
- Creating a Causticfile
- Building targets
- Running scripts
- Managing dependencies
- System libraries

- [ ] **Step 3: Delete all old granular guide files**

```bash
git rm -r docs/guide/variables/ docs/guide/types/ docs/guide/functions/ docs/guide/control-flow/ docs/guide/operators/ docs/guide/structs/ docs/guide/enums/ docs/guide/generics/ docs/guide/memory/ docs/guide/modules/ docs/guide/impl/ docs/guide/advanced/
```

- [ ] **Step 4: Verify new structure**

```bash
ls docs/guide/
```

Expected output:
```
getting-started.md  variables.md  types.md  functions.md  control-flow.md
operators.md  structs.md  enums.md  generics.md  memory.md  modules.md
impl.md  advanced.md  compiler-flags.md  build-system.md
```

15 files total.

- [ ] **Step 5: Commit**

```bash
git add docs/guide/getting-started.md docs/guide/build-system.md
git add -A docs/guide/
git commit -m "docs: consolidate guide (80 files -> 15), add getting-started and build-system"
```

---

## Final Verification

### Task 21: Final check and cleanup

- [ ] **Step 1: Verify no broken links in docs/README.md**

Read `docs/README.md` and check every link points to a file that exists:
```bash
grep -oP '\(([^)]+\.md)\)' docs/README.md | tr -d '()' | while read f; do test -f "docs/$f" || echo "BROKEN: $f"; done
```

- [ ] **Step 2: Verify no remaining wrong match syntax**

```bash
grep -rn "=>" docs/guide/ | grep -v "^Binary"
```

Should return nothing (no `=>` in any guide file).

- [ ] **Step 3: Verify opcode/node/type counts are consistent**

```bash
grep -rn "46 opcodes\|46 IR" docs/
grep -rn "51 node\|50 node" docs/
grep -rn "18 type" docs/reference/
```

All should return empty (old counts removed).

- [ ] **Step 4: Verify no references to `make`**

```bash
grep -rn "^make \|make assembler\|make linker\|make test-linker\|make clean" docs/ CLAUDE.md README.md
```

Should return nothing.

- [ ] **Step 5: Final commit if any fixes needed**

```bash
git add -A docs/ README.md CLAUDE.md
git commit -m "docs: final cleanup and verification"
```

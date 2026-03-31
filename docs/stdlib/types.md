## _module
types — Option and Result

Instead of returning magic values (-1, null) to indicate "no value" or
"error", use Option and Result for explicit, type-safe error handling.

Option gen T — a value that might not exist:
  let is types.Option gen i32 as maybe = types.Some gen i32 (42);
  if (maybe.has == 1) {
      // use maybe.val — it's an i32
  }
  let is types.Option gen *u8 as none = types.None gen *u8 ();

Result gen T, E — success or failure:
  let is types.Result gen i32, *u8 as r = do_something();
  if (r.ok == 1) {
      // use r.val (success value)
  } else {
      io.write_str(linux.STDERR, r.err);  // error message
  }

Option fields: { has as i32, val as T }
Result fields: { ok as i32, val as T, err as E }
---
## Option
struct Option gen T { has as i32; val as T; }

Optional value — either contains a value or is empty.

Fields:
  has - 1 if value is present, 0 if empty
  val - the contained value (only valid when has == 1)

Create with types.Some(val) or types.None gen T ().

Example:
  let is types.Option gen i32 as maybe = types.Some gen i32 (42);
  if (maybe.has == 1) {
      io.write_int(linux.STDOUT, cast(i64, maybe.val));
  }
---
## Some
fn Some gen T (val as T) as Option gen T

Create an Option containing a value.

Example:
  let is types.Option gen i32 as x = types.Some gen i32 (42);
---
## None
fn None gen T () as Option gen T

Create an empty Option (no value).

Example:
  let is types.Option gen *u8 as x = types.None gen *u8 ();
---
## Result
struct Result gen T, E { ok as i32; val as T; err as E; }

Result type for operations that can fail.

Fields:
  ok  - 1 if success, 0 if error
  val - success value (valid when ok == 1)
  err - error value (valid when ok == 0)

Example:
  let is types.Result gen i32, *u8 as r = do_something();
  if (r.ok == 1) {
      // use r.val
  } else {
      io.write_str(linux.STDERR, r.err);
  }
---
## Ok
fn Ok gen T, E (val as T) as Result gen T, E

Create a success Result.
---
## Err
fn Err gen T, E (err as E) as Result gen T, E

Create an error Result.
---

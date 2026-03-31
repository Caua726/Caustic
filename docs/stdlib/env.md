## _module
std/env.cst — Environment

Access command-line arguments and environment variables.

  argc()      — number of arguments
  argv(idx)   — get argument by index
  getenv(name)— get environment variable

Usage:
  use "std/env.cst" as env;
  let is *u8 as file = env.argv(1);
---
## argc
fn argc() as i64

Get the number of command-line arguments (including the program name).

Example:
  if (env.argc() < 2) {
      io.write_str(linux.STDERR, "usage: prog <file>\n");
      return 1;
  }
---
## argv
fn argv(idx as i64) as *u8

Get a command-line argument by index.

Parameters:
  idx - 0-based index (0 = program name, 1 = first argument)

Returns: null-terminated argument string.

Example:
  let is *u8 as filename = env.argv(1);
---
## getenv
fn getenv(name as *u8) as *u8

Get an environment variable value.

Parameters:
  name - variable name (e.g. "HOME", "PATH")

Returns: value string, or null if not set.

Example:
  let is *u8 as home = env.getenv("HOME");
  if (cast(i64, home) != 0) {
      io.write_str(linux.STDOUT, home);
  }
---

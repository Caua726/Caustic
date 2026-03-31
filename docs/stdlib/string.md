## _module
std/string.cst — Dynamic strings

Heap-allocated, growable strings with length tracking.

Key functions:
  String(text)          — create from C string
  from_parts(ptr, len)  — create from pointer + length
  concat(a, b)          — concatenate two strings
  strlen(s)             — length of C string
  streq(a, b)           — compare strings
  find(s, needle)       — search substring
  substring(s, a, b)    — extract range
  split(s, delim, ...)  — split by delimiter
  trim(s)               — remove whitespace
  replace(s, old, new)  — replace all occurrences
  int_to_string(n)      — integer to string

Usage:
  use "std/string.cst" as string;
  let is string.String as s = string.String("hello");
---
## String
struct String { ptr as *u8; len as i32; cap as i32; }

Dynamic string with automatic growth.

A heap-allocated, length-tracked string. Not null-terminated internally,
but ptr is always null-terminated for C compatibility.

Fields:
  ptr - pointer to character data (null-terminated)
  len - current length in bytes
  cap - allocated capacity

Create with string.String("text") or string.from_parts(ptr, len).
---
## String
fn String(text as *u8) as String

Create a new String from a null-terminated C string.

The input is copied to a heap-allocated buffer.

Example:
  let is string.String as s = string.String("hello world");
  io.write_str(linux.STDOUT, s.ptr);
---
## from_parts
fn from_parts(ptr as *u8, len as i32) as String

Create a String from a pointer and length (copies the data).

Useful when you have a non-null-terminated buffer.

Example:
  let is string.String as s = string.from_parts(buf_ptr, buf_len);
---
## concat
fn concat(a as String, b as String) as String

Concatenate two strings, returning a new String.

Example:
  let is string.String as greeting = string.concat(
      string.String("hello "),
      string.String("world")
  );
---
## strlen
fn strlen(s as *u8) as i32

Length of a null-terminated C string (not counting the null byte).

Example:
  let is i32 as n = string.strlen("hello");  // 5
---
## streq
fn streq(a as *u8, b as *u8) as i32

Compare two null-terminated strings for equality.

Returns: 1 if equal, 0 if different.
---
## streq_n
fn streq_n(a as *u8, alen as i32, b as *u8, blen as i32) as i32

Compare two length-delimited strings for equality.

Returns: 1 if equal, 0 if different.
---
## int_to_string
fn int_to_string(n as i64) as String

Convert an integer to its decimal string representation.

Example:
  let is string.String as s = string.int_to_string(42);  // "42"
---
## find
fn find(haystack as String, needle as *u8) as i32

Find the first occurrence of needle in haystack.

Returns: byte index of the match, or -1 if not found.

Example:
  let is i32 as pos = string.find(s, "world");
---
## substring
fn substring(s as String, start as i32, end as i32) as String

Extract a substring from index start (inclusive) to end (exclusive).

Example:
  let is string.String as sub = string.substring(s, 0, 5);
---
## split
fn split(s as String, delim as u8, out_parts as *i64, out_lens as *i32, max_parts as i32) as i32

Split a string by a delimiter character.

Parameters:
  s         - string to split
  delim     - delimiter byte (e.g. 44 for comma)
  out_parts - array of *u8 pointers to each part
  out_lens  - array of i32 lengths for each part
  max_parts - maximum number of parts

Returns: number of parts found.
---
## trim
fn trim(s as String) as String

Remove leading and trailing whitespace.
---
## starts_with
fn starts_with(s as String, prefix as *u8) as i32

Check if string starts with prefix. Returns 1 or 0.
---
## ends_with
fn ends_with(s as String, suffix as *u8) as i32

Check if string ends with suffix. Returns 1 or 0.
---
## replace
fn replace(s as String, old as *u8, new as *u8) as String

Replace all occurrences of 'old' with 'new' in the string.

Returns: new String with replacements made.
---
## to_upper
fn to_upper(s as String) as String

Convert all ASCII lowercase letters to uppercase.
---
## to_lower
fn to_lower(s as String) as String

Convert all ASCII uppercase letters to lowercase.
---
## char_at
fn char_at(s as String, idx as i32) as u8

Get the byte at a given index.

Returns: byte value, or 0 if index is out of bounds.
---

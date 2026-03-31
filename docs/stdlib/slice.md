## Slice
struct Slice gen T { data as *T; len as i32; cap as i32; }

Generic dynamic array (growable vector).

Automatically resizes when full. Elements are stored contiguously.
Type parameter T determines element type and size.

Example:
  let is slice.Slice gen i32 as nums = slice.new gen i32 ();
  slice.push gen i32 (&nums, 10);
  slice.push gen i32 (&nums, 20);
  let is i32 as val = slice.get gen i32 (&nums, 0);  // 10
---
## new
fn new gen T () as Slice gen T

Create a new empty Slice with default capacity (8 elements).

Example:
  let is slice.Slice gen i32 as s = slice.new gen i32 ();
---
## push
fn push gen T (s as *Slice gen T, val as T) as void

Append an element to the end. Grows automatically if needed (doubles capacity).

Parameters:
  s   - pointer to Slice
  val - element to append
---
## get
fn get gen T (s as *Slice gen T, idx as i32) as T

Get element at index. No bounds checking.

Parameters:
  s   - pointer to Slice
  idx - 0-based index
---
## set
fn set gen T (s as *Slice gen T, idx as i32, val as T) as void

Set element at index. No bounds checking.
---
## pop
fn pop gen T (s as *Slice gen T) as T

Remove and return the last element. Undefined behavior if empty.
---
## len
fn len gen T (s as *Slice gen T) as i32

Get the number of elements in the slice.
---
## clear
fn clear gen T (s as *Slice gen T) as void

Remove all elements (sets len to 0). Does not free memory.
---

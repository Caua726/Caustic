## _module
std/sort.cst — Sorting algorithms

Three sorting algorithms with function pointer comparators:
  quicksort  — average O(n log n), in-place
  heapsort   — guaranteed O(n log n), in-place
  mergesort  — O(n log n), stable, uses extra memory

Usage:
  use "std/sort.cst" as sort;
  sort.quicksort(&arr, n, fn_ptr(my_cmp));
---
## quicksort
fn quicksort(arr as *i64, n as i32, cmp as *u8) as void

Sort an array using quicksort algorithm.

Parameters:
  arr - pointer to i64 array
  n   - number of elements
  cmp - comparator function pointer: fn(a as i64, b as i64) as i32
        returns negative if a < b, 0 if equal, positive if a > b

Average O(n log n), worst case O(n^2).

Example:
  fn cmp_asc(a as i64, b as i64) as i32 { return cast(i32, a - b); }
  sort.quicksort(&arr, 100, fn_ptr(cmp_asc));
---
## heapsort
fn heapsort(arr as *i64, n as i32, cmp as *u8) as void

Sort an array using heapsort. Guaranteed O(n log n) worst case.
Same comparator interface as quicksort.
---
## mergesort
fn mergesort(arr as *i64, n as i32, cmp as *u8) as void

Sort an array using mergesort. Stable sort, O(n log n).
Uses extra memory for the merge buffer.
Same comparator interface as quicksort.
---

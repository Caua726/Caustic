# std/sort.cst

Sorting algorithms for `i64` and `i32` arrays. Five algorithms are provided: insertion sort, bubble sort, quicksort, heapsort, and mergesort. All algorithms accept ascending and descending variants, and support custom comparators via function pointers. Requires `std/mem.cst` for mergesort (heap allocation for the temporary buffer).

## Import

```cst
use "std/sort.cst" as sort;
use "std/mem.cst" as mem;    // required only for mergesort
```

## Comparators

Built-in comparator functions that return `-1`, `0`, or `1`. Used internally by the sorting algorithms and available for custom `sort_i64`/`sort_i32` calls.

| Function | Signature | Description |
|----------|-----------|-------------|
| `cmp_asc_i64` | `fn cmp_asc_i64(a as i64, b as i64) as i32` | Ascending order comparator for i64. |
| `cmp_desc_i64` | `fn cmp_desc_i64(a as i64, b as i64) as i32` | Descending order comparator for i64. |
| `cmp_asc_i32` | `fn cmp_asc_i32(a as i32, b as i32) as i32` | Ascending order comparator for i32. |
| `cmp_desc_i32` | `fn cmp_desc_i32(a as i32, b as i32) as i32` | Descending order comparator for i32. |

## Utility Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `swap_i64` | `fn swap_i64(arr as *i64, a as i64, b as i64) as void` | Swap elements at indices `a` and `b` in an i64 array. |
| `swap_i32` | `fn swap_i32(arr as *i32, a as i64, b as i64) as void` | Swap elements at indices `a` and `b` in an i32 array. |
| `reverse_i64` | `fn reverse_i64(arr as *i64, count as i64) as void` | Reverse an i64 array in place. |
| `reverse_i32` | `fn reverse_i32(arr as *i32, count as i64) as void` | Reverse an i32 array in place. |
| `is_sorted_i64` | `fn is_sorted_i64(arr as *i64, count as i64) as i64` | Returns 1 if the i64 array is sorted ascending, 0 otherwise. |
| `is_sorted_i32` | `fn is_sorted_i32(arr as *i32, count as i64) as i64` | Returns 1 if the i32 array is sorted ascending, 0 otherwise. |

## Sorting Algorithms

Each algorithm has four public variants: ascending i64, descending i64, ascending i32, descending i32.

### 1. Insertion Sort

Stable, O(n^2) average and worst case. Efficient for small or nearly-sorted arrays.

| Function | Signature | Description |
|----------|-----------|-------------|
| `insertion_sort_i64` | `fn insertion_sort_i64(arr as *i64, count as i64) as void` | Sort i64 array ascending. |
| `insertion_sort_desc_i64` | `fn insertion_sort_desc_i64(arr as *i64, count as i64) as void` | Sort i64 array descending. |
| `insertion_sort_i32` | `fn insertion_sort_i32(arr as *i32, count as i64) as void` | Sort i32 array ascending. |
| `insertion_sort_desc_i32` | `fn insertion_sort_desc_i32(arr as *i32, count as i64) as void` | Sort i32 array descending. |

### 2. Bubble Sort

Stable, O(n^2) average and worst case. Includes early-exit optimization when no swaps occur in a pass.

| Function | Signature | Description |
|----------|-----------|-------------|
| `bubble_sort_i64` | `fn bubble_sort_i64(arr as *i64, count as i64) as void` | Sort i64 array ascending. |
| `bubble_sort_desc_i64` | `fn bubble_sort_desc_i64(arr as *i64, count as i64) as void` | Sort i64 array descending. |
| `bubble_sort_i32` | `fn bubble_sort_i32(arr as *i32, count as i64) as void` | Sort i32 array ascending. |
| `bubble_sort_desc_i32` | `fn bubble_sort_desc_i32(arr as *i32, count as i64) as void` | Sort i32 array descending. |

### 3. Quicksort

Unstable, O(n log n) average, O(n^2) worst case. Uses median-of-three pivot selection and falls back to insertion sort for partitions smaller than 16 elements.

| Function | Signature | Description |
|----------|-----------|-------------|
| `quicksort_i64` | `fn quicksort_i64(arr as *i64, count as i64) as void` | Sort i64 array ascending. |
| `quicksort_desc_i64` | `fn quicksort_desc_i64(arr as *i64, count as i64) as void` | Sort i64 array descending. |
| `quicksort_i32` | `fn quicksort_i32(arr as *i32, count as i64) as void` | Sort i32 array ascending. |
| `quicksort_desc_i32` | `fn quicksort_desc_i32(arr as *i32, count as i64) as void` | Sort i32 array descending. |

### 4. Heapsort

Unstable, O(n log n) worst case guaranteed. In-place, no extra memory required.

| Function | Signature | Description |
|----------|-----------|-------------|
| `heapsort_i64` | `fn heapsort_i64(arr as *i64, count as i64) as void` | Sort i64 array ascending. |
| `heapsort_desc_i64` | `fn heapsort_desc_i64(arr as *i64, count as i64) as void` | Sort i64 array descending. |
| `heapsort_i32` | `fn heapsort_i32(arr as *i32, count as i64) as void` | Sort i32 array ascending. |
| `heapsort_desc_i32` | `fn heapsort_desc_i32(arr as *i32, count as i64) as void` | Sort i32 array descending. |

### 5. Mergesort

Stable, O(n log n) worst case guaranteed. Allocates a temporary buffer of the same size as the input array via `mem.galloc`. Requires `mem.gheapinit()` before use.

| Function | Signature | Description |
|----------|-----------|-------------|
| `mergesort_i64` | `fn mergesort_i64(arr as *i64, count as i64) as void` | Sort i64 array ascending. |
| `mergesort_desc_i64` | `fn mergesort_desc_i64(arr as *i64, count as i64) as void` | Sort i64 array descending. |
| `mergesort_i32` | `fn mergesort_i32(arr as *i32, count as i64) as void` | Sort i32 array ascending. |
| `mergesort_desc_i32` | `fn mergesort_desc_i32(arr as *i32, count as i64) as void` | Sort i32 array descending. |

## Generic Sort with Custom Comparator

For full control over sort order, use `sort_i64` or `sort_i32` with a custom comparator function pointer. These use quicksort internally.

| Function | Signature | Description |
|----------|-----------|-------------|
| `sort_i64` | `fn sort_i64(arr as *i64, count as i64, cmp as *u8) as void` | Sort i64 array using a custom comparator (quicksort). |
| `sort_i32` | `fn sort_i32(arr as *i32, count as i64, cmp as *u8) as void` | Sort i32 array using a custom comparator (quicksort). |

The comparator function must have the signature `fn(a as T, b as T) as i32` and return:
- Negative value if `a` should come before `b`
- Zero if `a` and `b` are equal
- Positive value if `a` should come after `b`

## Example

### Basic sorting

```cst
use "std/sort.cst" as sort;
use "std/mem.cst" as mem;

fn main() as i32 {
    mem.gheapinit(1048576);

    let is [5]i64 as arr;
    arr[0] = 50; arr[1] = 10; arr[2] = 40; arr[3] = 20; arr[4] = 30;

    sort.quicksort_i64(&arr, 5);
    // arr is now: 10, 20, 30, 40, 50

    sort.heapsort_desc_i64(&arr, 5);
    // arr is now: 50, 40, 30, 20, 10

    return 0;
}
```

### Custom comparator with fn_ptr

```cst
use "std/sort.cst" as sort;

// Sort by absolute value ascending
fn cmp_by_abs(a as i64, b as i64) as i32 {
    let is i64 as aa = a;
    let is i64 as bb = b;
    if (aa < 0) { aa = 0 - aa; }
    if (bb < 0) { bb = 0 - bb; }
    if (aa < bb) { return cast(i32, 0 - 1); }
    if (aa > bb) { return 1; }
    return 0;
}

fn main() as i32 {
    let is [4]i64 as arr;
    arr[0] = 0 - 5; arr[1] = 3; arr[2] = 0 - 1; arr[3] = 4;

    sort.sort_i64(&arr, 4, fn_ptr(cmp_by_abs));
    // arr is now: -1, 3, 4, -5

    return 0;
}
```

## Notes

- All algorithms are safe to call with `count < 2` (no-op).
- Quicksort uses median-of-three pivot selection to avoid worst-case behavior on sorted input.
- Mergesort is the only algorithm that requires heap allocation (`mem.gheapinit()`). All others are in-place.
- The `_` prefixed functions (e.g., `_insertion_sort_i64`, `_qs_impl_i64`) are internal implementation details and should not be called directly.

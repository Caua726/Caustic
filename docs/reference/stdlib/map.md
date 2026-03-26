# std/map.cst

Hash map implementations for two key types: `MapI64` (i64 keys to i64 values) and `MapStr` (null-terminated string keys to i64 values). Uses open addressing with linear probing and tombstone-based deletion. Automatically grows at 75% load factor. Requires `std/mem.cst` for heap allocation.

## Import

```cst
use "std/map.cst" as map;
use "std/mem.cst" as mem;
```

## Hash Algorithms

- **MapI64**: splitmix64-style hash. XOR-shift and multiply with additive constant to ensure `hash(0) != 0`. The sign bit is masked off to produce a non-negative result.
- **MapStr**: FNV-1a variant (64-bit prime `1099511628211`, offset basis `7793`). Iterates over each byte of the null-terminated string. Sign bit masked off.

## MapI64

### Struct

```cst
struct MapI64 {
    states as *u8;    // per-slot state: 0=EMPTY, 1=OCCUPIED, 2=TOMBSTONE
    keys   as *i64;   // key storage
    values as *i64;   // value storage
    cap    as i64;    // current capacity (always a power of two)
    count  as i64;    // number of occupied entries
}
```

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `mapi64_new` | `fn mapi64_new() as MapI64` | Create a new empty map with initial capacity 16. |
| `mapi64_free` | `fn mapi64_free(m as *MapI64) as void` | Free all internal buffers and reset the map to zero state. |
| `mapi64_set` | `fn mapi64_set(m as *MapI64, key as i64, value as i64) as void` | Insert or update a key-value pair. Triggers automatic growth at 75% load. |
| `mapi64_get` | `fn mapi64_get(m as *MapI64, key as i64, default_val as i64) as i64` | Retrieve the value for `key`. Returns `default_val` if the key is not found. |
| `mapi64_has` | `fn mapi64_has(m as *MapI64, key as i64) as i64` | Returns 1 if `key` exists, 0 otherwise. |
| `mapi64_del` | `fn mapi64_del(m as *MapI64, key as i64) as i64` | Delete `key`. Returns 1 if found and deleted, 0 if not found. Uses tombstone markers. |
| `mapi64_count` | `fn mapi64_count(m as *MapI64) as i64` | Return the number of entries in the map. |
| `mapi64_clear` | `fn mapi64_clear(m as *MapI64) as void` | Remove all entries but keep allocated capacity. |
| `mapi64_next` | `fn mapi64_next(m as *MapI64, pos as *i64) as i64` | Iterator: advance `pos` to the next occupied slot. Returns 1 if found, 0 if iteration is complete. |
| `mapi64_key_at` | `fn mapi64_key_at(m as *MapI64, idx as i64) as i64` | Return the key at raw slot index `idx`. |
| `mapi64_val_at` | `fn mapi64_val_at(m as *MapI64, idx as i64) as i64` | Return the value at raw slot index `idx`. |

## MapStr

### Struct

```cst
struct MapStr {
    states as *u8;    // per-slot state: 0=EMPTY, 1=OCCUPIED, 2=TOMBSTONE
    keys   as *u8;    // key pointer storage (array of *u8 pointers, stored as i64)
    values as *i64;   // value storage
    cap    as i64;    // current capacity (always a power of two)
    count  as i64;    // number of occupied entries
}
```

**Important**: Keys are stored as pointers, **not copied**. The caller must ensure that the string pointed to by each key outlives the map.

### Functions

| Function | Signature | Description |
|----------|-----------|-------------|
| `mapstr_new` | `fn mapstr_new() as MapStr` | Create a new empty string map with initial capacity 16. |
| `mapstr_free` | `fn mapstr_free(m as *MapStr) as void` | Free all internal buffers and reset the map to zero state. Does **not** free the key strings. |
| `mapstr_set` | `fn mapstr_set(m as *MapStr, key as *u8, value as i64) as void` | Insert or update a key-value pair. The key pointer is stored directly (not copied). Triggers automatic growth at 75% load. |
| `mapstr_get` | `fn mapstr_get(m as *MapStr, key as *u8, default_val as i64) as i64` | Retrieve the value for `key`. Returns `default_val` if not found. Comparison is byte-by-byte string equality. |
| `mapstr_has` | `fn mapstr_has(m as *MapStr, key as *u8) as i64` | Returns 1 if `key` exists, 0 otherwise. |
| `mapstr_del` | `fn mapstr_del(m as *MapStr, key as *u8) as i64` | Delete `key`. Returns 1 if found and deleted, 0 if not found. |
| `mapstr_count` | `fn mapstr_count(m as *MapStr) as i64` | Return the number of entries in the map. |
| `mapstr_clear` | `fn mapstr_clear(m as *MapStr) as void` | Remove all entries but keep allocated capacity. |
| `mapstr_next` | `fn mapstr_next(m as *MapStr, pos as *i64) as i64` | Iterator: advance `pos` to the next occupied slot. Returns 1 if found, 0 if iteration is complete. |
| `mapstr_key_at` | `fn mapstr_key_at(m as *MapStr, idx as i64) as *u8` | Return the key pointer at raw slot index `idx`. |
| `mapstr_val_at` | `fn mapstr_val_at(m as *MapStr, idx as i64) as i64` | Return the value at raw slot index `idx`. |

## Iterator Pattern

Both map types support iteration over all entries using a position variable. The `*_next` function scans forward from the current position to find the next occupied slot.

```cst
// Iterate over all entries in a MapI64
let is i64 as pos with mut = 0;
while (map.mapi64_next(&m, &pos) == 1) {
    let is i64 as key = map.mapi64_key_at(&m, pos - 1);
    let is i64 as val = map.mapi64_val_at(&m, pos - 1);
    // use key and val ...
}
```

```cst
// Iterate over all entries in a MapStr
let is i64 as pos with mut = 0;
while (map.mapstr_next(&m, &pos) == 1) {
    let is *u8 as key = map.mapstr_key_at(&m, pos - 1);
    let is i64 as val = map.mapstr_val_at(&m, pos - 1);
    // use key and val ...
}
```

Note: `pos` is set to `slot_index + 1` by `*_next`, so use `pos - 1` when calling `*_key_at` and `*_val_at`.

## Example

```cst
use "std/map.cst" as map;
use "std/mem.cst" as mem;

fn main() as i32 {
    mem.gheapinit(1048576);

    // --- MapI64 ---
    let is map.MapI64 as mi = map.mapi64_new();
    map.mapi64_set(&mi, 42, 100);
    map.mapi64_set(&mi, 7, 200);
    map.mapi64_set(&mi, 42, 300);    // overwrite key 42

    let is i64 as v = map.mapi64_get(&mi, 42, 0);   // 300
    let is i64 as h = map.mapi64_has(&mi, 99);       // 0
    let is i64 as c = map.mapi64_count(&mi);          // 2

    map.mapi64_del(&mi, 7);
    map.mapi64_free(&mi);

    // --- MapStr ---
    let is map.MapStr as ms = map.mapstr_new();
    map.mapstr_set(&ms, "hello", 1);
    map.mapstr_set(&ms, "world", 2);

    let is i64 as sv = map.mapstr_get(&ms, "hello", 0);  // 1

    map.mapstr_free(&ms);

    return 0;
}
```

## Notes

- Requires `mem.gheapinit()` before creating any maps.
- Initial capacity is 16, always a power of two. Doubles on growth.
- Growth is triggered when `count * 4 >= cap * 3` (75% load factor).
- Deletion uses tombstone markers. Tombstones are reused on insert but not reclaimed until a growth/rehash occurs.
- `mapstr_free` does **not** free the key strings themselves. If keys were heap-allocated, the caller must free them separately.
- Allocation failure during growth is handled gracefully: the map retains its old state and the insert is silently dropped.

## _module
map — Key-Value Hash Maps

When you need to associate keys with values. Two variants:

MapI64 — for integer keys (e.g. mapping IDs to pointers):
  let is map.MapI64 as m = map.mapi64_new();
  map.mapi64_set(&m, key, value);
  let is i64 as v = map.mapi64_get(&m, key, default);
  map.mapi64_has(&m, key)    — returns 1 if exists
  map.mapi64_del(&m, key)    — delete entry

MapStr — for string keys (e.g. symbol tables, configs):
  let is map.MapStr as m = map.mapstr_new();
  map.mapstr_set(&m, "name", value);
  let is i64 as v = map.mapstr_get(&m, "name", default);

Both auto-grow when 75% full. Values are i64 — store pointers
via cast(i64, ptr) and retrieve with cast(*MyType, value).

Hashing: MapI64 uses splitmix64, MapStr uses FNV-1a.
Warning: MapStr stores key pointers, NOT copies. Keys must outlive the map.
---
## MapI64
struct MapI64 { ... }

Hash map with i64 keys and i64 values.

Open addressing with linear probing. Uses splitmix64 hash function.
Auto-grows at 75% load factor.

Create with map.mapi64_new(). Store pointers as i64 with cast().

Example:
  let is map.MapI64 as m = map.mapi64_new();
  map.mapi64_set(&m, 42, 100);
  let is i64 as val = map.mapi64_get(&m, 42, 0);  // 100
---
## mapi64_new
fn mapi64_new() as MapI64

Create a new empty i64 hash map with default capacity (64 slots).

Example:
  let is map.MapI64 as m = map.mapi64_new();
---
## mapi64_set
fn mapi64_set(m as *MapI64, key as i64, value as i64) as void

Insert or update a key-value pair. Overwrites existing value if key exists.
Auto-grows the table if load factor exceeds 75%.

Parameters:
  m     - pointer to MapI64
  key   - integer key
  value - integer value (can store pointers via cast(i64, ptr))
---
## mapi64_get
fn mapi64_get(m as *MapI64, key as i64, default_val as i64) as i64

Look up a key in the map.

Parameters:
  m           - pointer to MapI64
  key         - key to look up
  default_val - value to return if key not found

Returns: the stored value, or default_val if key doesn't exist.
---
## mapi64_has
fn mapi64_has(m as *MapI64, key as i64) as i64

Check if a key exists in the map.

Returns: 1 if key exists, 0 if not.
---
## mapi64_del
fn mapi64_del(m as *MapI64, key as i64) as i64

Delete a key from the map (tombstone deletion).

Returns: 1 if key was found and deleted, 0 if not found.
---
## MapStr
struct MapStr { ... }

Hash map with string keys (*u8) and i64 values.

Uses FNV-1a hash function. Keys are stored as pointers (NOT copied).
Make sure key strings outlive the map.

Example:
  let is map.MapStr as m = map.mapstr_new();
  map.mapstr_set(&m, "name", cast(i64, name_ptr));
  let is i64 as val = map.mapstr_get(&m, "name", 0);
---
## mapstr_new
fn mapstr_new() as MapStr

Create a new empty string hash map with default capacity.
---
## mapstr_set
fn mapstr_set(m as *MapStr, key as *u8, value as i64) as void

Insert or update a key-value pair. The key pointer is stored directly
(NOT copied) — the string must remain valid for the lifetime of the map.

Parameters:
  m     - pointer to MapStr
  key   - null-terminated string key
  value - integer value
---
## mapstr_get
fn mapstr_get(m as *MapStr, key as *u8, default_val as i64) as i64

Look up a string key. Returns default_val if not found.
---
## mapstr_has
fn mapstr_has(m as *MapStr, key as *u8) as i64

Check if a string key exists. Returns 1 or 0.
---
## mapstr_del
fn mapstr_del(m as *MapStr, key as *u8) as i64

Delete a string key. Returns 1 if found, 0 if not.
---

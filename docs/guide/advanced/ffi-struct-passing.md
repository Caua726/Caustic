# FFI Struct Passing

When passing structs to or from C functions, Caustic follows the System V AMD64 ABI classification rules.

## Classification Rules

### Small structs (1-8 bytes): Register

Passed in a single GPR (INTEGER class) or XMM register (SSE class):

```cst
struct Small {
    a as i32;
    b as i32;
}
// 8 bytes -> passed in one register (e.g., rdi)

extern fn use_small(s as Small) as void;

fn main() as i32 {
    let is Small as s;
    s.a = 10;
    s.b = 20;
    use_small(s);
    return 0;
}
```

### Medium structs (9-16 bytes): Two Registers

Split into two 8-byte halves, each independently classified:

```cst
struct Medium {
    x as i64;
    y as i64;
}
// 16 bytes -> passed in two registers (rdi, rsi)
```

### Large structs (>16 bytes): Memory

Caller allocates a stack buffer, copies the struct, and passes a pointer:

```cst
struct Large {
    a as i64;
    b as i64;
    c as i64;
}
// 24 bytes -> passed via hidden pointer
```

## Return Values

- 1-8 bytes: returned in `rax` (INTEGER) or `xmm0` (SSE)
- 9-16 bytes: returned in `rax` + `rdx`
- Greater than 16 bytes: caller passes a hidden pointer (SRET), callee writes the result there

## Packed Struct Gotcha

Caustic structs are **packed** (no alignment padding). C structs have alignment padding. This mismatch causes problems:

```cst
// Caustic: 12 bytes (packed)
struct CausticMixed {
    a as i32;   // offset 0, 4 bytes
    b as f64;   // offset 4, 8 bytes
}

// C equivalent: 16 bytes (padded)
// struct CMixed {
//     int a;      // offset 0, 4 bytes
//     // 4 bytes padding
//     double b;   // offset 8, 8 bytes
// };
```

The Caustic struct is 12 bytes. The C struct is 16 bytes. Passing a Caustic `CausticMixed` to a C function expecting `CMixed` will read incorrect field values.

## Workaround: Match C Layout Manually

Add padding fields to match C alignment:

```cst
struct CCompatMixed {
    a as i32;
    _pad as i32;    // 4 bytes padding to match C alignment
    b as f64;
}
// Now 16 bytes, matches C layout
```

## Safe Patterns for C Interop

Use all-`i64` structs to avoid alignment issues:

```cst
struct SafePoint {
    x as i64;
    y as i64;
}
// 16 bytes, same layout in Caustic and C (assuming C uses long/int64_t)
```

Or use pointer-based passing for complex structs:

```cst
extern fn process_data(ptr as *u8, len as i64) as i32;
```

## Classification Edge Case

If a field crosses the 8-byte boundary in a packed struct, the entire struct is classified as MEMORY (passed via pointer), regardless of total size. This is the correct System V ABI behavior for unaligned fields.

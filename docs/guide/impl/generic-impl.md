# Generic Impl Blocks

Impl blocks can be generic, applying methods to a generic struct or enum. The generic parameters from the impl block are copied to each method inside it.

## Declaration

```cst
struct Wrapper gen T {
    value as T;
    count as i32;
}

impl Wrapper gen T {
    fn get(self as *Wrapper gen T) as T {
        return self.value;
    }

    fn set(self as *Wrapper gen T, val as T) as void {
        self.value = val;
        self.count = self.count + 1;
    }

    fn new(val as T) as Wrapper gen T {
        let is Wrapper gen T as w;
        w.value = val;
        w.count = 0;
        return w;
    }
}
```

## Usage

```cst
let is Wrapper gen i32 as w = Wrapper gen i32 .new(42);
let is i32 as v = w.get();
w.set(100);
```

## How It Works

1. The parser sees `impl Wrapper gen T { ... }` and copies the generic parameter `T` to each function inside the block.
2. Each function is desugared to a top-level generic function: `fn Wrapper_get gen T (self as *Wrapper gen T) as T`.
3. When `w.get()` is called on a `Wrapper gen i32`, the compiler instantiates `Wrapper_get gen i32`, producing `Wrapper_i32_get` (or `Wrapper_get_i32` depending on mangling order).
4. Type substitution replaces `Wrapper gen T` with `Wrapper_i32` in the self parameter and all usages within the method body.

## Multiple Type Parameters

```cst
struct Map gen K, V {
    keys as *K;
    vals as *V;
    len as i32;
}

impl Map gen K, V {
    fn get(self as *Map gen K, V, key as K) as V {
        // ...lookup logic...
    }
}
```

## Gotcha: substitute_type and generic_args

When the compiler substitutes types during generic instantiation, it must handle `generic_args` on struct type references. For example, `Wrapper gen T` in the self parameter has a `generic_args` list containing `T`. The substitution must replace each arg and re-mangle the struct name to produce `Wrapper_i32`.

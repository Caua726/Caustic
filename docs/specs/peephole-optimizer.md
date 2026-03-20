# Assembly peephole optimizer (-O1)

Post-pass on the generated assembly that eliminates redundant register-to-register MOVs and folds sign-extension patterns.

Runs after codegen, before writing the .s file. Only active under -O1.

## Patterns to eliminate

### 1. MOV + movsxd fusion
```
mov rA, rB        →  movsxd rA, rBd
movsxd rA, rAd
```

### 2. MOV + operation fusion
```
mov rA, rB        →  op rB, rC
op rA, rC
```
When rA is not used after the op (scratch register).

### 3. Dead MOV elimination
```
mov rA, rB        →  (remove)
mov rA, rC
```
When rA is immediately overwritten.

### 4. Constant cast elimination
```
mov rA, 1         →  mov BYTE PTR [rC], 1
mov rB, rA
movzx rB, rBb
mov rD, rB
mov BYTE PTR [rC], rDl
```

### 5. Redundant movsxd after 32-bit op
```
add eax, ebx      →  add eax, ebx
movsxd rax, eax      (remove — 32-bit write already zero-extends upper bits)
```

## Implementation

Buffer the last N emitted assembly lines (ring buffer of ~4 entries). Before emitting a new line, check if the buffered sequence matches any pattern. If so, replace with the fused version.

Alternatively: emit to a temporary buffer, then scan and optimize before writing to the output file.

## Where

In `src/codegen/emit.cst`, wrapping the `out_line`/`out_cstr` calls. Or as a separate post-pass function called after `codegen()` finishes but before the file is written.

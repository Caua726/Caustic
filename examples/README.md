# Examples

Runnable Caustic programs demonstrating language features and standard library usage.

## Building

Every example compiles with the full toolchain:

```bash
./caustic examples/<name>.cst
./caustic-as examples/<name>.cst.s
./caustic-ld examples/<name>.cst.s.o -o /tmp/<name>
/tmp/<name>
```

Or as a one-liner:

```bash
./caustic examples/hello.cst && ./caustic-as examples/hello.cst.s && ./caustic-ld examples/hello.cst.s.o -o /tmp/hello && /tmp/hello
```

## Index

| Example | Description |
|---------|-------------|
| [hello.cst](hello.cst) | Hello world — syscall and linux.write |
| [fibonacci.cst](fibonacci.cst) | Recursive and iterative fibonacci |
| [strings.cst](strings.cst) | String ops: create, concat, find, compare |
| [structs.cst](structs.cst) | Structs with impl blocks, methods, constructors |
| [files.cst](files.cst) | Read and write files using raw syscalls |
| [calculator.cst](calculator.cst) | Read stdin, parse numbers, do math |
| [linked_list.cst](linked_list.cst) | Heap-allocated linked list with insert/pop/print |
| [enums.cst](enums.cst) | Simple enums, tagged unions, pattern matching |

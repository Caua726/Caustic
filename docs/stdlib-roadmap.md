# Caustic Standard Library Roadmap

## Current (`std/`)

| Module | Description |
|--------|-------------|
| `linux.cst` | Syscall wrappers (read, write, open, mmap, exit, stat, etc.) |
| `mem.cst` | Heap allocator with free-list coalescing |
| `string.cst` | Dynamic strings (String struct with ptr/len/cap) |
| `io.cst` | Buffered I/O, line reading, printf |
| `slice.cst` | Generic dynamic array (Slice gen T) |
| `types.cst` | Type size helpers |
| `compatcffi.cst` | C FFI compatibility helpers |

## Essential — Priority 1

| Module | Description | Why |
|--------|-------------|-----|
| `math.cst` | abs, min, max, clamp, pow (integer). f64: sqrt, sin, cos, tan, atan2, floor, ceil, round | Qualquer programa numérico precisa. Games, gráficos, ciência |
| `hashmap.cst` | HashMap gen K, V — open addressing, probe linear | Estrutura de dados fundamental. Compiladores, ferramentas, apps |
| `fmt.cst` | sprintf/format pra string: format("{} + {} = {}", a, b, c) | Construir strings formatadas sem concatenação manual |
| `sort.cst` | quicksort/mergesort genérico pra Slice gen T com comparator | Ordenação é operação básica em qualquer programa |
| `random.cst` | xorshift64, seed, rand_range, rand_f64, shuffle | RNG sem depender de libc |
| `args.cst` | Parse argc/argv de _start, flag parsing simples (--flag, -f value) | Todo CLI tool precisa |

## Important — Priority 2

| Module | Description | Why |
|--------|-------------|-----|
| `fs.cst` | readfile, writefile, exists, mkdir_p, listdir, path join/dirname/basename | Operações de filesystem de alto nível |
| `net.cst` | socket, bind, listen, accept, connect — TCP client/server | Networking básico via syscalls |
| `time.cst` | clock_gettime wrapper, timestamp, sleep, Duration struct | Benchmarks, timeouts, scheduling |
| `testing.cst` | assert, assert_eq, test runner, pass/fail contagem | Framework de testes pra stdlib e programas |
| `json.cst` | Parse/emit JSON — object, array, string, number, bool, null | Formato de troca de dados universal |
| `env.cst` | getenv, setenv via /proc/self/environ ou stack scanning | Configuração via variáveis de ambiente |

## Nice to Have — Priority 3

| Module | Description | Why |
|--------|-------------|-----|
| `regex.cst` | Regex engine simples (NFA-based) — match, search, replace | Text processing |
| `csv.cst` | CSV parser/writer | Dados tabulares |
| `base64.cst` | Encode/decode base64 | Encoding binário pra texto |
| `sha256.cst` | SHA-256 hash | Checksums, integridade |
| `utf8.cst` | UTF-8 decode/encode, codepoint iteration | Texto Unicode |
| `thread.cst` | clone() wrapper, mutex via futex, channel | Paralelismo |
| `mmap_file.cst` | Memory-mapped file I/O | Processar arquivos grandes |
| `ringbuf.cst` | Ring buffer gen T — lock-free single producer/consumer | IPC, streaming |
| `bitset.cst` | Bitset com operações set/clear/test/popcount | Flags, bloom filters |
| `arena.cst` | Arena allocator — bulk alloc, free all at once | Compiladores, parsers |

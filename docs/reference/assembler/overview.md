# caustic-as Overview

caustic-as is a native x86_64/AArch64 assembler written entirely in Caustic. It translates the compiler's Intel-syntax x86_64 or GNU-like AArch64 assembly (`.s` files) into ELF64 relocatable objects (`.o`), with no dependency on GNU as, NASM, LLVM, or another external assembler in production.

The AArch64 path is selected with `--target=linux-aarch64` and lives in
`caustic-assembler/aarch64/assembler.cst`. It uses fixed-width instruction
encoding and emits AArch64 RELA relocations directly.

## Pipeline

At a high level, both architecture paths use a two-pass pipeline:

```
.s file --> Parse --> Pass 1 (symbols/sizes) --> Pass 2 (encode) --> ELF64 .o
```

The staged tokenizer/`ParsedLine` implementation described below is the x86_64
path. AArch64 performs line-oriented parsing and both passes in
`aarch64/assembler.cst`, then hands the resulting sections, symbols, and
relocations to the same target-aware ELF writer.

1. **Tokenize** (`lexer.cst`): The raw assembly source is scanned into a flat array of tokens -- registers, instructions, immediates, labels, directives, and punctuation.

2. **Parse**: Tokens are structured into `ParsedLine` records. Each line is classified as a label, instruction, directive, or empty. Operands are parsed into a flattened representation (register ID, immediate value, memory base/displacement, or label reference).

3. **Pass 1** (`main.cst:pass1`): Iterates over all parsed lines. Labels are recorded in the symbol table with their section and offset. Instruction sizes are computed by trial-encoding into a temporary buffer. Directive data sizes are calculated (string lengths, `.byte`/`.word`/`.long`/`.quad`/`.zero` sizes). Section offsets (`.text`, `.data`, `.rodata`, `.bss`) are tracked independently.

4. **Pass 2** (`main.cst:pass2`): Iterates again, this time encoding instructions into the `.text` buffer using real symbol offsets from pass 1. Data directives emit bytes into their respective section buffers. Relocations are generated for unresolved or cross-section references (external calls, RIP-relative accesses, GOT references).

5. **ELF Output** (`elf.cst`): The encoded section buffers, symbol table, relocation entries, and string tables are assembled into a valid ELF64 relocatable object file and written to disk.

## Source Files

| File | Purpose |
|------|---------|
| `main.cst` | Entry point, CLI parsing, `ParsedLine` struct, `parse_all()`, `pass1()`, `pass2()`, string decoding, I/O helpers |
| `lexer.cst` | Tokenizer: `Token` and `TokenList` structs, `tokenize()` function, register/instruction keyword recognition |
| `encoder.cst` | x86_64 instruction encoding: register IDs, instruction IDs, operand types, `encode()` function, ModR/M/SIB/REX helpers |
| `aarch64/assembler.cst` | AArch64 line parser, instruction encoder, two-pass symbol handling and relocation generation |
| `output/elf.cst` | Target-aware ELF object generation: symbols, relocations, sections and `e_machine` |

## Two-Pass Assembly

The two-pass design is necessary because forward references (e.g., a `jmp` to a label defined later in the file) cannot be resolved during a single scan. Pass 1 collects all symbol definitions and calculates exact instruction sizes, so that pass 2 can compute correct relative offsets for jumps and calls.

### Pass 1 Details

- Maintains four independent offset counters: `text_off`, `data_off`, `rodata_off`, `bss_off`.
- For each label, records its section and current offset via `sym_find_or_add()`.
- For `.globl` directives, marks the named symbol as global.
- For data directives (`.string`, `.byte`, `.word`, `.long`, `.quad`, `.zero`), calculates the byte size and advances the appropriate section offset.
- For instructions, calls `calc_inst_size()` which trial-encodes into a temporary buffer to determine the exact byte count. This is essential because x86_64 instructions have variable length.

### Pass 2 Details

- Encodes each instruction into the `.text` ByteBuffer using `enc.encode()`.
- Resolves label operands by looking up the symbol table. For `.text`-section labels, the resolved offset is passed directly to the encoder. For cross-section or undefined labels, a relocation entry is emitted instead.
- External function calls (`call`/`jmp` to undefined symbols) generate `R_X86_64_PLT32` relocations.
- RIP-relative memory accesses to undefined symbols generate `R_X86_64_GOTPCREL` relocations (for data) or `R_X86_64_PC32` relocations (for code).
- Data directives emit their bytes into the appropriate section buffer (`.data`, `.rodata`, or `.text`), with string escape sequence decoding for `.string`/`.asciz`.

## Dynamic Heap Sizing

The assembler dynamically sizes its heap allocation based on the input file size:

```
heap_size = file_size * 50 + 8MB
minimum = 16MB
```

This formula accounts for the memory needed by token arrays, parsed line arrays, section buffers, symbol tables, and the ELF output buffer. The multiplier of 50 provides headroom for large files with many symbols and relocations. The minimum of 16MB ensures the allocator has sufficient space even for trivially small inputs.

The heap is initialized via `mem.gheapinit()` using Linux `mmap` syscalls -- there is no libc dependency.

## Usage

```bash
# Assemble a single file
./caustic-as input.s          # produces input.s.o
./caustic-as --target=linux-aarch64 input.s

# Full pipeline from Caustic source to executable
./caustic program.cst         # produces program.cst.s
./caustic-as program.cst.s    # produces program.cst.s.o
./caustic-ld program.cst.s.o -o program
./program
```

## Build

```bash
./caustic-mk build caustic-as
```

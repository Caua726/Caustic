# Caustic self-hosted toolchain
CAUSTIC = ./caustic
CAUSTIC_AS = ./caustic-as
CAUSTIC_LD = ./caustic-ld

# Build the compiler from source
all: $(CAUSTIC)

$(CAUSTIC): caustic-compiler/main.cst
	$(CAUSTIC) caustic-compiler/main.cst
	$(CAUSTIC_AS) caustic-compiler/main.cst.s
	$(CAUSTIC_LD) caustic-compiler/main.cst.s.o -o $(CAUSTIC)

# Assembler
assembler: $(CAUSTIC)
	$(CAUSTIC) caustic-assembler/main.cst
	$(CAUSTIC_AS) caustic-assembler/main.cst.s
	$(CAUSTIC_LD) caustic-assembler/main.cst.s.o -o $(CAUSTIC_AS)

# Linker
linker: $(CAUSTIC)
	$(CAUSTIC) caustic-linker/main.cst
	$(CAUSTIC_AS) caustic-linker/main.cst.s
	$(CAUSTIC_LD) caustic-linker/main.cst.s.o -o $(CAUSTIC_LD)

# Test linker
test-linker: $(CAUSTIC) linker assembler
	bash tests_asm/test_linker.sh

# Clean build artifacts
clean:
	rm -f caustic-compiler/main.cst.s caustic-compiler/main.cst.s.o
	rm -f caustic-assembler/main.cst.s caustic-assembler/main.cst.s.o
	rm -f caustic-linker/main.cst.s caustic-linker/main.cst.s.o

.PHONY: all clean assembler linker test-linker

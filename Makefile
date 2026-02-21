# Caustic self-hosted toolchain
CAUSTIC = ./caustic
CAUSTIC_AS = ./caustic-as
CAUSTIC_LD = ./caustic-ld
CAUSTIC_MK = ./caustic-mk

# Build targets via caustic-mk
all: $(CAUSTIC_MK)
	$(CAUSTIC_MK) build caustic

assembler: $(CAUSTIC_MK)
	$(CAUSTIC_MK) build caustic-as

linker: $(CAUSTIC_MK)
	$(CAUSTIC_MK) build caustic-ld

# Bootstrap: build caustic-mk itself (needs existing toolchain)
maker: $(CAUSTIC)
	$(CAUSTIC) caustic-maker/main.cst
	$(CAUSTIC_AS) caustic-maker/main.cst.s
	$(CAUSTIC_LD) caustic-maker/main.cst.s.o -o $(CAUSTIC_MK)

test:
	$(CAUSTIC_MK) test

clean:
	$(CAUSTIC_MK) clean
	rm -f src/main.cst.s src/main.cst.s.o
	rm -f caustic-assembler/main.cst.s caustic-assembler/main.cst.s.o
	rm -f caustic-linker/main.cst.s caustic-linker/main.cst.s.o
	rm -f caustic-maker/main.cst.s caustic-maker/main.cst.s.o

.PHONY: all clean assembler linker test maker

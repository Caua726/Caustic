# Bootstrap only — use caustic-mk for everything else
# Usage: make bootstrap && ./caustic-mk build <target>

bootstrap:
	./caustic caustic-maker/main.cst
	./caustic-as caustic-maker/main.cst.s
	./caustic-ld caustic-maker/main.cst.s.o -o caustic-mk

.PHONY: bootstrap

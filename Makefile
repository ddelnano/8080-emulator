.PHONY: disassembler

CC = gcc
CFLAGS = -ggdb

EMULATOR_OBJS = emulator.c disassembler.c
DISASSEMBLER_OBJS = disassembler.c test.c

emulator: $(EMULATOR_OBJS)
	gcc $(CFLAGS) $(EMULATOR_OBJS) -o emulator
	./emulator invaders.h invaders.g invaders.f invaders.e

disassembler: $(DISASSEMBLER_OBJS)
	gcc $(DISASSEMBLER_OBJS) -o disassembler
	./disassembler invaders

clean:
	rm -f *.o emulator

test: $(EMULATOR_OBJS)
	gcc -D CPU_TEST $(CFLAGS) $(EMULATOR_OBJS) -o emulator
	./emulator --start 0x100 cpudiag.bin | head -n 1 | grep 'CPU IS OPERATIONAL'

.PHONY: disassembler

CC = gcc

ifdef DEBUG
    CFLAGS = -D DEBUG -ggdb
else
    CFLAGS = -ggdb
endif

EMULATOR_OBJS = emulator.c disassembler.c tests/emulator.c
DISASSEMBLER_OBJS = disassembler.c tests/disassembler.c

emulator: $(EMULATOR_OBJS)
	$(CC) $(CFLAGS) $(EMULATOR_OBJS) -o emulator
	./emulator invaders.h invaders.g invaders.f invaders.e

disassembler: $(DISASSEMBLER_OBJS)
	$(CC) $(DISASSEMBLER_OBJS) -o disassembler
	./disassembler invaders

clean:
	rm -f *.o emulator

test: $(EMULATOR_OBJS)
	$(CC) -D DEBUG -D CPU_TEST $(CFLAGS) $(EMULATOR_OBJS) -o emulator
	./emulator --start 0x100 cpudiag.bin | head -n 612 | grep 'CPU IS OPERATIONAL'

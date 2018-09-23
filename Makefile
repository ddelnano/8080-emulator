.PHONY: disassembler

CC = gcc

ifdef DEBUG
    CFLAGS = -D DEBUG -ggdb
else
    CFLAGS = -ggdb
endif

EMULATOR_OBJS = emulator.c disassembler.c
DISASSEMBLER_OBJS = disassembler.c test.c

emulator: $(EMULATOR_OBJS)
	$(CC) $(CFLAGS) $(EMULATOR_OBJS) -o emulator
	./emulator invaders.h invaders.g invaders.f invaders.e

disassembler: $(DISASSEMBLER_OBJS)
	$(CC) $(DISASSEMBLER_OBJS) -o disassembler
	./disassembler invaders

clean:
	rm -f *.o emulator

test: $(EMULATOR_OBJS)
	$(CC) -D CPU_TEST $(CFLAGS) $(EMULATOR_OBJS) -o emulator
	./emulator --start 0x100 cpudiag.bin | head -n 607 | grep 'CPU IS OPERATIONAL'

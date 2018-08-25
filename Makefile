.PHONY: disassembler

CC = gcc
CFLAGS = -ggdb

EMULATOR_OBJS = emulator.c disassembler.c
DISASSEMBLER_OBJS = disassembler.c test.c

emulator: $(EMULATOR_OBJS)
	gcc $(CFLAGS) $(EMULATOR_OBJS) -o emulator
	./emulator

disassembler: $(DISASSEMBLER_OBJS)
	gcc $(DISASSEMBLER_OBJS) -o disassembler
	./disassembler invaders

clean:
	rm *.o emulator

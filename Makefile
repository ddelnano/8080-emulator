CC = gcc
CFLAGS = -ggdb

%.o: %.c
	gcc $(CFLAGS) $< -o $@

disassemble: disassembler.o
	./disassembler.o invaders

emulator: emulator.o
	./emulator.o

CC = gcc

%.o: %.c
	gcc $< -o $@

run: disassembler.o
	./disassembler.o invaders

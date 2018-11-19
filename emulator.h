#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct ConditionCodes {
    uint8_t zero: 1;
    uint8_t parity: 1;
    uint8_t auxcarry: 1;
    uint8_t carry: 1;
    uint8_t sign: 1;
} ConditionCodes;

typedef struct State8080 {
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint16_t pc;
    uint16_t sp;
    uint8_t *memory;
uint8_t int_enable;
    struct ConditionCodes cc;
} State8080;

void read_rom_into_memory(State8080 *emu, unsigned int start_addr, char *rom_files[], unsigned int rom_count);

int emulate(State8080 *emu);

void generate_interrupt(State8080* emu, int interrupt_num);

void print_last_1000_instructions();

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

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
    struct ConditionCodes cc;
} State8080;

void read_rom_into_memory(State8080 *emu) {
    char* rom_files[] = {"invaders.h", "invaders.g", "invaders.f", "invaders.e"};
    int mem_offsets[] = {0, 0x800, 0x1000, 0x1800};
    for (int i = 0; i < sizeof(rom_files) / sizeof(rom_files[0]); i++) {
        char *file = rom_files[i];
        unsigned int offset = mem_offsets[i];

        FILE *fp = fopen(rom_files[i], "rb");

        if (fp == NULL) {
            printf("error opening %s\n", file);
        }

        fseek(fp, 0L, SEEK_END);
        size_t size = ftell(fp);
        fseek(fp, 0L, SEEK_SET);

        fread(&emu->memory[offset], size, 1, fp);
    }
}

int emulate(State8080 *emu) {
    return 0;
}

int main() {
    int error = 0;
    struct State8080 emulator =  {
        .a = 0,
        .b = 0,
        .c = 0,
        .d = 0,
        .e = 0,
        .h = 0,
        .l = 0,
        .pc = 0,
        .sp = 0,
        .memory = malloc(0xffff),
        .cc = {
            .zero = 0,
            .parity = 0,
            .auxcarry = 0,
            .carry = 0,
            .sign = 0,
        }
    };

    read_rom_into_memory(&emulator);
    while (error = 0) {
        emulate(&emulator);
    }

    return error;
}

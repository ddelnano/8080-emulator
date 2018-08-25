#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "disassembler.h"

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
        fclose(fp);
    }
}

void unimplemented_instruction(State8080 *emu) {
    emu->pc--;

    printf("\nUnimplemented instruction: [");
    disassemble(&emu->memory[emu->pc]);
    printf("]\n");

    exit(1);
}

// TODO: Needs to be tested
/* int parity(int number) { */
/*     int count = 0; */
/*     uint8_t b = 1; */
/*     for (int i = 0; i < 8; i++) { */
/*         if (number & (b << i)) { */
/*             count++; */
/*         } */
/*     } */
/*     if (count % 2) return 1; */

/*     return 0; */
/* } */
int parity(int x, int size)
{
    int i;
    int p = 0;
    x = (x & ((1<<size)-1));
    for (i=0; i<size; i++)
    {
        if (x & 0x1) p++;
        x = x >> 1;
    }
    return (0 == (p & 0x1));
}


int emulate(State8080 *emu) {
    unsigned char *opcode = &emu->memory[emu->pc];
    printf("%04x ", emu->pc);
    emu->pc++;

    disassemble(opcode);

    switch (*opcode) {
        case 0x00: break;
        case 0x01: // LXI B
                   emu->b = opcode[2];
                   emu->c = opcode[1];
                   emu->pc += 2;
            break;
        case 0x05: // DCR B
            {
                uint8_t result = emu->b - 1;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->b = result;
            }
            break;
        case 0x06: // MVI B, ADDR
            emu->b = opcode[1];
            emu->pc++;
            break;
        case 0x09: // DAD B
            {
                uint32_t result = ((emu->h << 8) | emu->l) + ((emu->b << 8) | emu->c);
                emu->cc.carry = (0x10000 == (result & 0x10000));
                emu->h = (result & 0xff00) >> 8;
                emu->l = result & 0xff;
            }
            break;
        case 0x0d: // DCR C
            {
                uint8_t result = emu->c - 1;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->c = result;
            }
            break;
        case 0x0e: // MVI C,addr
            emu->c = opcode[1];
            emu->pc++;
            break;
        case 0x0f: // RRC
            emu->cc.carry = emu->a & 0x1;
            emu->a = (emu->a >> 1) | (emu->cc.carry << 7);
            break;
        case 0x11:                                       // LXI D, D
            emu->d = opcode[2];
            emu->e = opcode[1];
            emu->pc += 2;
            break;
        case 0x13:                                       // INX D
            emu->e++;
            if (emu->e == 0) {
                emu->d++;
            }
            break;
        case 0x1a:                                       // LDAX D
            {
                uint16_t offset = (emu->d << 8) | emu->e;
                emu->a = emu->memory[offset];
            }
            break;
        case 0x19: // DAD D
            {
                uint32_t result = ((emu->h << 8) | emu->l) + ((emu->d << 8) | emu->e);
                emu->cc.carry = (0x10000 == (result & 0x10000));
                emu->h = (result & 0xff00) >> 8;
                emu->l = result & 0xff;
            }
            break;
        case 0x21:                                       // LXI H, D
            emu->h = opcode[2];
            emu->l = opcode[1];
            emu->pc += 2;
            break;
        case 0x23:                                       // INX H
            emu->l++;
            if (emu->l == 0) {
                emu->h++;
            }
            break;
        case 0x26: // MVI H,value
            emu->h = opcode[1];
            emu->pc++;
            break;
        case 0x29: // DAD H
            {
                uint32_t result = 2 * ((emu->h << 8) | emu->l);
                emu->cc.carry = (0x10000 == (result & 0x10000));
                emu->h = (result & 0xff00) >> 8;
                emu->l = result & 0xff;
            }
            break;
        case 0x31:                                       // LXI SP, D
            emu->sp = (opcode[2] << 8) | opcode[1];
            emu->pc += 2;
            break;
        case 0x32: // STA addr
            {
                uint16_t offset = opcode[2] << 8 | opcode[1];
                emu->memory[offset] = emu->a;
                emu->pc += 2;
            }
            break;
        case 0x36: // MVI M,addr
            {
                uint16_t offset = (emu->h << 8) | emu->l;
                emu->memory[offset] = opcode[1];
                emu->pc++;
            }
            break;
        /* case 0x3d: // DCR A */
        /*     { */
        /*         uint8_t result = emu->a - 1; */
        /*         emu->cc.zero = (result == 0); */
        /*         emu->cc.parity = parity(result, 8); */
        /*         emu->cc.sign = (0x80 == (result & 0x80)); */
        /*         emu->a = result; */
        /*     } */
        /*     break; */
        case 0x3a: // LDA addr
            emu->a = emu->memory[opcode[2] << 8 | opcode[1]];
            emu->pc += 2;
            break;
        case 0x3e: // MVI A, ADDR
            emu->a = opcode[1];
            emu->pc++;
            break;
        case 0x56: // MOV D,M
            {
                uint16_t offset = (emu->h << 8) | emu->l;
                emu->d = emu->memory[offset];
            }
            break;
        case 0x5e: // MOV E,M
            {
                uint16_t offset = (emu->h << 8) | emu->l;
                emu->e = emu->memory[offset];
            }
            break;
        case 0x66: // MOV H,M
            {
                uint16_t offset = (emu->h << 8) | emu->l;
                emu->h = emu->memory[offset];
            }
            break;
        case 0x6f:
            emu->l = emu->a;
            break;
        case 0x77:                                       // MOV M,A
            {
                uint16_t offset = (emu->h << 8) | emu->l;
                emu->memory[offset] = emu->a;
            }
            break;
        case 0x7a: // MOV A,D
            emu->a = emu->d;
            break;
        case 0x7b: // MOV A,E
            emu->a = emu->e;
            break;
        case 0x7c:
            emu->a = emu->h;
            break;
        case 0x7e: // MOV A,M
            {
                uint16_t offset = (emu->h << 8) | emu->l;
                emu->a = emu->memory[offset];
            }
            break;
        case 0xa7: // ANA A
            emu->cc.carry = 0;
            emu->cc.auxcarry = 0;
            emu->cc.zero = (emu->a == 0);
            emu->cc.parity = parity(emu->a, 8);
            emu->cc.sign = (0x80 == (emu->a & 0x80));
            break;
        case 0xaf: // XRA A
            emu->a = 0;
            emu->cc.zero = 1;
            emu->cc.carry = 0;
            emu->cc.sign = 0;
            emu->cc.auxcarry = 0;
            emu->cc.parity = 1;
            break;
        case 0xc1: // POP B
            {
                emu->b = emu->memory[emu->sp+1];
                emu->c = emu->memory[emu->sp];
                emu->sp += 2;
            }
            break;
        case 0xc2: // JNZ ADDR
            if (emu->cc.zero == 0)
                emu->pc = (opcode[2] << 8) | opcode[1];
            else
                emu->pc += 2;
            break;
        case 0xc3: // JMP ADDR
            emu->pc = (opcode[2] << 8) | opcode[1];
            break;
        case 0xc5: // PUSH B
            emu->memory[emu->sp - 1] = emu->b;
            emu->memory[emu->sp - 2] = emu->c;
            emu->sp -= 2;
            break;
        case 0xc6: // ADI addr
            {
                uint16_t result = emu->a + opcode[1];
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result & 0xffff, 16);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a + opcode[1];
                emu->pc++;
            }
            break;
        case 0xc9: // RET
            emu->pc = emu->memory[emu->sp] | (emu->memory[emu->sp+1] << 8);
            emu->sp += 2;
            break;
        // TODO: Understand how to tell how endianess matches the ret_instruction
        case 0xcd:
            {
                uint16_t ret_instruction = emu->pc + 2;
                emu->memory[emu->sp-1] = (ret_instruction >> 8) & 0xff;
                emu->memory[emu->sp-2] = ret_instruction & 0xff;
                emu->sp = emu->sp - 2;
                emu->pc = (opcode[2] << 8) | opcode[1];
            }
            break;
        case 0xd1: // POP D
            {
                emu->d = emu->memory[emu->sp+1];
                emu->e = emu->memory[emu->sp];
                emu->sp += 2;
            }
            break;
        case 0xd3: // OUT addr
            // TODO: Don't know what to do here yet.
            emu->pc++;
            break;
        case 0xd5: // PUSH D
            emu->memory[emu->sp - 1] = emu->d;
            emu->memory[emu->sp - 2] = emu->e;
            emu->sp -= 2;
            break;
        case 0xe1: // POP H
            {
                emu->h = emu->memory[emu->sp+1];
                emu->l = emu->memory[emu->sp];
                emu->sp += 2;
            }
            break;
        case 0xe5:
            emu->memory[emu->sp - 1] = emu->h;
            emu->memory[emu->sp - 2] = emu->l;
            emu->sp -= 2;
            break;
        case 0xe6: // ANI
            {
                emu->a = emu->a & opcode[1];
                uint16_t result = emu->a & opcode[1];
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result & 0xffff, 16);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result;
                emu->pc++;
                // TODO: why no auxcarry
                /* emu->auxcarry = */ 
            }
            break;
        case 0xeb:
            {
                uint16_t hl = (emu->h << 8) | emu->l;
                uint16_t de = (emu->d << 8) | emu->e;
                emu->h = (de & 0xff00) >> 8;
                emu->l = de & 0xff;
                emu->d = (hl & 0xff00) >> 8;
                emu->e = hl & 0xff;
            }
            break;
        case 0xf1: // POP PSW
            {
                emu->a = emu->memory[emu->sp + 1];
                uint8_t flags = emu->memory[emu->sp];
                emu->cc.zero = (0x40 == (flags & 0x40));
                emu->cc.sign = (0x80 == (flags & 0x80));
                emu->cc.auxcarry = (0x10 == (flags & 0x10));
                emu->cc.parity= (0x4 == (flags & 0x4));
                emu->cc.carry = flags & 0x1;
                emu->sp += 2;
            }
            break;
        case 0xf5: // PUSH PSW
            {
                emu->memory[emu->sp - 2] = (emu->cc.zero << 6    |
                                            emu->cc.sign << 7    |
                                            emu->cc.auxcarry << 4|
                                            emu->cc.parity << 2  |
                                            emu->cc.carry & 0x1);
                emu->memory[emu->sp - 1] = emu->a;
                emu->sp -= 2;
                break;
            }
            break;
        case 0xfb: // EI (enable interrupt)
            emu->int_enable = 1;
            break;
        case 0xfe:
            {
                uint16_t result = emu->a - opcode[1];
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result & 0xffff, 16);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                // TODO: why no auxcarry
                /* emu->auxcarry = */ 
                emu->pc++;
            }
            break;
        default: unimplemented_instruction(emu); break;
    }

        printf("\t");
        printf("%c", emu->cc.zero ? 'z' : '.');
        printf("%c", emu->cc.sign ? 's' : '.');
        printf("%c", emu->cc.parity ? 'p' : '.');
        printf("%c", emu->cc.carry ? 'c' : '.');
        printf("%c  ", emu->cc.auxcarry ? 'a' : '.');
        printf("A $%02x B $%02x C $%02x D $%02x E $%02x H $%02x L $%02x SP %04x\n", emu->a, emu->b, emu->c,
        emu->d, emu->e, emu->h, emu->l, emu->sp);

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
    while (error == 0) {
        error = emulate(&emulator);
    }

    return error;
}

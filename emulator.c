#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "emulator.h"
#include "disassembler.h"

unsigned int instruction_count = 0;
uint16_t last_1000_pcs[1000] = { 0 };
uint16_t last_1000_sps[1000] = { 0 };

unsigned char cycles8080[] = {
    4, 10, 7, 5, 5, 5, 7, 4, 4, 10, 7, 5, 5, 5, 7, 4, //0x00..0x0f
    4, 10, 7, 5, 5, 5, 7, 4, 4, 10, 7, 5, 5, 5, 7, 4, //0x10..0x1f
    4, 10, 16, 5, 5, 5, 7, 4, 4, 10, 16, 5, 5, 5, 7, 4, //etc
    4, 10, 13, 5, 10, 10, 10, 4, 4, 10, 13, 5, 5, 5, 7, 4,
    
    5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5, //0x40..0x4f
    5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5,
    5, 5, 5, 5, 5, 5, 7, 5, 5, 5, 5, 5, 5, 5, 7, 5,
    7, 7, 7, 7, 7, 7, 7, 7, 5, 5, 5, 5, 5, 5, 7, 5,
    
    4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4, //0x80..8x4f
    4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
    4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
    4, 4, 4, 4, 4, 4, 7, 4, 4, 4, 4, 4, 4, 4, 7, 4,
    
    11, 10, 10, 10, 17, 11, 7, 11, 11, 10, 10, 10, 10, 17, 7, 11, //0xc0..0xcf
    11, 10, 10, 10, 17, 11, 7, 11, 11, 10, 10, 10, 10, 17, 7, 11,
    11, 10, 10, 18, 17, 11, 7, 11, 11, 5, 10, 5, 17, 17, 7, 11,
    11, 10, 10, 4, 17, 11, 7, 11, 11, 5, 10, 4, 17, 17, 7, 11,
};

void read_rom_into_memory(State8080 *emu, unsigned int start_addr, char *rom_files[], unsigned int rom_count) {
    unsigned int mem_offset = start_addr;
    for (unsigned int i = 0; i < rom_count; i++) {
        char *file = rom_files[i];
        FILE *fp = fopen(rom_files[i], "rb");

        if (fp == NULL) {
            printf("error opening %s\n", file);
        }

        fseek(fp, 0L, SEEK_END);
        size_t size = ftell(fp);
        fseek(fp, 0L, SEEK_SET);

        fread(&emu->memory[mem_offset], size, 1, fp);
        fclose(fp);

        mem_offset += size;
    }
}

void record_debug_info(State8080* emu)
{
    last_1000_pcs[instruction_count] = emu->pc;
    last_1000_sps[instruction_count] = emu->sp;
    instruction_count++;
    
    if (instruction_count == 1000) {
        instruction_count = 0;
    }
}

void write_memory(State8080* emu, uint16_t addr, uint8_t value)
{
#ifndef CPU_TEST
    if (addr < 0x2000) {
        printf("Writing into ROM at address %x, exiting!\n", addr);
        print_last_1000_instructions(emu);
        exit(0);
    }
    if (addr >=0x4000)
    {    
        printf("Writing out of Space Invaders RAM not allowed %x\n", addr);
        return;
    }
#endif

    emu->memory[addr] = value;
}

uint8_t read_memory_from_hl(State8080* emu)
{
    uint16_t offset = (emu->h << 8) | emu->l;
    return emu->memory[offset];
}

void write_memory_from_hl(State8080* emu, uint8_t value)
{
    uint16_t offset = (emu->h << 8) | emu->l;
    if (offset == 0x59d) {
        printf("Overwriting DAA instruction back to normal!!");
        exit(1);
    }
    write_memory(emu, offset, value);
    return;
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

void push_stack(State8080* emu, uint16_t value) {
    
    write_memory(emu, emu->sp - 1, (value & 0xff00) >> 8);
    write_memory(emu, emu->sp - 2, value & 0xff);
    
    emu->sp -= 2;
}

void generate_interrupt(State8080* emu, int interrupt_num) {
    // Save pc on the stack
    push_stack(emu, emu->pc);
    
    // Each restart instruction is 8 bytes and there
    // are 8 of them.  Juump to the correct one.
    // https://stackoverflow.com/questions/2165914/how-do-interrupts-work-on-the-intel-8080
    emu->pc = 8 * interrupt_num;
    
    // DI (disable interrupt) instruction
    emu->int_enable = 0;
}

int emulate(State8080 *emu) {
    unsigned char *opcode = &emu->memory[emu->pc];
#ifdef DEBUG
    printf("%04x ", emu->pc);

    disassemble(opcode);
#endif
    emu->pc++;

    switch (*opcode) {
        case 0x00: break;
        case 0x01: // LXI B
                   emu->b = opcode[2];
                   emu->c = opcode[1];
                   emu->pc += 2;
            break;
        case 0x02: // STAX B
            {
                uint16_t offset = emu->b << 8 | emu->c;
                write_memory(emu, offset, emu->a);
            }
            break;
        case 0x03: // INX B
            emu->c++;
            if (emu->c == 0) {
                emu->b++;
            }
            break;
        case 0x04: // INR B
            {
                uint16_t result = emu->b + 1;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->b = emu->b + 1;
            }
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
        case 0x07: // RLC
            {
                uint8_t x = emu->a;
                emu->cc.carry = 0x80 == (x & 0x80);
                emu->a = x << 1 | ((x & 0x80) >> 7);
            }
            break;
        case 0x08: // NOP
            break;
        case 0x09: // DAD B
            {
                uint32_t result = ((emu->h << 8) | emu->l) + ((emu->b << 8) | emu->c);
                emu->cc.carry = (0x10000 == (result & 0x10000));
                emu->h = (result & 0xff00) >> 8;
                emu->l = result & 0xff;
            }
            break;
        case 0x0c: // INR C
            {
                uint16_t result = emu->c + 1;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->c = emu->c + 1;
            }
            break;
        case 0x10: // NOP
            break;
        case 0x12: // STAX D
            {
                uint16_t offset = emu->d << 8 | emu->e;
                write_memory(emu, offset, emu->a);
            }
            break;
        case 0x14: // INR D
            {
                uint16_t result = emu->d + 1;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->d = emu->d + 1;
            }
            break;
        case 0x17: // RAL
            {
                uint8_t bit_7 = 0x80 == (emu->a & 0x80);
                emu->a = emu->a << 1 | emu->cc.carry;
                emu->cc.carry = bit_7;
            }
            break;
        case 0x1f: // RAR
            {
                uint8_t x = (1 == (emu->a & 1));
                emu->a = emu->cc.carry << 7 | emu->a >> 1;
                emu->cc.carry = x;
            }
            break;
        case 0x0a: // LDAX B
            {
                uint16_t offset = emu->b << 8 | emu->c;
                emu->a = emu->memory[offset];
            }
            break;
        case 0x0b: // DCX B
            emu->c -= 1;
            if (emu->c == 0xff) {
                emu->b -= 1;
            }
            break;
        case 0x0d: // DCR C
            {
                uint8_t result = emu->c - 1;
                emu->cc.zero = ((result & 0xff) == 0);
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
        {
            uint8_t x = emu->a;
            emu->a = ((x & 1) << 7) | (x >> 1);
            emu->cc.carry = (1 == (x&1));
        }
            break;
        case 0x11:                                       // LXI D,word
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
        case 0x15: // DCR D
            {
                uint8_t result = emu->d - 1;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->d = result;
            }
            break;
        case 0x16: // MVI D,data
            emu->d = opcode[1];
            emu->pc++;
            break;
        case 0x18: // NOP
            break;
        case 0x1a:                                       // LDAX D
            {
                uint16_t offset = (emu->d << 8) | emu->e;
                emu->a = emu->memory[offset];
            }
            break;
        case 0x1b: // DCX D
            emu->e -= 1;
            if (emu->e == 0xff) {
                emu->d -= 1;
            }
            break;
        case 0x1d: // DCR E
            {
                uint8_t result = emu->e - 1;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->e = result;
            }
            break;
        case 0x1e: // MVI E,data
            emu->e = opcode[1];
            emu->pc++;
            break;
        case 0x19: // DAD D
            {
                uint32_t result = ((emu->h << 8) | emu->l) + ((emu->d << 8) | emu->e);
                emu->cc.carry = (0x10000 == (result & 0x10000));
                emu->h = (result & 0xff00) >> 8;
                emu->l = result & 0xff;
            }
            break;
        case 0x1c: // INR E
            {
                uint16_t result = emu->e + 1;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->e = emu->e + 1;
            }
            break;
        case 0x20: // NOP
            break;
        case 0x21:                                       // LXI H, D
            emu->h = opcode[2];
            emu->l = opcode[1];
            emu->pc += 2;
            break;
        case 0x22: // SHLD addr
            {
                uint16_t offset = opcode[1] | (opcode[2] << 8);
                write_memory(emu, offset, emu->l);
                write_memory(emu, offset + 1, emu->h);
                emu->pc += 2;
            }
            break;
        case 0x23:                                       // INX H
            emu->l++;
            if (emu->l == 0) {
                emu->h++;
            }
            break;
        case 0x24: // INR H
            {
                uint16_t result = emu->h + 1;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->h = emu->h + 1;
            }
            break;
        case 0x25: // DCR H
            {
                uint8_t result = emu->h - 1;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->h = result;
            }
            break;
        case 0x26: // MVI H,value
            emu->h = opcode[1];
            emu->pc++;
            break;
        case 0x27: // DAA, TODO: not implementing this so just skip it
            if ((emu->a &0xf) > 9)
                emu->a += 6;
            if ((emu->a&0xf0) > 0x90)
            {
                uint16_t res = (uint16_t) emu->a + 0x60;
                emu->a = res & 0xff;
                emu->cc.carry = (res > 0xff);
                emu->cc.zero = ((res&0xff) == 0);
                emu->cc.sign = (0x80 == (res & 0x80));
                emu->cc.parity = parity(res&0xff, 8);
            }
            break;
        case 0x28: // NOP
            break;
        case 0x29: // DAD H
            {
                uint32_t result = 2 * ((emu->h << 8) | emu->l);
                emu->cc.carry = (0x10000 == (result & 0x10000));
                emu->h = (result & 0xff00) >> 8;
                emu->l = result & 0xff;
            }
            break;
        case 0x2a: // LHLD addr
            {
                uint16_t offset = opcode[1] | opcode[2] << 8;
                emu->l = emu->memory[offset];
                emu->h = emu->memory[offset+1];
                emu->pc += 2;
            }
            break;
        case 0x2b: // DCX H
            emu->l -= 1;
            if (emu->l == 0xff) {
                emu->h -= 1;
            }
            break;
        case 0x2c: // INR L
            {
                uint16_t result = emu->l + 1;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->l = emu->l + 1;
            }
            break;
        case 0x2d: // DCR L
            {
                uint8_t result = emu->l - 1;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->l = result;
            }
            break;
        case 0x2e: // MVI L,data
            emu->l = opcode[1];
            emu->pc++;
            break;
        /* case 0x2c: // INR L */
        /*     emu->l++; */
        /*     break; */
        case 0x2f: // CMA
            emu->a = ~emu->a;
            break;
        case 0x30: // SIM (NOP)
            break;
        case 0x31:                                       // LXI SP, D
            emu->sp = (opcode[2] << 8) | opcode[1];
            emu->pc += 2;
            break;
        case 0x32: // STA addr
            {
                uint16_t offset = opcode[2] << 8 | opcode[1];
                write_memory(emu, offset, emu->a);
                emu->pc += 2;
            }
            break;
        case 0x33: // INX SP
            emu->sp += 1;
            break;
        case 0x34: // INR M
            {
                uint16_t result = read_memory_from_hl(emu) + 1;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                write_memory_from_hl(emu, result & 0xff);
            }
            break;
        case 0x35: // DCR M
            {
                uint8_t value = read_memory_from_hl(emu);
                emu->cc.zero = (value == 0);
                emu->cc.sign = (0x80 == (value & 0x80));
                emu->cc.parity = parity(value, 8);
                write_memory_from_hl(emu, value - 1);
            }
            break;
        case 0x36: // MVI M,addr
            {
                uint16_t offset = (emu->h << 8) | emu->l;
                write_memory(emu, offset, opcode[1]);
                emu->pc++;
            }
            break;
        case 0x37: // STC
            emu->cc.carry = 1;
            break;
        case 0x38: // NOP
            break;
        case 0x39: // DAD SP
            {
                uint32_t hl = emu->h << 8 | emu->l;
                uint32_t res = hl + emu->sp;
                emu->h = (res & 0xff00) >> 8;
                emu->l = res & 0xff;
                emu->cc.carry = (res & 0xffff0000) > 0;
            }
            break;
        case 0x3b:
            emu->sp -= 1;
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
        case 0x3c: // INR A
            {
                uint16_t result = emu->a + 1;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a + 1;
            }
            break;
        case 0x3d: // DCR A
            {
                uint8_t result = emu->a - 1;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->a = result;
            }
            break;
        case 0x3e: // MVI A, ADDR
            emu->a = opcode[1];
            emu->pc++;
            break;
        case 0x3f: // CMC
            emu->cc.carry = emu->cc.carry ? 0 : 1;
            break;
        case 0x40: // MOV B,B
            break;
        case 0x41: // MOV B,C
            emu->b = emu->c;
            break;
        case 0x42: // MOV B,D
            emu->b = emu->d;
            break;
        case 0x43: // MOV B,E
            emu->b = emu->e;
            break;
        case 0x44: // MOV B,H
            emu->b = emu->h;
            break;
        case 0x45: // MOV B,L
            emu->b = emu->l;
            break;
        case 0x46: // MOV B,M
            emu->b = read_memory_from_hl(emu);
            break;
        case 0x47: // MOV B,A
            emu->b = emu->a;
            break;
        case 0x48: // MOV C,B
            emu->c = emu->b;
            break;
        case 0x49: // MOV C,C
            break;
        case 0x4a: // MOV C,D
            emu->c = emu->d;
            break;
        case 0x4b: // MOV C,E
            emu->c = emu->e;
            break;
        case 0x4c: // MOV C,H
            emu->c = emu->h;
            break;
        case 0x4e: // MOV C,M
            {
                uint16_t offset = (emu->h << 8) | emu->l;
                emu->c = emu->memory[offset];
            }
            break;
        case 0x4d: // MOV C,L
            emu->c = emu->l;
            break;
        case 0x4f: // MOV C,A
            emu->c = emu->a;
            break;
        case 0x50: // MOV D,B
            emu->d = emu->b;
            break;
        case 0x51: // MOV D,C
            emu->d = emu->c;
            break;
        case 0x52: // MOV D,D
            break;
        case 0x53: // MOV D,E
            emu->d = emu->e;
            break;
        case 0x54: // MOV D,H
            emu->d = emu->h;
            break;
        case 0x55: // MOV D,L
            emu->d = emu->l;
            break;
        case 0x56: // MOV D,M
            {
                uint16_t offset = (emu->h << 8) | emu->l;
                emu->d = emu->memory[offset];
            }
            break;
        case 0x57: // MOV D,A
            emu->d = emu->a;
            break;
        case 0x58: // MOV E,B
            emu->e = emu->b;
            break;
        case 0x59: // MOV E,C
            emu->e = emu->c;
            break;
        case 0x5a: // MOV E,D
            emu->e = emu->d;
            break;
        case 0x5c: // MOV E,H
            emu->e = emu->h;
            break;
        case 0x5d: // MOV E,L
            emu->e = emu->l;
            break;
        case 0x5e: // MOV E,M
            {
                uint16_t offset = (emu->h << 8) | emu->l;
                emu->e = emu->memory[offset];
            }
            break;
        case 0x5f: // MOV E,A
            emu->e = emu->a;
            break;
        case 0x60: // MOV H,B
            emu->h = emu->b;
            break;
        case 0x61: // MOV H,C
            emu->h = emu->c;
            break;
        case 0x62: // MOV H,D
            emu->h = emu->d;
            break;
        case 0x63: // MOV H,E
            emu->h = emu->e;
            break;
        case 0x64: break; // MOV H,H
        case 0x65: // MOV H,L
            emu->h = emu->l;
            break;
        case 0x66: // MOV H,M
            {
                uint16_t offset = (emu->h << 8) | emu->l;
                emu->h = emu->memory[offset];
            }
            break;
        case 0x67: // MOV H,A
            emu->h = emu->a;
            break;
        case 0x68: // MOV L,B
            emu->l = emu->b;
            break;
        case 0x69: // MOV L,C
            emu->l = emu->c;
            break;
        case 0x6a: // MOV L,D
            emu->l = emu->d;
            break;
        case 0x6b: // MOV L,E
            emu->l = emu->e;
            break;
        case 0x6c: // MOV L,H
            emu->l = emu->h;
            break;
        case 0x6d: break; // MOV L,L
        case 0x6e: // MOV L,M
            emu->l = read_memory_from_hl(emu);
            break;
        case 0x6f: // MOV L,A
            emu->l = emu->a;
            break;
        case 0x70: // MOV M,B
            write_memory_from_hl(emu, emu->b);
            break;
        case 0x71:
            write_memory_from_hl(emu, emu->c);
            break;
        case 0x72: // MOV M,D
            write_memory_from_hl(emu, emu->d);
            break;
        case 0x73: // MOV M,E
            write_memory_from_hl(emu, emu->e);
            break;
        case 0x74: // MOV M,H
            write_memory_from_hl(emu, emu->h);
            break;
        case 0x75: // MOV M,L
            write_memory_from_hl(emu, emu->l);
            break;
        case 0x77:                                       // MOV M,A
            {
                uint16_t offset = (emu->h << 8) | emu->l;
                write_memory(emu, offset, emu->a);
            }
            break;
        case 0x78: // MOV A,B
            emu->a = emu->b;
            break;
        case 0x79: // MOV A,C
            emu->a = emu->c;
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
        case 0x7d: // MOV A,L
            emu->a = emu->l;
            break;
        case 0x7e: // MOV A,M
            {
                uint16_t offset = (emu->h << 8) | emu->l;
                emu->a = emu->memory[offset];
            }
            break;
        case 0x7f: break;
        case 0x80: // ADD B
            {
                uint16_t result = emu->a + emu->b;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a + emu->b;
            }
            break;
        case 0x81: // ADD C
            {
                uint16_t result = emu->a + emu->c;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a + emu->c;
            }
            break;
        case 0x82: // ADD D
            {
                uint16_t result = emu->a + emu->d;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a + emu->d;
            }
            break;
        case 0x83: // ADD E
            {
                uint16_t result = emu->a + emu->e;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a + emu->e;
            }
            break;
        case 0x84: // ADD H
            {
                uint16_t result = emu->a + emu->h;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a + emu->h;
            }
            break;
        case 0x85: // ADD L
            {
                uint16_t result = emu->a + emu->l;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a + emu->l;
            }
            break;
        case 0x86: // ADD M
            {
                uint16_t result = emu->a + read_memory_from_hl(emu);
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x87: // ADD A
            {
                uint16_t result = emu->a + emu->a;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a + emu->a;
            }
            break;
        case 0x88: // ADC B
            {
                uint16_t result = emu->a + emu->b + emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x89: // ADC C
            {
                uint16_t result = emu->a + emu->c + emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x8a: // ADC D
            {
                uint16_t result = emu->a + emu->d + emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x8b: // ADC E
            {
                uint16_t result = emu->a + emu->e + emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x8c: // ADC H
            {
                uint16_t result = emu->a + emu->h + emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x8d: // ADC L
            {
                uint16_t result = emu->a + emu->l + emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x8e: // ADC M
            {
                uint16_t result = emu->a + read_memory_from_hl(emu) + emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x8f: // ADC A 
            {
                uint16_t result = emu->a + emu->a + emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x90: // SUB B
            {
                uint16_t result = emu->a - emu->b;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a - emu->b;
            }
            break;
        case 0x91: // SUB C
            {
                uint16_t result = emu->a - emu->c;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a - emu->c;
            }
            break;
        case 0x92: // SUB D
            {
                uint16_t result = emu->a - emu->d;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a - emu->d;
            }
            break;
        case 0x93: // SUB E
            {
                uint16_t result = emu->a - emu->e;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a - emu->e;
            }
            break;
        case 0x94: // SUB H
            {
                uint16_t result = emu->a - emu->h;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a - emu->h;
            }
            break;
        case 0x95: // SUB L
            {
                uint16_t result = emu->a - emu->l;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a - emu->l;
            }
            break;
        case 0x96: // SUB M
            {
                uint16_t result = emu->a - read_memory_from_hl(emu);
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = emu->a - read_memory_from_hl(emu);
            }
            break;
        case 0x97: // SUB A 
            {
                uint16_t result = emu->a - emu->a;
                emu->cc.zero = 1;
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = 0;
            }
            break;
        case 0x98: // SBB B
            {
                uint16_t result = emu->a - emu->b - emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x99: // SBB C
            {
                uint16_t result = emu->a - emu->c - emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x9a: // SBB D
            {
                uint16_t result = emu->a - emu->d - emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x9b: // SBB E
            {
                uint16_t result = emu->a - emu->e - emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x9c: // SBB H
            {
                uint16_t result = emu->a - emu->h - emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x9d: // SBB L
            {
                uint16_t result = emu->a - emu->l - emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x9e: // SBB M
            {
                uint16_t result = emu->a - read_memory_from_hl(emu) - emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0x9f: // SBB A 
            {
                uint16_t result = - emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
            }
            break;
        case 0xa0: // ANA B
        {
            uint8_t result = emu->a & emu->b;
            emu->cc.zero = (result == 0);
            emu->cc.parity = parity(result, 8);
            emu->cc.sign = (0x80 == (result & 0x80));
            emu->cc.carry = 0;
            emu->cc.auxcarry = 0;
            emu->a = result & 0xff;
        }
            break;
        case 0xa1: // ANA C
            {
                uint8_t result = emu->a & emu->c;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result & 0xff;
            }
            break;
        case 0xa2: // ANA D
            {
                uint8_t result = emu->a & emu->d;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result & 0xff;
            }
            break;
        case 0xa3: // ANA E
            {
                uint8_t result = emu->a & emu->e;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result & 0xff;
            }
            break;
        case 0xa4: // ANA H
            {
                uint8_t result = emu->a & emu->h;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result & 0xff;
            }
            break;
        case 0xa5: // ANA L
            {
                uint8_t result = emu->a & emu->l;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result & 0xff;
            }
            break;
        case 0xa6: // ANA M
            {
                uint8_t result = emu->a & read_memory_from_hl(emu);
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result & 0xff;
            }
            break;
        case 0xa7: // ANA A
            emu->cc.carry = 0;
            emu->cc.auxcarry = 0;
            emu->cc.zero = (emu->a == 0);
            emu->cc.parity = parity(emu->a, 8);
            emu->cc.sign = (0x80 == (emu->a & 0x80));
            break;
        case 0xa8: // XRA B
            {
                uint8_t result = emu->a ^ emu->b;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result;
            }
            break;
        case 0xa9: // XRA C
            {
                uint8_t result = emu->a ^ emu->c;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result;
            }
            break;
        case 0xaa: // XRA D
            {
                uint8_t result = emu->a ^ emu->d;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result;
            }
            break;
        case 0xab: // XRA E
            {
                uint8_t result = emu->a ^ emu->e;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result;
            }
            break;
        case 0xac: // XRA H
            {
                uint8_t result = emu->a ^ emu->h;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result;
            }
            break;
        case 0xad: // XRA L
            {
                uint8_t result = emu->a ^ emu->l;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result;
            }
            break;
        case 0xae: // XRA M
            {
                uint8_t result = emu->a ^ read_memory_from_hl(emu);
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result;
            }
            break;
        case 0xaf: // XRA A
            emu->a = 0;
            emu->cc.zero = 1;
            emu->cc.carry = 0;
            emu->cc.sign = 0;
            emu->cc.auxcarry = 0;
            emu->cc.parity = 1;
            break;
        case 0xb0: // ORA B
            {
                uint8_t result = emu->a | emu->b;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result & 0xff;
            }
            break;
        case 0xb1: // ORA C
            {
                uint8_t result = emu->a | emu->c;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result & 0xff;
            }
            break;
        case 0xb2: // ORA D
            {
                uint8_t result = emu->a | emu->d;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result & 0xff;
            }
            break;
        case 0xb3: // ORA E
            {
                uint8_t result = emu->a | emu->e;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result & 0xff;
            }
            break;
        case 0xb4: // ORA H
            {
                uint8_t result = emu->a | emu->h;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result & 0xff;
            }
            break;
        case 0xb5: // ORA L
            {
                uint8_t result = emu->a | emu->l;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result & 0xff;
            }
            break;
        case 0xb6: // ORA M
            {
                uint8_t result = emu->a | read_memory_from_hl(emu);
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result & 0xff;
            }
            break;
        case 0xb7: // ORA A 
            {
                uint8_t result = emu->a | emu->a;
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result & 0xff;
            }
            break;
        case 0xb8: // CMP B
            {
                uint16_t result = emu->a - emu->b;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
            }
            break;
        case 0xb9: // CMP C
            {
                uint16_t result = emu->a - emu->c;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
            }
            break;
        case 0xba: // CMP D
            {
                uint16_t result = emu->a - emu->d;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
            }
            break;
        case 0xbb: // CMP E
            {
                uint16_t result = emu->a - emu->e;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
            }
            break;
        case 0xbc: // CMP H
            {
                uint16_t result = emu->a - emu->h;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
            }
            break;
        case 0xbd: // CMP L
            {
                uint16_t result = emu->a - emu->l;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
            }
            break;
        case 0xbe: // CMP M 
            {
                uint16_t result = emu->a - read_memory_from_hl(emu);
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
            }
            break;
        case 0xbf: // CMP A
            emu->cc.zero = 1;
            emu->cc.parity = 1;
            emu->cc.sign = 0;
            emu->cc.carry = 0;
            break;
        case 0xc0: // RNZ
            if (emu->cc.zero == 0) {
                emu->pc = emu->memory[emu->sp] | (emu->memory[emu->sp+1] << 8);
                emu->sp += 2;
            }
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
        case 0xc4: // CNZ addr
            if (emu->cc.zero == 0) {
                uint16_t ret_instruction = emu->pc + 2;
                write_memory(emu, emu->sp - 1, (ret_instruction >> 8) & 0xff);
                write_memory(emu, emu->sp - 2, ret_instruction & 0xff);
                emu->sp = emu->sp - 2;
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
            }
            break;
        case 0xc5: // PUSH B
            write_memory(emu, emu->sp - 1, emu->b);
            write_memory(emu, emu->sp - 2, emu->c);
            emu->sp -= 2;
            break;
        case 0xc6: // ADI addr
            {
                uint16_t result = emu->a + opcode[1];
                emu->cc.zero = ((result & 0xff) == 0);
                /* emu->cc.zero = ((result & 0xff) == 0); */
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                emu->a = result & 0xff;
                emu->pc++;
            }
            break;
        case 0xc8: // RZ
            if (emu->cc.zero) {
                emu->pc = emu->memory[emu->sp] | (emu->memory[emu->sp+1] << 8);
                emu->sp += 2;
            }
            break;
        case 0xc9: // RET
            emu->pc = emu->memory[emu->sp] | (emu->memory[emu->sp+1] << 8);
            emu->sp += 2;
            break;
        case 0xca: // JZ addr
            if (emu->cc.zero) {
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
            }

            break;
        case 0xcc: // CZ addr
            if (emu->cc.zero) {
                uint16_t ret_instruction = emu->pc + 2;
                write_memory(emu, emu->sp - 1, (ret_instruction >> 8) & 0xff); 
                write_memory(emu, emu->sp - 2, ret_instruction & 0xff);
                emu->sp = emu->sp - 2;
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
            }
            break;
        // TODO: Understand how to tell how endianess matches the ret_instruction
        case 0xce: // ACI data
            {
                uint16_t result = emu->a + opcode[1] + emu->cc.carry;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                /* emu->cc.auxcarry = 0; */
                emu->a = result & 0xff;
                emu->pc++;
            }
            break;
        case 0xcd: // CALL addr
            {
                if (5 ==  ((opcode[2] << 8) | opcode[1])) {
                    /* if (emu->c == 9) */    
                    /* { */    
                        uint16_t offset = (emu->d<<8) | (emu->e);    
                        char *str = (char *)&emu->memory[offset+3];  //skip the prefix bytes
                        while (*str != '$')    
                            printf("%c", *str++);    
                        printf("\n\n");
                    /* } */    
                    /* else if (emu->c == 2) */    
                    /* { */    
                    /*     //saw this in the inspected code, never saw it called */    
                    /*     printf ("print char routine called\n"); */    
                    /* } */    
                    emu->pc += 2;
                }
                else if (0 ==  ((opcode[2] << 8) | opcode[1]))    
                {    
                    exit(0);    
                }    
                else {
                    uint16_t ret_instruction = emu->pc + 2;
                    write_memory(emu, emu->sp - 1, (ret_instruction >> 8) & 0xff);
                    write_memory(emu, emu->sp - 2, ret_instruction & 0xff);
                    emu->sp = emu->sp - 2;
                    emu->pc = (opcode[2] << 8) | opcode[1];
                }
            }
            break;
        case 0xd0: // RNC
            if (emu->cc.carry == 0) {
                emu->pc = emu->memory[emu->sp] | (emu->memory[emu->sp+1] << 8);
                emu->sp += 2;
            }
            break;
        case 0xd1: // POP D
            {
                emu->d = emu->memory[emu->sp+1];
                emu->e = emu->memory[emu->sp];
                emu->sp += 2;
            }
            break;
        case 0xd2: // JNC addr
            if (emu->cc.carry == 0) {
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
            }
            break;
        case 0xd3: // OUT addr
            // TODO: Don't know what to do here yet.
            emu->pc++;
            break;
        case 0xd4: // CNC addr
            if (emu->cc.carry == 0) {
                uint16_t ret_instruction = emu->pc + 2;
                /* emu->memory[emu->sp-1] = (ret_instruction >> 8) & 0xff; */
                /* emu->memory[emu->sp-2] = ret_instruction & 0xff; */
                push_stack(emu, ret_instruction);
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
            }
            break;
        case 0xd5: // PUSH D
            /* emu->memory[emu->sp - 1] = emu->d; */
            /* emu->memory[emu->sp - 2] = emu->e; */
            push_stack(emu, (emu->d << 8) | emu->e);
            break;
        case 0xd6: // SUI addr
            {
                uint16_t result = emu->a - opcode[1];
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                /* emu->cc.auxcarry = 0; */
                emu->a = result & 0xff;
                emu->pc++;
            }
            break;
        case 0xd8: // RC
            if (emu->cc.carry) {
                emu->pc = emu->memory[emu->sp] | (emu->memory[emu->sp+1] << 8);
                emu->sp += 2;
            }
            break;
        /* case 0xd7: // RST 2 */
        /*     { */
        /*         uint16_t ret_instruction = emu->pc + 2; */
        /*         emu->memory[emu->sp-1] = (ret_instruction >> 8) & 0xff; */
        /*         emu->memory[emu->sp-2] = ret_instruction & 0xff; */
        /*         emu->sp = emu->sp - 2; */
        /*         emu->pc = 0x10; */
        /*     } */
        /*     break; */
        case 0xda: // JC addr
            if (emu->cc.carry) {
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
            }
            break;
        case 0xdc: // CC addr
            if (emu->cc.carry) {
                uint16_t ret_instruction = emu->pc + 2;
                /* emu->memory[emu->sp-1] = (ret_instruction >> 8) & 0xff; */
                /* emu->memory[emu->sp-2] = ret_instruction & 0xff; */
                push_stack(emu, ret_instruction);
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
            }
            break;
        case 0xde: // SBI addr
            {
                uint16_t result = emu->a - (opcode[1] + emu->cc.carry);
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                /* emu->cc.auxcarry = 0; */
                emu->a = result & 0xff;
                emu->pc++;
            }
            break;
        case 0xe0: // RPO
            if (emu->cc.parity == 0) {
                emu->pc = emu->memory[emu->sp] | (emu->memory[emu->sp+1] << 8);
                emu->sp += 2;
            }
            break;
        case 0xe1: // POP H
            {
                emu->h = emu->memory[emu->sp+1];
                emu->l = emu->memory[emu->sp];
                emu->sp += 2;
            }
            break;
        case 0xe2: // JPO addr
            if (emu->cc.parity == 0) {
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
            }
            break;
        case 0xe3: // XTHL
            {
                uint8_t new_h = emu->memory[emu->sp + 1];
                uint8_t new_l = emu->memory[emu->sp];

                /* emu->memory[emu->sp + 1] = emu->h; */
                /* emu->memory[emu->sp] = emu->l; */
                write_memory(emu, emu->sp + 1, emu->h);
                write_memory(emu, emu->sp, emu->l);
                emu->h = new_h;
                emu->l = new_l;
            }
            break;
        case 0xe4: // CPO addr
            if (emu->cc.parity == 0) {
                uint16_t ret_instruction = emu->pc + 2;
                /* emu->memory[emu->sp-1] = (ret_instruction >> 8) & 0xff; */
                /* emu->memory[emu->sp-2] = ret_instruction & 0xff; */
                push_stack(emu, ret_instruction);
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
            }
            break;
        case 0xe5: // PUSH H
            /* emu->memory[emu->sp - 1] = emu->h; */
            /* emu->memory[emu->sp - 2] = emu->l; */
            push_stack(emu, (emu->h << 8) | emu->l);
            break;
        case 0xe6: // ANI
            {
                uint8_t result = emu->a & opcode[1];
                emu->cc.zero = (result == 0);
                emu->cc.parity = parity(result, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = 0;
                emu->cc.auxcarry = 0;
                emu->a = result;
                emu->pc++;
                // TODO: why no auxcarry
                /* emu->auxcarry = */ 
            }
            break;
        case 0xe8: // RPE
            if (emu->cc.parity) {
                emu->pc = emu->memory[emu->sp] | (emu->memory[emu->sp+1] << 8);
                emu->sp += 2;
            }
            break;
        case 0xe9: // PCHL
            emu->pc = emu->h << 8 | emu->l;
            break;
        case 0xea: // JPE addr
            if (emu->cc.parity) {
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
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
        case 0xec: // CPE addr
            if (emu->cc.parity) {
                uint16_t ret_instruction = emu->pc + 2;
                /* emu->memory[emu->sp-1] = (ret_instruction >> 8) & 0xff; */
                /* emu->memory[emu->sp-2] = ret_instruction & 0xff; */
                push_stack(emu, ret_instruction);
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
            }
            break;
        case 0xee: // XRI data
            {
                uint16_t result = emu->a ^ opcode[1];
                emu->cc.carry = 0;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->a = result & 0xff;
                emu->pc++;
            }
            break;
        case 0xef: // RST 5
        {
            uint16_t ret = emu->pc + 2;
            push_stack(emu, ((ret >> 8) & 0xff) | (ret & 0xff));
            emu->pc = 0x28;
        }
            break;
        case 0xf0: // RP
            if (emu->cc.sign == 0) {
                emu->pc = emu->memory[emu->sp] | (emu->memory[emu->sp+1] << 8);
                emu->sp += 2;
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
        case 0xf2: // JP addr
            if (! emu->cc.sign) {
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
            }
            break;
        case 0xf4: // CP addr
            if (emu->cc.sign == 0) {
                uint16_t ret_instruction = emu->pc + 2;
                /* emu->memory[emu->sp-1] = (ret_instruction >> 8) & 0xff; */
                /* emu->memory[emu->sp-2] = ret_instruction & 0xff; */
                push_stack(emu, ret_instruction);
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
            }
            break;
        case 0xf5: // PUSH PSW
            {
                uint16_t ret_instruction = emu->a << 8 |
                    (
                        emu->cc.zero     << 6    |
                        emu->cc.sign     << 7    |
                        emu->cc.auxcarry << 4    |
                        emu->cc.parity   << 2    |
                        (emu->cc.carry & 0x1)
                    );
                push_stack(emu, ret_instruction);
            }
            break;
        case 0xf6: // ORI data
            {
                uint16_t result = emu->a | opcode[1];
                emu->cc.carry = 0;
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->a = result & 0xff;
                emu->pc++;
            }
            break;
        case 0xf8: // RM
            if (emu->cc.sign) {
                emu->pc = emu->memory[emu->sp] | (emu->memory[emu->sp+1] << 8);
                emu->sp += 2;
            }
            break;
        case 0xf9: // SPHL
            emu->sp = emu->h << 8 | emu->l;
            break;
        case 0xfa: // JM addr
            if (emu->cc.sign) {
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
            }
            break;
        case 0xfb: // EI (enable interrupt)
            emu->int_enable = 1;
            break;
        case 0xfc: // CM addr
            if (emu->cc.sign) {
                uint16_t ret_instruction = emu->pc + 2;
                /* emu->memory[emu->sp-1] = (ret_instruction >> 8) & 0xff; */
                /* emu->memory[emu->sp-2] = ret_instruction & 0xff; */
                push_stack(emu, ret_instruction);
                emu->pc = (opcode[2] << 8) | opcode[1];
            } else {
                emu->pc += 2;
            }
            break;
        case 0xfe: // CPI addr
            {
                uint16_t result = emu->a - opcode[1];
                emu->cc.zero = ((result & 0xff) == 0);
                emu->cc.parity = parity(result & 0xff, 8);
                emu->cc.sign = (0x80 == (result & 0x80));
                emu->cc.carry = (0x100 == (result & 0x100));
                // TODO: why no auxcarry
                /* emu->auxcarry = */ 
                emu->pc++;
            }
            break;
        case 0xff: // RST 7
            {
                uint16_t ret = emu->pc + 2;
                push_stack(emu, ((ret >> 8) & 0xff) | (ret & 0xff));
                emu->pc = 0x38;
            }
            break;
        default: unimplemented_instruction(emu); break;
    }

#ifdef DEBUG
        printf("\t");
        printf("%c", emu->cc.zero ? 'z' : '.');
        printf("%c", emu->cc.sign ? 's' : '.');
        printf("%c", emu->cc.parity ? 'p' : '.');
        printf("%c", emu->cc.carry ? 'c' : '.');
        printf("%c  ", emu->cc.auxcarry ? 'a' : '.');
        printf("A $%02x B $%02x C $%02x D $%02x E $%02x H $%02x L $%02x SP %04x\n", emu->a, emu->b, emu->c,
        emu->d, emu->e, emu->h, emu->l, emu->sp);
#endif

    record_debug_info(emu);
    return cycles8080[*opcode];
}

void print_last_1000_instructions(State8080* emu)
{
    int i;
    for (i=0; i<100;i++)
    {
        int j;
        printf("%04d ", i*10);
        for (j=0; j<10; j++)
        {
            int n = i*10 + j;
//            uint16_t pc = last_1000_pcs[n];
//            unsigned char *opcode = &emu->memory[pc];
            printf("%04x %04x  ", last_1000_pcs[n], last_1000_sps[n]);
            if (n == instruction_count)
                printf("**");
        }
        printf ("\n");
    }
}


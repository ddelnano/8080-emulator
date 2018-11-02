#include "../emulator.h"

int main(int argc, char* argv[]) {
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
    unsigned int addr = 0;
    unsigned int files = argc - 1;
    if (strcmp(argv[1], "--start") == 0) {
        sscanf(argv[2], "%x", &addr);
        argv += 2;
        files -= 2;

        emulator.pc = addr;
    }

    read_rom_into_memory(&emulator, addr, ++argv, files);

// When testing the CPU we want to skip the DAA test.  This instruction
// is not needed for Space invaders to run properly.  We also need to fix
// the stack pointer.
#ifdef CPU_TEST
    // Fix the stack pointer from 0x6ad to 0x7ad    
    // this 0x06 byte 112 in the code, which is    
    // byte 112 + 0x100 = 368 in memory  
    emulator.memory[368] = 0x7;

    // Skip DAA test    
    emulator.memory[0x59d] = 0xc3; //JMP
    emulator.memory[0x59e] = 0xc2;
    emulator.memory[0x59f] = 0x05;
#endif

    printf("Starting test!\n");
    while (1) {
        emulate(&emulator);
    }
    return 0;
}

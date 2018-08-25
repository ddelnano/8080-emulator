#include <stdio.h>
#include <stdlib.h>

#include "disassembler.h"

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("Error must provide filename to disassemble");
        return 1;
    }
    FILE *fptr;
    unsigned long size;
    fptr = fopen(argv[1], "rb");

    // Seek to end of file and find size
    fseek(fptr, 0, SEEK_END);
    size = ftell(fptr);

    // Reset back to beginning of file
    fseek(fptr, 0, SEEK_SET);
    if (ftell(fptr) != 0) {
        printf("Failed to reset pointer!");
        return 1;
    }

    printf("Read file %s of size %ld\n\n", argv[1], size);

    unsigned char *buffer = malloc(size);
    fread(buffer, size, 1, fptr);
    fclose(fptr);

    unsigned int i = 0;
    while (i < size) {
        unsigned char *code = &buffer[i];

        /* printf("Found opcode [%02x]: ", buf); */
        printf("%04x ", i);

        unsigned int opcode_len = disassemble(code);

        printf("\n");
        i += opcode_len;
    }
    return 0;
}

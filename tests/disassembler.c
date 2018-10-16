#include <stdio.h>
#include <stdlib.h>

#include "../disassembler.h"

int main(int argc, char** argv)
{
    if (argc < 2) {
        printf("Error must provide filename to disassemble");
        return 1;
    }

    unsigned int pc = 0;
    for (int j = 1; j < argc; j++) {
        FILE *fptr;
        unsigned long size;
        fptr = fopen(argv[j], "rb");

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

            printf("%04x ", pc);

            int opcode_len = disassemble(code);

            if (opcode_len < 0) {
                exit(1);
            }

            printf("\n");
            i += opcode_len;
            pc += opcode_len;
        }
    }
    return 0;
}

This is my implementation of emulator101.com

## Testing

```bash
gcc -ggdb emulator.c disassembler.c -o emulator
./emulator --start 0x100 cpudiag.bin
```

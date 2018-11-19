#include "emulator.h"
#include <sys/time.h>
#include <unistd.h>

#define KEY_COIN 'c'
#define KEY_P1_LEFT 'a'
#define KEY_P1_RIGHT 's'
#define KEY_P1_FIRE ' '
#define KEY_P1_START '1'
#define KEY_PAUSE 'p'

char* files[] = {"invaders.h", "invaders.g", "invaders.f", "invaders.e"};
struct State8080* emulator;

uint8_t in_port1;
uint8_t shift0;
uint8_t shift1;
uint8_t shift_offset;
uint8_t out_port3, out_port5, last_out_port3, last_out_port5;

double timeusec()
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return ((double)time.tv_sec * 1E6) + ((double)time.tv_usec);
}

uint8_t read_in_port(uint8_t port)
{
    unsigned char a = 0;
    switch (port) {
        case 0:
            return 0xf;
            break;
        case 1:
            return in_port1;
            break;
        // TODO: Only matters for player 2.
        case 2:
            return 0;
            break;
        case 3:
        {
            uint16_t v = (shift1<<8) | shift0;
            a = ((v >> (8-shift_offset)) & 0xff);
        }
            break;
    }
    
    return a;
}

void set_out_port(uint8_t port, uint8_t value)
{
    switch(port)
    {
        case 2:
            shift_offset = value & 0x7;
            break;
        case 3:
            out_port3 = value;
            break;
        case 4:
            shift0 = shift1;
            shift1 = value;
            break;
        case 5:
            out_port5 = value;
            break;
    }
}

int main()
{
  emulator = calloc(sizeof(State8080), 1);
  emulator->memory = malloc(16 * 0x1000);
  read_rom_into_memory(emulator, 0, files, 4);
  printf("Finished reading roms into memory\n");
  uint32_t pixels[256 * 224 * 4];
  memset(pixels, 0xFF0000, 256 * 224 * sizeof(uint32_t));
  uint32_t white[256 * 224 *4];
  memset(white, 0xFF0000, 256 * 224 * sizeof(uint32_t));

  double timeSinceInterrupt = 0.0;
  double lastTimer = 0.0;
  double nextInterrupt = 0.0;
  int whichInterrupt = 1;
  while (1) {
    double now = timeusec();
    
    if (lastTimer == 0.0)
    {
        lastTimer = now;
        nextInterrupt = lastTimer + 16000.0;
        whichInterrupt = 1;
    }
    
    if ((emulator->int_enable) && (now > nextInterrupt))
    {
        if (whichInterrupt == 1)
        {
            generate_interrupt(emulator, 1);
            whichInterrupt = 2;
        }
        else
        {
            generate_interrupt(emulator, 2);
            whichInterrupt = 1;
        }
        nextInterrupt = now+8000.0;
    }
    
    
    //How much time has passed?  How many instructions will it take to keep up with
    // the current time?  Assume:
    //CPU is 2 MHz
    // so 2M cycles/sec
    
    double sinceLast = now - lastTimer; // microseconds
    int cycles_to_catch_up = 2 * sinceLast;
    int cycles = 0;
    
    while (cycles_to_catch_up > cycles)
    {
        unsigned char *op;
        op = &emulator->memory[emulator->pc];
        if (*op == 0xdb) //machine specific handling for IN
        {
            /* emulator->a = [self InPort:op[1]]; */
            emulator->a = read_in_port(op[1]);
            emulator->pc += 2;
            cycles+=3;
        }
        else if (*op == 0xd3) //machine specific handling for OUT
        {
            set_out_port(op[1], emulator->a);
            emulator->pc += 2;
            cycles+=3;
            /* [self PlaySounds]; */
        }
        else
            cycles += emulate(emulator);
    
    }
    lastTimer  = now;
    usleep(3000);

  }

  return 0;
}


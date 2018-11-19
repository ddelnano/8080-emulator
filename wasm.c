#include "emulator.h"
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include "SDL.h"

#ifdef __EMSCRIPTEN__
#include "emscripten.h"
#endif

#define RGB_ON  0xFFFFFFFF
#define RGB_OFF 0xFF000000

#define KEY_COIN 'c'
#define KEY_P1_LEFT 'a'
#define KEY_P1_RIGHT 's'
#define KEY_P1_FIRE ' '
#define KEY_P1_START '1'
#define KEY_PAUSE 'p'

_Atomic(struct State8080*) emulator;

struct draw_screen_args {
  unsigned char* game_memory;
  void* buffer;
  SDL_Renderer* renderer;
  SDL_Texture* texture;
};

char* files[] = {"invaders.h", "invaders.g", "invaders.f", "invaders.e"};

_Atomic(uint8_t) in_port1;
_Atomic(uint8_t) shift0;
_Atomic(uint8_t) shift1;
_Atomic(uint8_t) shift_offset;
_Atomic(uint8_t) out_port3, out_port5, last_out_port3, last_out_port5;

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

void* emulator_thread(void* arg)
{
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
  return NULL;
}

void events_thread(struct draw_screen_args* args)
{
  unsigned char* fb = args->game_memory;
  SDL_Texture* texture = args->texture;
  SDL_Renderer* renderer = args->renderer;
  uint32_t* b = args->buffer;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      uint8_t ascii;
      switch (event.type) {
        case SDL_KEYDOWN:
          ascii = (uint8_t) (event.key.keysym.sym & 0xff);
          switch (ascii) {
            case KEY_COIN:
                in_port1 |= 0x1;
                break;
            case KEY_P1_LEFT:
                in_port1 |= 0x20;
                break;
            case KEY_P1_RIGHT:
                in_port1 |= 0x40;
                break;
            case KEY_P1_FIRE:
                in_port1 |= 0x10;
                break;
            case KEY_P1_START:
                in_port1 |= 0x04;
                break;
          }
          printf("scan code: %d state: %d in_port1: %d\n", event.key.keysym.sym, event.key.state, in_port1);
          break;
        case SDL_KEYUP:
          ascii = (unsigned char) (event.key.keysym.sym & 0xff);
          switch (ascii) {
            case KEY_COIN:
                in_port1 &= ~0x1;
                break;
            case KEY_P1_LEFT:
                in_port1 &= ~0x20;
                break;
            case KEY_P1_RIGHT:
                in_port1 &= ~0x40;
                break;
            case KEY_P1_FIRE:
                in_port1 &= ~0x10;
                break;
            case KEY_P1_START:
                in_port1 &= ~0x04;
                break;
          }
          printf("scan code: %d state: %d in_port1: %d\n", event.key.keysym.sym, event.key.state, in_port1);
          break;
      }
    }
    int i, j;
    // Walk the width of the screen by the byte
    // 224 / 8 = 28
    for (i=0; i< 224; i++)
    {
        // Walk the width of the screen by the byte
        // 32 / 8 = 28
        for (j = 0; j < 256; j+= 8)
        {
            int p;
            //Read the first 1-bit pixel
            // divide by 8 because there are 8 pixels
            // in a byte
            int bit_offset = i*(256/8) + j/8;
            unsigned char pix = fb[(i*(256/8)) + j/8];
            int offset = i*8 + j*8;
            
            //That makes 8 output vertical pixels
            // we need to do a vertical flip
            // so j needs to start at the last line
            // and advance backward through the buffer
            int byte_offset = 8 * bit_offset;
            uint32_t *p1 = (uint32_t *)(&b[byte_offset]);
            for (p=0; p<8; p++)
            {
                if ( 0!= (pix & (1<<p)))
                    *p1 = RGB_ON;
                else
                    *p1 = RGB_OFF;
                p1++;
            }
        }
    }
    SDL_UpdateTexture(texture, NULL, b, 256 * sizeof(uint32_t));
    SDL_RenderClear(renderer);
    SDL_RenderCopy(renderer, texture, NULL, NULL);
    SDL_RenderPresent(renderer);
}

int main()
{
  if (SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO) != 0) {
      SDL_Log("Unable to initialize SDL: %s", SDL_GetError());
      return 1;
  }
  emulator = calloc(sizeof(State8080), 1);
  emulator->memory = malloc(16 * 0x1000);
  read_rom_into_memory(emulator, 0, files, 4);
  printf("Finished reading roms into memory\n");
  printf("%s\n", SDL_GetPlatform());

    SDL_Window *window;
    // Create an application window with the following settings:
    window = SDL_CreateWindow(
        "An SDL2 window",                  // window title
        SDL_WINDOWPOS_UNDEFINED,           // initial x position
        SDL_WINDOWPOS_UNDEFINED,           // initial y position
        256,                               // width, in pixels
        224,                               // height, in pixels
        SDL_WINDOW_OPENGL                  // flags - see below
    );

    // Check that the window was successfully created
    if (window == NULL) {
        // In the case that the window could not be made...
        printf("Could not create window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetWindowTitle(window, "Testing");
    pthread_t emu_thread;
    int rc = pthread_create(&emu_thread, NULL, emulator_thread, NULL);
    if (rc != 0) {
      printf("Could not create pthread, exited with code %d", rc);
    }
    uint32_t pixels[256 * 224];
    SDL_Texture* texture;
    SDL_Renderer* renderer;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    texture = SDL_CreateTexture(renderer,
      SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 256, 224);

    struct draw_screen_args args = {
      .game_memory = &emulator->memory[0x2400],
      .buffer = &pixels,
      .renderer = renderer,
      .texture = texture,
    };
    while (1) {
      events_thread(&args);
      usleep(16000);
    }

  return 0;
}


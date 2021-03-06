#include "emulator.h"
#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include <assert.h>
#include "SDL.h"

#define RGB_ON  0xFFFFFFFF
#define RGB_OFF 0xFF000000

#define KEY_COIN 'c'
#define KEY_P1_LEFT 'a'
#define KEY_P1_RIGHT 's'
#define KEY_P1_FIRE ' '
#define KEY_P1_START '1'
#define KEY_PAUSE 'p'

struct State8080* emulator;

struct draw_screen_args {
  unsigned char* game_memory;
  SDL_Renderer* renderer;
  SDL_Texture* texture;
};

char* files[] = {"invaders.h", "invaders.g", "invaders.f", "invaders.e"};

int should_quit = 0;
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

void events_thread(void *arg)
{
    struct draw_screen_args* args = (struct draw_screen_args *) arg;
    unsigned char* fb = args->game_memory;
    uint32_t buffer[256][224];
    SDL_Texture* texture = args->texture;
    SDL_Renderer* renderer = args->renderer;
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
      uint8_t ascii;
      switch (event.type) {
        case SDL_QUIT:
            should_quit = 1;
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
          break;
      }
    }
    for (int row = 0; row < 256; row += 8) {
        for (int col = 0; col < 224; col++) {

            uint32_t rotated_row = col;
            uint32_t rotated_col = 256 - row;
            uint32_t offset = rotated_row == 0 ? rotated_col/8 : (rotated_row * 256 - (256 - rotated_col))/8;
            assert(offset < 7168);
            unsigned char pixel = fb[offset];

            // Loop through the pixel's 8 bits.  Each bit corresponds to a 32 bit int
            // in the SDL screen buffer.  Since the pixel is rotated counter clockwise
            // by 90 degress the first bit in the pixel is actually the last row in the
            // SDL screen buffer.
            // 
            // 1 byte pixel (before it's rotated)
            //
            // +---+---+---+---+---+---+---+---+
            // |   |   |   |   |   |   |   |   |
            // |   |   |   |   |   |   |   |   |
            // +-+-+---+---+---+---+---+---+---+
            //   ^
            //   |
            //   |
            //   |
            //   +
            //
            // The 8 32 bit ints for a given pixel
            //          +----+
            //  row     |    |
            //          +----+
            //  row + 1 |    |
            //          +----+
            //  row + 2 |    |
            //          +----+
            //  row + 3 |    |
            //          +----+
            //  row + 4 |    |
            //          +----+
            //  row + 5 |    |
            //          +----+
            //  row + 6 |    |
            //          +----+
            //   +----> |    |
            //  row + 7 +----+
            for (int i = 0; i < 8; i++) {
                if ((pixel & (1 << (7 - i))) != 0)
                    buffer[row + i][col] = RGB_ON;
                else
                    buffer[row + i][col] = RGB_OFF;
            }
        }
    }
    SDL_UpdateTexture(texture, NULL, buffer, 224 * sizeof(uint32_t));
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

    SDL_Window *window;
    // Create an application window with the following settings:
    window = SDL_CreateWindow(
        "An SDL2 window",                  // window title
        SDL_WINDOWPOS_UNDEFINED,           // initial x position
        SDL_WINDOWPOS_UNDEFINED,           // initial y position
        224,                               // width, in pixels
        256,                               // height, in pixels
        SDL_WINDOW_OPENGL                  // flags - see below
    );

    // Check that the window was successfully created
    if (window == NULL) {
        // In the case that the window could not be made...
        printf("Could not create window: %s\n", SDL_GetError());
        return 1;
    }
    SDL_SetWindowTitle(window, "Space Invaders");
    pthread_t emu_thread;
    int rc = pthread_create(&emu_thread, NULL, emulator_thread, NULL);
    if (rc != 0) {
      printf("Could not create pthread, exited with code %d", rc);
    }
    SDL_Texture* texture;
    SDL_Renderer* renderer;
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_PRESENTVSYNC);
    texture = SDL_CreateTexture(renderer,
      SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STATIC, 224, 256);

    struct draw_screen_args args = {
      .game_memory = &emulator->memory[0x2400],
      .renderer    = renderer,
      .texture     = texture,
    };
    while (! should_quit) {
      events_thread(&args);
      usleep(16000);
    }

    return 0;
}

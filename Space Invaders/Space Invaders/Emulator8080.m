//
//  Emulator8080.m
//  Space Invaders
//
//  Created by CI Server on 9/25/18.
//  Copyright © 2018 ddelnano. All rights reserved.
//

#import "Emulator8080.h"
#import "disassembler.h"

@implementation Emulator8080

- (id) init
{
    State8080* state = calloc(sizeof(State8080), 1);
    self.state = state;
    self.state->memory = malloc(16 * 0x1000);
    
    self.in_port1 = 0;
    
    [self loadFile: @"invaders.h" IntoMemoryAt:0];
    [self loadFile: @"invaders.g" IntoMemoryAt:0x800];
    [self loadFile: @"invaders.f" IntoMemoryAt:0x1000];
    [self loadFile: @"invaders.e" IntoMemoryAt:0x1800];
    
    return self;
}

- (void) loadFile:(NSString *)file IntoMemoryAt:(uint16)location
{
    NSBundle* bundle = [NSBundle mainBundle];
    NSString* path = [bundle pathForResource:file ofType:nil];
    
    NSData* data = [[NSData alloc] initWithContentsOfFile:path];
    uint8_t* mem = &self.state->memory[location];
    memcpy(mem, [data bytes], [data length]);
}

- (uint8_t) InPort: (uint8_t) port
{
    unsigned char a = 0;
    switch (port) {
        case 0:
            return 0xf;
            break;
        case 1:
            return self.in_port1;
            break;
        // TODO: Only matters for player 2.
        case 2:
            return 0;
            break;
        case 3:
        {
            uint16_t v = (self.shift1<<8) | self.shift0;
            a = ((v >> (8-self.shift_offset)) & 0xff);
        }
            break;
    }
    
    return a;
}

-(void) OutSpaceInvaders:(uint8_t) port value:(uint8_t)value
{
    switch(port)
    {
        case 2:
            self.shift_offset = value & 0x7;
            break;
        case 3:
            self.out_port3 = value;
            break;
        case 4:
            self.shift0 = self.shift1;
            self.shift1 = value;
            break;
        case 5:
            self.out_port5 = value;
            break;
    }
}

- (void) KeyDown:(uint8_t) key
{
    switch (key) {
        case KEY_COIN:
            self.in_port1 |= 0x1;
            break;
        case KEY_P1_LEFT:
            self.in_port1 |= 0x20;
            break;
        case KEY_P1_RIGHT:
            self.in_port1 |= 0x40;
            break;
        case KEY_P1_FIRE:
            self.in_port1 |= 0x10;
            break;
        case KEY_P1_START:
            self.in_port1 |= 0x2;
            break;
    }
    // bit 3 is always 1
//    self.in_port1 |= 0x8;
    NSLog(@"%d key pressedand IN Port 1: %d\n", key, self.in_port1);
}

- (void) KeyUp:(uint8_t) key
{
    switch (key) {
        case KEY_COIN:
            self.in_port1 &= ~0x1;
            break;
        case KEY_P1_LEFT:
            self.in_port1 &= ~0x20;
            break;
        case KEY_P1_RIGHT:
            self.in_port1 &= ~0x40;
            break;
        case KEY_P1_FIRE:
            self.in_port1 &= ~0x10;
            break;
        case KEY_P1_START:
            self.in_port1 &= ~0x2;
            break;
    }
    
    NSLog(@"%d key released and IN Port 1: %d\n", key, self.in_port1);
}

- (void) startEmulation
{
    NSThread* thread = [[NSThread alloc]initWithTarget:self selector:@selector(runCpu) object:nil];
    
    [thread start];
}

- (void) runCpu
{
    @autoreleasepool {
        double timeSinceInterrupt = 0.0;
        int whichInterrupt = 1;
        
        while (true) {
            double now = [self timeusec];
            if (self.state->int_enable && timeSinceInterrupt > 8000) {
                generate_interrupt(self.state, whichInterrupt);
                timeSinceInterrupt = 0;
                
                whichInterrupt = whichInterrupt == 1 ? 2 : 1;
            }
            
            int instruction_catch_up = 1;
            int cycles = 0;
            while (instruction_catch_up > cycles) {
            
                unsigned char* opcode = &self.state->memory[self.state->pc];
                
                // IN instruction
                if (*opcode == 0xdb) {
                    self.state->pc += 2;
                    NSLog(@"In port: %d", opcode[1]);
                    self.state->a = [self InPort:opcode[1]];
                    cycles += 3;
                } else if (*opcode == 0xd3) {
                    
                    // TODO: Add sounds later.
                    [self OutSpaceInvaders:opcode[1] value:self.state->a];
                    self.state->pc += 2;
                    cycles += 3;
                    [self PlaySounds];
                } else {
                    cycles += emulate(self.state);
                }
                double duration = [self timeusec] - now;
                timeSinceInterrupt += duration;
            }
        }
    }
}

- (unsigned char *) screenBuffer
{
    return &self.state->memory[0x2400];
}

-(void) PlaySounds
{
    if (self.out_port3 != self.last_out_port3)
    {
        if ( (self.out_port3 & 0x1) && !(self.last_out_port3 & 0x1))
        {
            //start UFO
            self.ufo = [NSSound soundNamed:@"0.wav"];
            [self.ufo setLoops:YES];
            [self.ufo play];
        }
        else if ( !(self.out_port3 & 0x1) && (self.last_out_port3 & 0x1))
        {
            //stop UFO
            if (self.ufo)
            {
                [self.ufo stop];
                self.ufo = NULL;
            }
        }
        
        if ( (self.out_port3 & 0x2) && !(self.last_out_port3 & 0x2))
            [[NSSound soundNamed:@"1.wav"] play];
        
        if ( (self.out_port3 & 0x4) && !(self.last_out_port3 & 0x4))
            [[NSSound soundNamed:@"2.wav"] play];
        
        if ( (self.out_port3 & 0x8) && !(self.last_out_port3 & 0x8))
            [[NSSound soundNamed:@"3.wav"] play];
        
        self.last_out_port3 = self.out_port3;
    }
    if (self.out_port5 != self.last_out_port5)
    {
        if ( (self.out_port5 & 0x1) && !(self.last_out_port5 & 0x1))
            [[NSSound soundNamed:@"4.wav"] play];
        
        if ( (self.out_port5 & 0x2) && !(self.last_out_port5 & 0x2))
            [[NSSound soundNamed:@"5.wav"] play];
        
        if ( (self.out_port5 & 0x4) && !(self.last_out_port5 & 0x4))
            [[NSSound soundNamed:@"6.wav"] play];
        
        if ( (self.out_port5 & 0x8) && !(self.last_out_port5 & 0x8))
            [[NSSound soundNamed:@"7.wav"] play];
        
        if ( (self.out_port5 & 0x10) && !(self.last_out_port5 & 0x10))
            [[NSSound soundNamed:@"8.wav"] play];
        
        self.last_out_port5 = self.out_port5;
    }
}

- (double) timeusec
{
    struct timeval time;
    gettimeofday(&time, NULL);
    return ((double)time.tv_sec * 1E6) + ((double)time.tv_usec);
}

@end

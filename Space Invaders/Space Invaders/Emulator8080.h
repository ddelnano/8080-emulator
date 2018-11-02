//
//  Emulator8080.h
//  Space Invaders
//
//  Created by CI Server on 9/25/18.
//  Copyright © 2018 ddelnano. All rights reserved.
//

#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#import "emulator.h"
#include <time.h>

#define KEY_COIN 'c'
#define KEY_P1_LEFT 'a'
#define KEY_P1_RIGHT 's'
#define KEY_P1_FIRE ' '
#define KEY_P1_START '1'
#define KEY_PAUSE 'p'

@interface Emulator8080 : NSObject
{
    double      lastTimer;
    double      nextInterrupt;
    int         whichInterrupt;
}

@property State8080* state;
@property uint8_t     in_port1;
@property uint8_t     shift0;         //LSB of Space Invader's external shift hardware
@property uint8_t     shift1;         //MSB
@property uint8_t     shift_offset;         // offset for external shift hardware
@property uint8_t     out_port3, out_port5, last_out_port3, last_out_port5;

@property NSSound     *ufo;

- (void) KeyDown:(uint8_t) key;
- (void) KeyUp:(uint8_t) key;

- (void) startEmulation;
- (unsigned char *) screenBuffer;
@end

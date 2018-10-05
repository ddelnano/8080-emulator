//
//  Emulator8080.h
//  Space Invaders
//
//  Created by CI Server on 9/25/18.
//  Copyright © 2018 ddelnano. All rights reserved.
//

#import <Foundation/Foundation.h>
#import "emulator.h"
#include <time.h>

#define KEY_COIN 'c'
#define KEY_P1_LEFT 'a'
#define KEY_P1_RIGHT 's'
#define KEY_P1_FIRE ' '
#define KEY_P1_START '1'
#define KEY_PAUSE 'p'

@interface Emulator8080 : NSObject

@property State8080* state;
@property uint8_t in_port1;

- (void) KeyDown:(uint8_t) key;
- (void) KeyUp:(uint8_t) key;

- (void) startEmulation;
- (unsigned char *) screenBuffer;
@end

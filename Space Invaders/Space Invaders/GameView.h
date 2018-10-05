//
//  GameView.h
//  Space Invaders
//
//  Created by CI Server on 9/25/18.
//  Copyright © 2018 ddelnano. All rights reserved.
//

#import <Cocoa/Cocoa.h>
#import "Emulator8080.h"

@interface GameView : NSView

@property Emulator8080* emulator;
@property void* emulatorBuffer;
@property CGContextRef contextRef;

@end

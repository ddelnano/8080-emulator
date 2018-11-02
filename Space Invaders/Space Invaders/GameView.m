//
//  GameView.m
//  Space Invaders
//
//  Created by CI Server on 9/25/18.
//  Copyright © 2018 ddelnano. All rights reserved.
//

#import "GameView.h"
#import "emulator.h"

#define RGB_ON   0xFFFFFFFFL
#define RGB_OFF 0x00000000L

@implementation GameView

- (void)drawRect:(NSRect)dirtyRect {
    int i, j;
    
    //Translate the 1-bit space invaders frame buffer into
    // my 32bpp RGB bitmap.  We have to rotate and
    // flip the image as we go.
    //
    unsigned char *b = (unsigned char *)self.emulatorBuffer;
    unsigned char *fb = [self.emulator screenBuffer];
    for (i=0; i< 224; i++)
    {
        for (j = 0; j < 256; j+= 8)
        {
            int p;
            //Read the first 1-bit pixel
            // divide by 8 because there are 8 pixels
            // in a byte
            unsigned char pix = fb[(i*(256/8)) + j/8];
            
            //That makes 8 output vertical pixels
            // we need to do a vertical flip
            // so j needs to start at the last line
            // and advance backward through the buffer
            int offset = (255-j)*(224*4) + (i*4);
            unsigned int*p1 = (unsigned int*)(&b[offset]);
            for (p=0; p<8; p++)
            {
                if ( 0!= (pix & (1<<p)))
                    *p1 = RGB_ON;
                else
                    *p1 = RGB_OFF;
                p1-=224;  //next line
            }
        }
    }
        
        
    CGContextRef myContext = [[NSGraphicsContext currentContext] graphicsPort];
    CGImageRef ir = CGBitmapContextCreateImage(self.contextRef);
    CGContextDrawImage(myContext, self.bounds, ir);
    CGImageRelease(ir);
    // TODO: maybe i need this?
//    [super drawRect:dirtyRect];
}



- (void)awakeFromNib
{
    [super awakeFromNib];
    
    self.emulatorBuffer = malloc(4 * 224 * 256);
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    self.contextRef = CGBitmapContextCreate(self.emulatorBuffer, 224, 256, 8, 4 * 224, colorSpace, kCGImageAlphaNoneSkipFirst);
    self.emulator = [[Emulator8080 alloc] init];
    
    NSTimer* timer = [NSTimer timerWithTimeInterval:0.016 target:self selector:@selector(timerFired) userInfo:nil repeats:YES];
    [[NSRunLoop currentRunLoop] addTimer:timer forMode:NSDefaultRunLoopMode];
    [self.emulator startEmulation];
    
    NSLog(@"Testing this");
}

- (void) timerFired
{
    [self setNeedsDisplay:YES];
}

//************************************
#pragma mark Key Handling
//************************************

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void) keyDown: (NSEvent *) event
{
    NSString* key = [event characters];
    uint8_t* ascii = (uint8_t *)[[key dataUsingEncoding:NSASCIIStringEncoding allowLossyConversion:YES] bytes];
    [self.emulator KeyDown:*ascii];
}

- (void) keyUp: (NSEvent *) event
{
    NSString* key = [event characters];
    uint8_t* ascii = (uint8_t *)[[key dataUsingEncoding:NSASCIIStringEncoding allowLossyConversion:YES] bytes];
    [self.emulator KeyUp:*ascii];
}

@end

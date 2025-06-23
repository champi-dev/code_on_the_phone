#import <Cocoa/Cocoa.h>

@interface TestView : NSView
@end

@implementation TestView

- (void)drawRect:(NSRect)dirtyRect {
    [[NSColor purpleColor] setFill];
    NSRectFill(dirtyRect);
    
    // Draw some text
    NSString *text = @"Quantum Terminal Test - Window is visible!";
    NSDictionary *attrs = @{
        NSFontAttributeName: [NSFont systemFontOfSize:24],
        NSForegroundColorAttributeName: [NSColor whiteColor]
    };
    
    NSSize textSize = [text sizeWithAttributes:attrs];
    NSPoint textPoint = NSMakePoint((dirtyRect.size.width - textSize.width) / 2,
                                   (dirtyRect.size.height - textSize.height) / 2);
    
    [text drawAtPoint:textPoint withAttributes:attrs];
}

- (BOOL)acceptsFirstResponder {
    return YES;
}

- (void)keyDown:(NSEvent *)event {
    NSLog(@"Key pressed: %@", event.characters);
}

@end

int main(int argc, const char *argv[]) {
    @autoreleasepool {
        // Create the application
        NSApplication *app = [NSApplication sharedApplication];
        [app setActivationPolicy:NSApplicationActivationPolicyRegular];
        
        // Create window
        NSRect frame = NSMakeRect(0, 0, 800, 600);
        NSWindow *window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:(NSWindowStyleMaskTitled |
                                                                 NSWindowStyleMaskClosable |
                                                                 NSWindowStyleMaskMiniaturizable |
                                                                 NSWindowStyleMaskResizable)
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];
        
        [window setTitle:@"Quantum Terminal Test"];
        [window center];
        
        // Create and set content view
        TestView *view = [[TestView alloc] initWithFrame:frame];
        [window setContentView:view];
        [window makeFirstResponder:view];
        
        // Create menu bar
        NSMenu *menubar = [[NSMenu alloc] init];
        NSMenuItem *appMenuItem = [[NSMenuItem alloc] init];
        [menubar addItem:appMenuItem];
        [app setMainMenu:menubar];
        
        NSMenu *appMenu = [[NSMenu alloc] init];
        NSString *quitTitle = @"Quit Test Window";
        NSMenuItem *quitMenuItem = [[NSMenuItem alloc] initWithTitle:quitTitle
                                                              action:@selector(terminate:)
                                                       keyEquivalent:@"q"];
        [appMenu addItem:quitMenuItem];
        [appMenuItem setSubmenu:appMenu];
        
        // Show window
        [window makeKeyAndOrderFront:nil];
        [app activateIgnoringOtherApps:YES];
        
        NSLog(@"Window should be visible now!");
        
        // Run the app
        [app run];
    }
    return 0;
}
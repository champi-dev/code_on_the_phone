//
//  main.m
//  QuantumTerminal
//
//  Entry point that works for both macOS and iOS/Mac Catalyst
//

#import <TargetConditionals.h>

#if TARGET_OS_OSX && !TARGET_OS_MACCATALYST
// macOS
int main(int argc, const char *argv[]);
#else
// iOS/Mac Catalyst
#import <UIKit/UIKit.h>
#import "AppDelegate.h"

int main(int argc, char * argv[]) {
    @autoreleasepool {
        return UIApplicationMain(argc, argv, nil, NSStringFromClass([AppDelegate class]));
    }
}
#endif
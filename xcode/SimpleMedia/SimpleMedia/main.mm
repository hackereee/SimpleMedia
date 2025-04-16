//
//  main.m
//  SimpleMedia
//
//  Created by 东东杨 on 2025/3/2.
//

#import <Cocoa/Cocoa.h>
#import <bx/bx.h>
#import <bx/uint32_t.h>
#import <bx/handlealloc.h>
#import <bgfx/bgfx.h>


#if  BX_PLATFORM_OSX

#define WINDOW_WIDTH 800
#define WINDOW_HEIGHT 600

#import <entry.h>
#import "tools.h"
namespace  entry{
bx::HandleAlloc windHandleAlloc(1);
    
void init(){
    entry::windowId = windHandleAlloc.alloc();
    if(UINT16_MAX != entry::windowId){
        NSRect screenFrame = [[NSScreen mainScreen] frame];
        CGFloat x = (screenFrame.size.width - WINDOW_WIDTH) / 2;
        CGFloat y = (screenFrame.size.height - WINDOW_HEIGHT) / 2;
        NSRect frame = NSMakeRect(x, y, WINDOW_WIDTH, WINDOW_HEIGHT);
        
        NSWindow* osxWindow = [[NSWindow alloc] initWithContentRect:frame
        styleMask:(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable | NSWindowStyleMaskResizable | NSWindowStyleMaskMiniaturizable)
        backing:NSBackingStoreBuffered
        defer:NO];
        [osxWindow setTitle:@"SimpleMedia"];
        void* castWindow = BRIDGE_VOID_CAST(osxWindow);
        CGDirectDisplayID mainDisplayId = CGMainDisplayID();
        entry::displayHandle =  reinterpret_cast<void *>(static_cast<uintptr_t>(mainDisplayId));
        entry::displayType = bgfx::NativeWindowHandleType::Enum::Default;
        entry::rendererType = bgfx::RendererType::Enum::Metal;
    }
}

void openFile(){
    NSString* path = [[NSBundle mainBundle] pathForResource:@"a" ofType:@"mp4"];
    const char* cPath = [path UTF8String];
    entry::play(cPath);

    
}

int main(int argc, const char * argv[]) {
    @autoreleasepool {
        entry::init();
    }
    return 0;
}
#endif

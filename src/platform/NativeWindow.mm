#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include "NativeWindow.h"
#include "MetalView.h"

// ─── Window Delegate ─────────────────────────────────────────

@interface DCWindowDelegate : NSObject <NSWindowDelegate>
@property (nonatomic, assign) dc::platform::NativeWindow* owner;
@end

@implementation DCWindowDelegate

- (void)windowDidResize:(NSNotification*)notification
{
    if (_owner && _owner->onResize)
    {
        _owner->onResize (_owner->getWidth(), _owner->getHeight());
    }
}

- (BOOL)windowShouldClose:(NSWindow*)sender
{
    if (_owner && _owner->onClose)
    {
        _owner->onClose();
    }
    return YES;
}

@end

// ─── NativeWindow Implementation ─────────────────────────────

namespace dc
{
namespace platform
{

NativeWindow::NativeWindow (const std::string& title, int width, int height)
{
    @autoreleasepool
    {
        NSRect frame = NSMakeRect (0, 0, width, height);
        NSUInteger styleMask = NSWindowStyleMaskTitled
                             | NSWindowStyleMaskClosable
                             | NSWindowStyleMaskMiniaturizable
                             | NSWindowStyleMaskResizable;

        NSWindow* window = [[NSWindow alloc] initWithContentRect:frame
                                                       styleMask:styleMask
                                                         backing:NSBackingStoreBuffered
                                                           defer:NO];

        [window setTitle:[NSString stringWithUTF8String:title.c_str()]];
        [window center];
        [window setMinSize:NSMakeSize (800, 600)];

        // Create delegate
        DCWindowDelegate* delegate = [[DCWindowDelegate alloc] init];
        delegate.owner = this;
        [window setDelegate:delegate];
        windowDelegate = (__bridge_retained void*) delegate;

        nativeWindow = (__bridge_retained void*) window;

        // Create MetalView as content view
        metalView = std::make_unique<MetalView> ((__bridge void*) window);
    }
}

NativeWindow::~NativeWindow()
{
    @autoreleasepool
    {
        metalView.reset();

        if (nativeWindow)
        {
            NSWindow* window = (__bridge_transfer NSWindow*) nativeWindow;
            [window setDelegate:nil];
            [window close];
            nativeWindow = nullptr;
        }

        if (windowDelegate)
        {
            (void) (__bridge_transfer DCWindowDelegate*) windowDelegate;
            windowDelegate = nullptr;
        }
    }
}

void NativeWindow::show()
{
    @autoreleasepool
    {
        NSWindow* window = (__bridge NSWindow*) nativeWindow;
        [window makeKeyAndOrderFront:nil];
    }
}

void NativeWindow::close()
{
    @autoreleasepool
    {
        NSWindow* window = (__bridge NSWindow*) nativeWindow;
        [window close];
    }
}

int NativeWindow::getWidth() const
{
    @autoreleasepool
    {
        NSWindow* window = (__bridge NSWindow*) nativeWindow;
        return static_cast<int> ([window contentView].frame.size.width);
    }
}

int NativeWindow::getHeight() const
{
    @autoreleasepool
    {
        NSWindow* window = (__bridge NSWindow*) nativeWindow;
        return static_cast<int> ([window contentView].frame.size.height);
    }
}

float NativeWindow::getScaleFactor() const
{
    @autoreleasepool
    {
        NSWindow* window = (__bridge NSWindow*) nativeWindow;
        return static_cast<float> ([window backingScaleFactor]);
    }
}

} // namespace platform
} // namespace dc

#import <Cocoa/Cocoa.h>
#import <Metal/Metal.h>
#import <MetalKit/MetalKit.h>
#include "MetalView.h"
#include "graphics/core/Event.h"

// ─── Helper: Convert NSEvent to dc events ────────────────────

static dc::gfx::MouseEvent makeMouseEvent (NSEvent* event, NSView* view)
{
    NSPoint loc = [view convertPoint:[event locationInWindow] fromView:nil];
    // Flip Y: NSView origin is bottom-left, we want top-left
    loc.y = view.bounds.size.height - loc.y;

    dc::gfx::MouseEvent e;
    e.x = static_cast<float> (loc.x);
    e.y = static_cast<float> (loc.y);
    e.clickCount = static_cast<int> ([event clickCount]);

    NSUInteger flags = [event modifierFlags];
    e.shift   = (flags & NSEventModifierFlagShift) != 0;
    e.control = (flags & NSEventModifierFlagControl) != 0;
    e.alt     = (flags & NSEventModifierFlagOption) != 0;
    e.command = (flags & NSEventModifierFlagCommand) != 0;

    return e;
}

static dc::gfx::KeyEvent makeKeyEvent (NSEvent* event)
{
    dc::gfx::KeyEvent e;
    e.keyCode = [event keyCode];

    NSString* chars = [event characters];
    if (chars.length > 0)
        e.character = [chars characterAtIndex:0];

    NSString* charsNoMod = [event charactersIgnoringModifiers];
    if (charsNoMod.length > 0)
        e.unmodifiedCharacter = [charsNoMod characterAtIndex:0];

    NSUInteger flags = [event modifierFlags];
    e.shift   = (flags & NSEventModifierFlagShift) != 0;
    e.control = (flags & NSEventModifierFlagControl) != 0;
    e.alt     = (flags & NSEventModifierFlagOption) != 0;
    e.command = (flags & NSEventModifierFlagCommand) != 0;

    return e;
}

// ─── Custom MTKView that handles events ──────────────────────

@interface DCMetalView : MTKView <MTKViewDelegate>
@property (nonatomic, assign) dc::platform::MetalView* owner;
@end

@implementation DCMetalView

- (BOOL)acceptsFirstResponder { return YES; }
- (BOOL)canBecomeKeyView { return YES; }

- (void)drawInMTKView:(nonnull MTKView*)view
{
    if (_owner && _owner->onFrame)
        _owner->onFrame();
}

- (void)mtkView:(nonnull MTKView*)view drawableSizeWillChange:(CGSize)size
{
    // Renderer handles size from drawable each frame
}

// ─── Mouse Events ────────────────────────────────────────────

- (void)mouseDown:(NSEvent*)event
{
    if (_owner && _owner->onMouseDown)
        _owner->onMouseDown (makeMouseEvent (event, self));
}

- (void)mouseDragged:(NSEvent*)event
{
    if (_owner && _owner->onMouseDrag)
        _owner->onMouseDrag (makeMouseEvent (event, self));
}

- (void)mouseUp:(NSEvent*)event
{
    if (_owner && _owner->onMouseUp)
        _owner->onMouseUp (makeMouseEvent (event, self));
}

- (void)mouseMoved:(NSEvent*)event
{
    if (_owner && _owner->onMouseMove)
        _owner->onMouseMove (makeMouseEvent (event, self));
}

- (void)rightMouseDown:(NSEvent*)event
{
    auto e = makeMouseEvent (event, self);
    e.rightButton = true;
    if (_owner && _owner->onMouseDown)
        _owner->onMouseDown (e);
}

- (void)rightMouseDragged:(NSEvent*)event
{
    auto e = makeMouseEvent (event, self);
    e.rightButton = true;
    if (_owner && _owner->onMouseDrag)
        _owner->onMouseDrag (e);
}

- (void)rightMouseUp:(NSEvent*)event
{
    auto e = makeMouseEvent (event, self);
    e.rightButton = true;
    if (_owner && _owner->onMouseUp)
        _owner->onMouseUp (e);
}

- (void)scrollWheel:(NSEvent*)event
{
    if (_owner && _owner->onWheel)
    {
        dc::gfx::WheelEvent e;
        NSPoint loc = [self convertPoint:[event locationInWindow] fromView:nil];
        loc.y = self.bounds.size.height - loc.y;
        e.x = static_cast<float> (loc.x);
        e.y = static_cast<float> (loc.y);
        e.deltaX = static_cast<float> ([event scrollingDeltaX]);
        e.deltaY = static_cast<float> ([event scrollingDeltaY]);
        e.isPixelDelta = [event hasPreciseScrollingDeltas];

        NSUInteger flags = [event modifierFlags];
        e.shift   = (flags & NSEventModifierFlagShift) != 0;
        e.control = (flags & NSEventModifierFlagControl) != 0;
        e.alt     = (flags & NSEventModifierFlagOption) != 0;
        e.command = (flags & NSEventModifierFlagCommand) != 0;

        _owner->onWheel (e);
    }
}

// ─── Keyboard Events ─────────────────────────────────────────

- (void)keyDown:(NSEvent*)event
{
    if (_owner && _owner->onKeyDown)
        _owner->onKeyDown (makeKeyEvent (event));
}

- (void)keyUp:(NSEvent*)event
{
    if (_owner && _owner->onKeyUp)
        _owner->onKeyUp (makeKeyEvent (event));
}

- (void)flagsChanged:(NSEvent*)event
{
    // Could dispatch modifier-only changes if needed
}

@end

// ─── MetalView Implementation ────────────────────────────────

namespace dc
{
namespace platform
{

MetalView::MetalView (void* nsWindow)
{
    @autoreleasepool
    {
        NSWindow* window = (__bridge NSWindow*) nsWindow;

        // Create Metal device
        id<MTLDevice> device = MTLCreateSystemDefaultDevice();
        metalDevice = (__bridge_retained void*) device;

        // Create command queue
        id<MTLCommandQueue> queue = [device newCommandQueue];
        metalCommandQueue = (__bridge_retained void*) queue;

        // Create MTKView
        NSRect frame = [[window contentView] bounds];
        DCMetalView* view = [[DCMetalView alloc] initWithFrame:frame device:device];
        view.owner = this;
        view.delegate = view;
        view.preferredFramesPerSecond = 60;
        view.enableSetNeedsDisplay = NO; // Continuous rendering
        view.colorPixelFormat = MTLPixelFormatBGRA8Unorm;
        view.depthStencilPixelFormat = MTLPixelFormatInvalid;
        view.sampleCount = 1;
        view.layer.magnificationFilter = kCAFilterNearest;

        // Accept mouse move events
        [window setAcceptsMouseMovedEvents:YES];

        // Set as content view
        [window setContentView:view];
        [window makeFirstResponder:view];

        mtkView = (__bridge_retained void*) view;
    }
}

MetalView::~MetalView()
{
    @autoreleasepool
    {
        if (mtkView)
        {
            DCMetalView* view = (__bridge_transfer DCMetalView*) mtkView;
            view.owner = nullptr;
            view.delegate = nil;
            [view removeFromSuperview];
            mtkView = nullptr;
        }

        if (metalCommandQueue)
        {
            (void) (__bridge_transfer id<MTLCommandQueue>) metalCommandQueue;
            metalCommandQueue = nullptr;
        }

        if (metalDevice)
        {
            (void) (__bridge_transfer id<MTLDevice>) metalDevice;
            metalDevice = nullptr;
        }
    }
}

void* MetalView::getMetalDevice() const { return metalDevice; }
void* MetalView::getMetalCommandQueue() const { return metalCommandQueue; }
void* MetalView::getMTKView() const { return mtkView; }

int MetalView::getDrawableWidth() const
{
    @autoreleasepool
    {
        MTKView* view = (__bridge MTKView*) mtkView;
        return static_cast<int> (view.drawableSize.width);
    }
}

int MetalView::getDrawableHeight() const
{
    @autoreleasepool
    {
        MTKView* view = (__bridge MTKView*) mtkView;
        return static_cast<int> (view.drawableSize.height);
    }
}

float MetalView::getContentsScale() const
{
    @autoreleasepool
    {
        MTKView* view = (__bridge MTKView*) mtkView;
        return static_cast<float> ([[view window] backingScaleFactor]);
    }
}

void MetalView::setPaused (bool paused)
{
    @autoreleasepool
    {
        MTKView* view = (__bridge MTKView*) mtkView;
        [view setPaused:paused];
    }
}

} // namespace platform
} // namespace dc

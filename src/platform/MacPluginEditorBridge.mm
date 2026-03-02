#include "MacPluginEditorBridge.h"

#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#include "include/core/SkBitmap.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"
#include "dc/foundation/assert.h"

namespace dc
{

MacPluginEditorBridge::MacPluginEditorBridge (void* nativeWindowHandle)
    : nsWindow (nativeWindowHandle)
{
}

MacPluginEditorBridge::~MacPluginEditorBridge()
{
    closeEditor();
}

void MacPluginEditorBridge::openEditor (dc::PluginInstance* plugin)
{
    closeEditor();

    if (plugin == nullptr)
        return;

    editor_ = plugin->createEditor();
    if (editor_ == nullptr)
        return;

    auto [w, h] = editor_->getPreferredSize();
    nativeWidth = w;
    nativeHeight = h;

    // Create an off-screen NSWindow to host the plugin editor.
    // The plugin editor needs to be in a real NSWindow for CGWindowListCreateImage.
    NSRect frame = NSMakeRect (-10000, -10000, w, h);
    NSWindow* window = [[NSWindow alloc]
        initWithContentRect:frame
                  styleMask:NSWindowStyleMaskBorderless
                    backing:NSBackingStoreBuffered
                      defer:NO];
    [window setReleasedWhenClosed:NO];
    [window setLevel:NSNormalWindowLevel];
    [window orderFront:nil];

    NSView* contentView = [window contentView];
    editorNSWindow = (__bridge void*) window;
    editorNSView = (__bridge void*) contentView;

    // Attach the plugin editor to the NSView
    editor_->attachToWindow (editorNSView);

    // Get the CGWindowID for capture
    cgWindowId = (uint32_t) [window windowNumber];

    dc_log ("[MacPluginEditorBridge] Opened editor: %dx%d cgWindowId=%u",
            nativeWidth, nativeHeight, cgWindowId);

    damaged = true;
}

void MacPluginEditorBridge::closeEditor()
{
    if (editor_)
    {
        editor_->detach();
        editor_.reset();
    }

    if (editorNSWindow != nullptr)
    {
        NSWindow* window = (__bridge NSWindow*) editorNSWindow;
        [window close];
        editorNSWindow = nullptr;
        editorNSView = nullptr;
    }

    nativeWidth = 0;
    nativeHeight = 0;
    cgWindowId = 0;
    cachedImage = nullptr;
    damaged = true;
}

bool MacPluginEditorBridge::isOpen() const
{
    return editor_ != nullptr;
}

int MacPluginEditorBridge::getNativeWidth() const
{
    return nativeWidth;
}

int MacPluginEditorBridge::getNativeHeight() const
{
    return nativeHeight;
}

void MacPluginEditorBridge::setTargetBounds (int /*x*/, int /*y*/, int /*w*/, int /*h*/)
{
    // On macOS, the editor stays at native size off-screen.
    // Scaling is handled by the paint method in PluginViewWidget.
}

bool MacPluginEditorBridge::hasDamage()
{
    // macOS doesn't have XDamage-style notification.
    // Always report damage so we capture fresh frames.
    damaged = true;
    return true;
}

sk_sp<SkImage> MacPluginEditorBridge::capture()
{
    if (cgWindowId == 0)
        return cachedImage;

    // Capture the plugin editor window via CGWindowListCreateImage
    CGRect bounds = CGRectNull;  // Capture entire window
    CGImageRef cgImage = CGWindowListCreateImage (
        bounds,
        kCGWindowListOptionIncludingWindow,
        cgWindowId,
        kCGWindowImageBoundsIgnoreFraming | kCGWindowImageNominalResolution);

    if (cgImage == nullptr)
        return cachedImage;

    size_t width = CGImageGetWidth (cgImage);
    size_t height = CGImageGetHeight (cgImage);

    if (width == 0 || height == 0)
    {
        CGImageRelease (cgImage);
        return cachedImage;
    }

    // Convert CGImage to SkImage via SkBitmap
    SkBitmap bitmap;
    SkImageInfo info = SkImageInfo::Make (
        static_cast<int> (width), static_cast<int> (height),
        kBGRA_8888_SkColorType, kPremul_SkAlphaType);
    bitmap.allocPixels (info);

    // Draw CGImage into our pixel buffer
    CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();
    CGContextRef ctx = CGBitmapContextCreate (
        bitmap.getPixels(),
        width, height,
        8,
        bitmap.rowBytes(),
        colorSpace,
        kCGImageAlphaPremultipliedFirst | kCGBitmapByteOrder32Little);

    CGContextDrawImage (ctx, CGRectMake (0, 0, width, height), cgImage);

    CGContextRelease (ctx);
    CGColorSpaceRelease (colorSpace);
    CGImageRelease (cgImage);

    bitmap.setImmutable();
    cachedImage = bitmap.asImage();
    damaged = false;

    return cachedImage;
}

bool MacPluginEditorBridge::isCompositing() const
{
    return cgWindowId != 0;
}

dc::PluginEditor* MacPluginEditorBridge::getEditor() const
{
    return editor_.get();
}

} // namespace dc

#endif // __APPLE__

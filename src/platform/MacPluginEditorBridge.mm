#include "MacPluginEditorBridge.h"

#if defined(__APPLE__)

#import <Cocoa/Cocoa.h>
#import <CoreGraphics/CoreGraphics.h>
#include "include/core/SkBitmap.h"
#include "include/core/SkImage.h"
#include "include/core/SkImageInfo.h"

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

void MacPluginEditorBridge::openEditor (juce::AudioPluginInstance* plugin)
{
    closeEditor();

    if (plugin == nullptr)
        return;

    editor = plugin->createEditorIfNeeded();
    if (editor == nullptr)
        return;

    nativeWidth = editor->getWidth();
    nativeHeight = editor->getHeight();

    // Create a holder component and add the editor as a child
    holder = std::make_unique<juce::Component>();
    holder->setSize (nativeWidth, nativeHeight);
    holder->addAndMakeVisible (editor);

    // Add to desktop as an off-screen window so it renders but isn't visible
    holder->addToDesktop (juce::ComponentPeer::windowIsTemporary);

    // Position off-screen initially
    holder->setTopLeftPosition (-10000, -10000);

    // Get the CGWindowID for capture
    if (auto* peer = holder->getPeer())
    {
        auto* nsView = (NSView*) peer->getNativeHandle();
        if (nsView != nil && [nsView window] != nil)
        {
            cgWindowId = (uint32_t) [[nsView window] windowNumber];
        }
    }

    damaged = true;
}

void MacPluginEditorBridge::closeEditor()
{
    if (holder)
    {
        holder->removeFromDesktop();
        holder.reset();
    }
    editor = nullptr;
    nativeWidth = 0;
    nativeHeight = 0;
    cgWindowId = 0;
    cachedImage = nullptr;
    damaged = true;
}

bool MacPluginEditorBridge::isOpen() const
{
    return editor != nullptr;
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

juce::AudioProcessorEditor* MacPluginEditorBridge::getEditor() const
{
    return editor;
}

} // namespace dc

#endif // __APPLE__

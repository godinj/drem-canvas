#pragma once

#include <functional>
#include <string>

namespace dc
{
namespace platform
{

class MetalView;

class NativeWindow
{
public:
    NativeWindow (const std::string& title, int width, int height);
    ~NativeWindow();

    void show();
    void close();

    int getWidth() const;
    int getHeight() const;
    float getScaleFactor() const;

    MetalView* getMetalView() const { return metalView.get(); }

    // Resize callback
    std::function<void (int, int)> onResize;

    // Close callback
    std::function<void()> onClose;

    void* getNativeHandle() const { return nativeWindow; }

private:
    void* nativeWindow = nullptr;       // NSWindow*
    void* windowDelegate = nullptr;     // NSWindowDelegate
    std::unique_ptr<MetalView> metalView;
};

} // namespace platform
} // namespace dc

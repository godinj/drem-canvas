#pragma once

#include "graphics/core/Event.h"
#include <functional>
#include <string>

struct GLFWwindow;

namespace dc
{
namespace platform
{

class GlfwWindow
{
public:
    GlfwWindow (const std::string& title, int width, int height);
    ~GlfwWindow();

    // Non-copyable
    GlfwWindow (const GlfwWindow&) = delete;
    GlfwWindow& operator= (const GlfwWindow&) = delete;

    GLFWwindow* getHandle() const { return window; }

    int getWidth() const { return width; }
    int getHeight() const { return height; }
    float getScale() const { return scale; }

    void show();
    void pollEvents();
    bool shouldClose() const;

    // Callbacks (same pattern as MetalView)
    std::function<void()> onFrame;
    std::function<void (int, int)> onResize;
    std::function<void()> onClose;
    std::function<void (const gfx::MouseEvent&)> onMouseDown;
    std::function<void (const gfx::MouseEvent&)> onMouseUp;
    std::function<void (const gfx::MouseEvent&)> onMouseMove;
    std::function<void (const gfx::MouseEvent&)> onMouseDrag;
    std::function<void (const gfx::KeyEvent&)> onKeyDown;
    std::function<void (const gfx::KeyEvent&)> onKeyUp;
    std::function<void (const gfx::WheelEvent&)> onWheel;

private:
    static void framebufferSizeCallback (GLFWwindow* w, int width, int height);
    static void windowCloseCallback (GLFWwindow* w);
    static void mouseButtonCallback (GLFWwindow* w, int button, int action, int mods);
    static void cursorPosCallback (GLFWwindow* w, double xpos, double ypos);
    static void scrollCallback (GLFWwindow* w, double xoffset, double yoffset);
    static void keyCallback (GLFWwindow* w, int key, int scancode, int action, int mods);
    static void charCallback (GLFWwindow* w, unsigned int codepoint);

    // Convert GLFW special key to macOS virtual key code (VimEngine compatibility)
    static uint16_t glfwKeyToMacKeyCode (int glfwKey);

    GLFWwindow* window = nullptr;
    int width = 0;
    int height = 0;
    float scale = 1.0f;

    // Track mouse state for drag detection
    bool mousePressed = false;
    double lastMouseX = 0.0;
    double lastMouseY = 0.0;

    // Pending character from charCallback for next keyCallback
    char32_t pendingChar = 0;
};

} // namespace platform
} // namespace dc

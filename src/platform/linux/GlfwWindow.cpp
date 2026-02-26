#include "GlfwWindow.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <stdexcept>

namespace dc
{
namespace platform
{

GlfwWindow::GlfwWindow (const std::string& title, int w, int h)
    : width (w), height (h)
{
    if (!glfwInit())
        throw std::runtime_error ("Failed to initialize GLFW");

    // No OpenGL — Vulkan manages its own context
    glfwWindowHint (GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint (GLFW_RESIZABLE, GLFW_TRUE);

    window = glfwCreateWindow (width, height, title.c_str(), nullptr, nullptr);
    if (!window)
    {
        glfwTerminate();
        throw std::runtime_error ("Failed to create GLFW window");
    }

    // Store this pointer for static callbacks
    glfwSetWindowUserPointer (window, this);

    // Query content scale (HiDPI)
    float xScale, yScale;
    glfwGetWindowContentScale (window, &xScale, &yScale);
    scale = xScale;

    // Query framebuffer size (may differ from window size on HiDPI)
    int fbWidth, fbHeight;
    glfwGetFramebufferSize (window, &fbWidth, &fbHeight);

    // Set callbacks
    glfwSetFramebufferSizeCallback (window, framebufferSizeCallback);
    glfwSetWindowCloseCallback (window, windowCloseCallback);
    glfwSetMouseButtonCallback (window, mouseButtonCallback);
    glfwSetCursorPosCallback (window, cursorPosCallback);
    glfwSetScrollCallback (window, scrollCallback);
    glfwSetKeyCallback (window, keyCallback);
    glfwSetCharCallback (window, charCallback);
}

GlfwWindow::~GlfwWindow()
{
    if (window)
        glfwDestroyWindow (window);
    glfwTerminate();
}

void GlfwWindow::show()
{
    glfwShowWindow (window);
}

void GlfwWindow::pollEvents()
{
    glfwPollEvents();
}

bool GlfwWindow::shouldClose() const
{
    return glfwWindowShouldClose (window);
}

// ─── Static callbacks ────────────────────────────────────────────────────────

void GlfwWindow::framebufferSizeCallback (GLFWwindow* w, int newWidth, int newHeight)
{
    auto* self = static_cast<GlfwWindow*> (glfwGetWindowUserPointer (w));

    // Update window size in logical coordinates
    int logicalW, logicalH;
    glfwGetWindowSize (w, &logicalW, &logicalH);
    self->width = logicalW;
    self->height = logicalH;

    // Update scale
    float xScale, yScale;
    glfwGetWindowContentScale (w, &xScale, &yScale);
    self->scale = xScale;

    if (self->onResize)
        self->onResize (logicalW, logicalH);
}

void GlfwWindow::windowCloseCallback (GLFWwindow* w)
{
    auto* self = static_cast<GlfwWindow*> (glfwGetWindowUserPointer (w));
    if (self->onClose)
        self->onClose();
}

void GlfwWindow::mouseButtonCallback (GLFWwindow* w, int button, int action, int mods)
{
    auto* self = static_cast<GlfwWindow*> (glfwGetWindowUserPointer (w));

    double xpos, ypos;
    glfwGetCursorPos (w, &xpos, &ypos);

    gfx::MouseEvent event;
    event.x = static_cast<float> (xpos);
    event.y = static_cast<float> (ypos);
    event.rightButton = (button == GLFW_MOUSE_BUTTON_RIGHT);
    event.shift = (mods & GLFW_MOD_SHIFT) != 0;
    event.control = (mods & GLFW_MOD_CONTROL) != 0;
    event.alt = (mods & GLFW_MOD_ALT) != 0;
    event.command = (mods & GLFW_MOD_SUPER) != 0;

    if (action == GLFW_PRESS)
    {
        // Detect double-click: same button, within time and distance thresholds
        double now = glfwGetTime();
        double dx = xpos - self->lastClickX;
        double dy = ypos - self->lastClickY;
        double dist = dx * dx + dy * dy;

        if (button == self->lastClickButton
            && (now - self->lastClickTime) < doubleClickMaxSeconds
            && dist < doubleClickMaxDistance * doubleClickMaxDistance)
        {
            self->currentClickCount++;
        }
        else
        {
            self->currentClickCount = 1;
        }

        self->lastClickTime = now;
        self->lastClickX = xpos;
        self->lastClickY = ypos;
        self->lastClickButton = button;

        event.clickCount = self->currentClickCount;
        self->mousePressed = true;
        self->lastMouseX = xpos;
        self->lastMouseY = ypos;
        if (self->onMouseDown)
            self->onMouseDown (event);
    }
    else if (action == GLFW_RELEASE)
    {
        event.clickCount = self->currentClickCount;
        self->mousePressed = false;
        if (self->onMouseUp)
            self->onMouseUp (event);
    }
}

void GlfwWindow::cursorPosCallback (GLFWwindow* w, double xpos, double ypos)
{
    auto* self = static_cast<GlfwWindow*> (glfwGetWindowUserPointer (w));

    self->lastMouseX = xpos;
    self->lastMouseY = ypos;

    gfx::MouseEvent event;
    event.x = static_cast<float> (xpos);
    event.y = static_cast<float> (ypos);

    if (self->mousePressed)
    {
        if (self->onMouseDrag)
            self->onMouseDrag (event);
    }
    else
    {
        if (self->onMouseMove)
            self->onMouseMove (event);
    }
}

void GlfwWindow::scrollCallback (GLFWwindow* w, double xoffset, double yoffset)
{
    auto* self = static_cast<GlfwWindow*> (glfwGetWindowUserPointer (w));

    gfx::WheelEvent event;
    event.x = static_cast<float> (self->lastMouseX);
    event.y = static_cast<float> (self->lastMouseY);
    event.deltaX = static_cast<float> (xoffset);
    event.deltaY = static_cast<float> (yoffset);
    event.isPixelDelta = false;

    if (self->onWheel)
        self->onWheel (event);
}

void GlfwWindow::keyCallback (GLFWwindow* w, int key, int /*scancode*/, int action, int mods)
{
    auto* self = static_cast<GlfwWindow*> (glfwGetWindowUserPointer (w));

    if (action == GLFW_PRESS || action == GLFW_REPEAT)
    {
        uint16_t macKeyCode = glfwKeyToMacKeyCode (key);

        // On X11, GLFW fires keyCallback BEFORE charCallback, so we
        // derive characters directly from GLFW key codes for letters,
        // digits, and space to avoid ordering issues.
        char32_t ch = 0;
        bool charKnown = false;

        if (key >= GLFW_KEY_A && key <= GLFW_KEY_Z)
        {
            bool shift = (mods & GLFW_MOD_SHIFT) != 0;
            ch = static_cast<char32_t> (key - GLFW_KEY_A + (shift ? 'A' : 'a'));
            charKnown = true;
        }
        else if (key >= GLFW_KEY_0 && key <= GLFW_KEY_9 && ! (mods & GLFW_MOD_SHIFT))
        {
            ch = static_cast<char32_t> ('0' + (key - GLFW_KEY_0));
            charKnown = true;
        }
        else if (key == GLFW_KEY_SPACE)
        {
            ch = ' ';
            charKnown = true;
        }

        if (charKnown || macKeyCode != 0)
        {
            // Known character or special key — dispatch immediately
            self->pendingKeyDown = false;

            gfx::KeyEvent event;
            event.keyCode = macKeyCode;
            event.character = ch;
            event.unmodifiedCharacter = ch;
            event.shift = (mods & GLFW_MOD_SHIFT) != 0;
            event.control = (mods & GLFW_MOD_CONTROL) != 0;
            event.alt = (mods & GLFW_MOD_ALT) != 0;
            event.command = (mods & GLFW_MOD_SUPER) != 0;
            event.isRepeat = (action == GLFW_REPEAT);

            if (self->onKeyDown)
                self->onKeyDown (event);
        }
        else
        {
            // Printable key with unknown character (symbols, shifted digits) —
            // defer to charCallback which will provide the actual character.
            self->pendingKeyDown = true;
            self->pendingKeyCode = macKeyCode;
            self->pendingMods = mods;
            self->pendingIsRepeat = (action == GLFW_REPEAT);
        }
    }
    else if (action == GLFW_RELEASE)
    {
        gfx::KeyEvent event;
        event.keyCode = glfwKeyToMacKeyCode (key);
        event.shift = (mods & GLFW_MOD_SHIFT) != 0;
        event.control = (mods & GLFW_MOD_CONTROL) != 0;
        event.alt = (mods & GLFW_MOD_ALT) != 0;
        event.command = (mods & GLFW_MOD_SUPER) != 0;

        if (self->onKeyUp)
            self->onKeyUp (event);
    }
}

void GlfwWindow::charCallback (GLFWwindow* w, unsigned int codepoint)
{
    auto* self = static_cast<GlfwWindow*> (glfwGetWindowUserPointer (w));
    auto ch = static_cast<char32_t> (codepoint);

    if (self->pendingKeyDown)
    {
        // Combine stashed key info with the character and dispatch
        gfx::KeyEvent event;
        event.keyCode = self->pendingKeyCode;
        event.character = ch;
        event.unmodifiedCharacter = ch;
        event.shift = (self->pendingMods & GLFW_MOD_SHIFT) != 0;
        event.control = (self->pendingMods & GLFW_MOD_CONTROL) != 0;
        event.alt = (self->pendingMods & GLFW_MOD_ALT) != 0;
        event.command = (self->pendingMods & GLFW_MOD_SUPER) != 0;
        event.isRepeat = self->pendingIsRepeat;

        self->pendingKeyDown = false;

        if (self->onKeyDown)
            self->onKeyDown (event);
    }
}

// ─── Key code translation ────────────────────────────────────────────────────
// Maps GLFW key codes to the macOS virtual key codes that VimEngine expects
// (see src/vim/VimEngine.cpp:31-41). Non-special keys use 0 (character-based).

uint16_t GlfwWindow::glfwKeyToMacKeyCode (int glfwKey)
{
    switch (glfwKey)
    {
        case GLFW_KEY_ESCAPE:    return 0x35;
        case GLFW_KEY_ENTER:     return 0x24;
        case GLFW_KEY_TAB:       return 0x30;
        case GLFW_KEY_SPACE:     return 0x31;
        case GLFW_KEY_BACKSPACE: return 0x33;
        case GLFW_KEY_UP:        return 0x7E;
        case GLFW_KEY_DOWN:      return 0x7D;
        case GLFW_KEY_LEFT:      return 0x7B;
        case GLFW_KEY_RIGHT:     return 0x7C;
        default:                 return 0;
    }
}

} // namespace platform
} // namespace dc

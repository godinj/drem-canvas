#include <JuceHeader.h>

#if defined(__APPLE__)
#include "platform/NativeWindow.h"
#include "platform/MetalView.h"
#include "platform/EventBridge.h"
#include "graphics/rendering/MetalBackend.h"
#elif defined(__linux__)
#include "platform/linux/GlfwWindow.h"
#include "platform/linux/VulkanBackend.h"
#endif

#include "graphics/rendering/GpuBackend.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/core/EventDispatch.h"
#include "ui/AppController.h"

class DremCanvasApplication : public juce::JUCEApplication
#if defined(__linux__)
                            , private juce::Timer
#endif
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }

    // JUCE's X11-based instance detection is unreliable on Wayland —
    // it falsely detects a running instance and silently exits.
    // Disable it on Linux; GLFW manages the actual window.
    bool moreThanOneInstanceAllowed() override
    {
#if defined(__linux__)
        return true;
#else
        return false;
#endif
    }

    void initialise (const juce::String& /*commandLine*/) override
    {
#if defined(__APPLE__)
        // Create native Metal window (replaces JUCE MainWindow)
        nativeWindow = std::make_unique<dc::platform::NativeWindow> ("Drem Canvas", 1280, 800);

        // Create Skia Metal backend
        gpuBackend = std::make_unique<dc::gfx::MetalBackend> (*nativeWindow->getMetalView());

        // Create renderer
        renderer = std::make_unique<dc::gfx::Renderer> (*gpuBackend);

        // Create root widget (AppController)
        appController = std::make_unique<dc::ui::AppController>();
        appController->setRenderer (renderer.get());

        // Set root widget bounds to window size
        float w = static_cast<float> (nativeWindow->getWidth());
        float h = static_cast<float> (nativeWindow->getHeight());
        appController->setBounds (dc::gfx::Rect (0, 0, w, h));

        // Create event dispatch with root widget
        eventDispatch = std::make_unique<dc::gfx::EventDispatch> (*appController);

        // Wire event bridge (connects MTKView events to EventDispatch)
        eventBridge = std::make_unique<dc::platform::EventBridge> (
            *nativeWindow->getMetalView(), *eventDispatch);

        // Wire frame callback — Renderer drives the full frame loop
        nativeWindow->getMetalView()->onFrame = [this]()
        {
            renderer->renderFrame (*appController);
        };

        // Handle window resize
        nativeWindow->onResize = [this] (int newW, int newH)
        {
            appController->setBounds (dc::gfx::Rect (0, 0,
                static_cast<float> (newW), static_cast<float> (newH)));
            renderer->forceNextFrame();
        };

        // Handle window close
        nativeWindow->onClose = [this]()
        {
            systemRequestedQuit();
        };

        // Initialize the audio engine and all UI
        appController->initialise();

        nativeWindow->show();

#elif defined(__linux__)
        // Create GLFW window (Vulkan-ready, no GL context)
        glfwWindow = std::make_unique<dc::platform::GlfwWindow> ("Drem Canvas", 1280, 800);

        // Set window icon for X11 (Wayland uses the .desktop file instead)
        auto exeDir = juce::File::getSpecialLocation (
            juce::File::currentExecutableFile).getParentDirectory();
        auto iconFile = exeDir.getChildFile ("drem-canvas.png");
        if (iconFile.existsAsFile())
            glfwWindow->setWindowIcon (iconFile.getFullPathName().toStdString());

        // Create Skia Vulkan backend
        gpuBackend = std::make_unique<dc::platform::VulkanBackend> (
            glfwWindow->getHandle(),
            glfwWindow->getWidth(), glfwWindow->getHeight(),
            glfwWindow->getScale());

        // Create renderer
        renderer = std::make_unique<dc::gfx::Renderer> (*gpuBackend);

        // Create root widget (AppController)
        appController = std::make_unique<dc::ui::AppController>();
        appController->setRenderer (renderer.get());

        // Set root widget bounds to window size
        float w = static_cast<float> (glfwWindow->getWidth());
        float h = static_cast<float> (glfwWindow->getHeight());
        appController->setBounds (dc::gfx::Rect (0, 0, w, h));

        // Create event dispatch with root widget
        eventDispatch = std::make_unique<dc::gfx::EventDispatch> (*appController);

        // Wire GLFW callbacks to EventDispatch
        glfwWindow->onMouseDown = [this] (const dc::gfx::MouseEvent& e)
        { eventDispatch->dispatchMouseDown (e); };

        glfwWindow->onMouseUp = [this] (const dc::gfx::MouseEvent& e)
        { eventDispatch->dispatchMouseUp (e); };

        glfwWindow->onMouseMove = [this] (const dc::gfx::MouseEvent& e)
        { eventDispatch->dispatchMouseMove (e); };

        glfwWindow->onMouseDrag = [this] (const dc::gfx::MouseEvent& e)
        { eventDispatch->dispatchMouseDrag (e); };

        glfwWindow->onKeyDown = [this] (const dc::gfx::KeyEvent& e)
        { eventDispatch->dispatchKeyDown (e); };

        glfwWindow->onKeyUp = [this] (const dc::gfx::KeyEvent& e)
        { eventDispatch->dispatchKeyUp (e); };

        glfwWindow->onWheel = [this] (const dc::gfx::WheelEvent& e)
        { eventDispatch->dispatchWheel (e); };

        // Handle window resize
        glfwWindow->onResize = [this] (int newW, int newH)
        {
            auto* vulkan = static_cast<dc::platform::VulkanBackend*> (gpuBackend.get());
            vulkan->resize (newW, newH, glfwWindow->getScale());
            appController->setBounds (dc::gfx::Rect (0, 0,
                static_cast<float> (newW), static_cast<float> (newH)));
            renderer->forceNextFrame();
        };

        // Handle window close
        glfwWindow->onClose = [this]()
        {
            systemRequestedQuit();
        };

        // Pass GLFW window handle for X11 reparenting of plugin editors
        appController->setGlfwWindow (glfwWindow->getHandle());

        // Initialize the audio engine and all UI
        appController->initialise();

        glfwWindow->show();

        // Start render loop via juce::Timer (60 Hz)
        startTimerHz (60);
#endif
    }

    void shutdown() override
    {
#if defined(__linux__)
        stopTimer();
#endif
        // Tear down in reverse order
        appController.reset();
#if defined(__APPLE__)
        eventBridge.reset();
#endif
        eventDispatch.reset();
        renderer.reset();
        gpuBackend.reset();
#if defined(__APPLE__)
        nativeWindow.reset();
#elif defined(__linux__)
        glfwWindow.reset();
#endif
    }

    void systemRequestedQuit() override
    {
        quit();
    }

#if defined(__linux__)
    void timerCallback() override
    {
        glfwWindow->pollEvents();
        renderer->renderFrame (*appController);
    }
#endif

private:
#if defined(__APPLE__)
    std::unique_ptr<dc::platform::NativeWindow> nativeWindow;
    std::unique_ptr<dc::platform::EventBridge> eventBridge;
#elif defined(__linux__)
    std::unique_ptr<dc::platform::GlfwWindow> glfwWindow;
#endif
    std::unique_ptr<dc::gfx::GpuBackend> gpuBackend;
    std::unique_ptr<dc::gfx::Renderer> renderer;
    std::unique_ptr<dc::gfx::EventDispatch> eventDispatch;
    std::unique_ptr<dc::ui::AppController> appController;
};

START_JUCE_APPLICATION (DremCanvasApplication)

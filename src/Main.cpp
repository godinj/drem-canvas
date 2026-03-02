#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <string>
#include <thread>

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
#include "model/Track.h"
#include "plugins/PluginHost.h"
#include "plugins/SpatialScanCache.h"
#include "ui/AppController.h"

// ─── macOS entry point ──────────────────────────────────────────

#if defined(__APPLE__)

// Implemented in platform/AppMain.mm — sets up NSApplication,
// creates an NSApp delegate that calls the supplied callbacks
// for applicationDidFinishLaunching and applicationWillTerminate,
// then runs [NSApp run].
extern void dc_runNSApplication (std::function<void()> onReady,
                                 std::function<void()> onTerminate);

int main (int argc, char* argv[])
{
    // Parse command-line flags
    bool smokeMode = false;
    std::string loadPath;
    int expectTracks = -1;
    int expectPlugins = -1;
    int scanTrack = -1;
    int scanSlot = -1;
    bool noSpatialCache = false;
    int expectSpatialParamsGt = -1;
    bool browserScan = false;
    int expectKnownPluginsGt = -1;
    bool capturePluginState = false;
    int processFrames = 0;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg (argv[i]);
        if (arg == "--smoke")
            smokeMode = true;
        else if (arg == "--load" && i + 1 < argc)
            loadPath = argv[++i];
        else if (arg == "--expect-tracks" && i + 1 < argc)
            expectTracks = std::atoi (argv[++i]);
        else if (arg == "--expect-plugins" && i + 1 < argc)
            expectPlugins = std::atoi (argv[++i]);
        else if (arg == "--scan-plugin" && i + 2 < argc)
        {
            scanTrack = std::atoi (argv[++i]);
            scanSlot = std::atoi (argv[++i]);
        }
        else if (arg == "--no-spatial-cache")
            noSpatialCache = true;
        else if (arg == "--expect-spatial-params-gt" && i + 1 < argc)
            expectSpatialParamsGt = std::atoi (argv[++i]);
        else if (arg == "--browser-scan")
            browserScan = true;
        else if (arg == "--expect-known-plugins-gt" && i + 1 < argc)
            expectKnownPluginsGt = std::atoi (argv[++i]);
        else if (arg == "--capture-plugin-state")
            capturePluginState = true;
        else if (arg == "--process-frames" && i + 1 < argc)
            processFrames = std::atoi (argv[++i]);
    }

    // Pointers kept alive across the NSApplication run loop.
    // Created in onReady, torn down in onTerminate.
    std::unique_ptr<dc::platform::NativeWindow> nativeWindow;
    std::unique_ptr<dc::gfx::GpuBackend>        gpuBackend;
    std::unique_ptr<dc::gfx::Renderer>           renderer;
    std::unique_ptr<dc::ui::AppController>       appController;
    std::unique_ptr<dc::gfx::EventDispatch>      eventDispatch;
    std::unique_ptr<dc::platform::EventBridge>   eventBridge;

    dc_runNSApplication (
        // ── applicationDidFinishLaunching ──
        [&]()
        {
            // Create native Metal window
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

            // Wire frame callback — MTKView display-link drives rendering.
            // tick() is called every frame for meter updates and message queue processing.
            nativeWindow->getMetalView()->onFrame = [&]()
            {
                appController->tick();
                renderer->renderFrame (*appController);
            };

            // Handle window resize
            nativeWindow->onResize = [&] (int newW, int newH)
            {
                appController->setBounds (dc::gfx::Rect (0, 0,
                    static_cast<float> (newW), static_cast<float> (newH)));
                renderer->forceNextFrame();
            };

            // Handle window close — terminate the NSApplication run loop
            nativeWindow->onClose = [&]()
            {
                // dc_runNSApplication is expected to call [NSApp terminate:nil]
                // when the window close callback fires. The implementation can
                // also simply post NSApplicationWillTerminateNotification.
                // For now we leave the mechanics to the .mm helper.
            };

            // Pass native window handle for plugin editor compositing
            appController->setNativeWindowHandle (nativeWindow->getNativeHandle());

            // Initialize the audio engine and all UI
            appController->initialise();

            nativeWindow->show();

            // Smoke mode: one tick to prove the loop works, then exit
            if (smokeMode)
            {
                appController->tick();

                // Load a session if requested
                if (! loadPath.empty())
                {
                    appController->loadSessionFromDirectory (std::filesystem::path (loadPath));

                    // Drain a few ticks — plugins load asynchronously
                    for (int i = 0; i < 10; ++i)
                        appController->tick();
                }

                int exitCode = 0;

                // Validate track count
                if (expectTracks >= 0)
                {
                    int actual = appController->getProject().getNumTracks();
                    if (actual != expectTracks)
                    {
                        std::cerr << "FAIL: expected " << expectTracks
                                  << " tracks, got " << actual << "\n";
                        exitCode = 1;
                    }
                }

                // Validate plugin count
                if (expectPlugins >= 0)
                {
                    int totalPlugins = 0;
                    auto& project = appController->getProject();
                    for (int t = 0; t < project.getNumTracks(); ++t)
                    {
                        dc::Track track (project.getTrack (t));
                        totalPlugins += track.getNumPlugins();
                    }
                    if (totalPlugins != expectPlugins)
                    {
                        std::cerr << "FAIL: expected " << expectPlugins
                                  << " plugins, got " << totalPlugins << "\n";
                        exitCode = 1;
                    }
                }

                // Capture and print plugin state for test fixture generation
                if (capturePluginState)
                {
                    auto& project = appController->getProject();
                    for (int t = 0; t < project.getNumTracks(); ++t)
                    {
                        auto& pluginChain = appController->getTrackPluginChain (t);
                        for (int p = 0; p < static_cast<int> (pluginChain.size()); ++p)
                        {
                            if (pluginChain[static_cast<size_t> (p)].plugin != nullptr)
                            {
                                auto stateStr = dc::PluginHost::savePluginState (
                                    *pluginChain[static_cast<size_t> (p)].plugin);
                                std::cout << "PLUGIN_STATE track=" << t
                                          << " slot=" << p
                                          << " state=" << stateStr << "\n";
                            }
                        }
                    }
                }

                // Run audio frames to exercise the process() path
                if (processFrames > 0)
                {
                    for (int f = 0; f < processFrames; ++f)
                    {
                        appController->tick();
                        std::this_thread::sleep_for (std::chrono::milliseconds (10));
                    }
                }

                // Run spatial scan if requested
                if (scanTrack >= 0 && scanSlot >= 0)
                {
                    auto& project = appController->getProject();
                    if (scanTrack >= project.getNumTracks())
                    {
                        std::cerr << "FAIL: scan track " << scanTrack << " out of range\n";
                        exitCode = 1;
                    }
                    else
                    {
                        dc::Track track (project.getTrack (scanTrack));
                        if (scanSlot >= track.getNumPlugins())
                        {
                            std::cerr << "FAIL: scan slot " << scanSlot << " out of range\n";
                            exitCode = 1;
                        }
                        else
                        {
                            auto pluginNode = track.getPlugin (scanSlot);
                            auto desc = dc::PluginHost::descriptionFromPropertyTree (pluginNode);

                            // Get the plugin instance from the track's plugin chain
                            // The plugin was already instantiated during loadSessionFromDirectory
                            auto* pluginViewWidget = appController->getPluginViewWidget();
                            if (pluginViewWidget == nullptr)
                            {
                                std::cerr << "FAIL: no PluginViewWidget available\n";
                                exitCode = 1;
                            }
                            else
                            {
                                // Invalidate cache if requested
                                if (noSpatialCache)
                                {
                                    auto fileOrId = pluginNode.getProperty (dc::IDs::pluginFileOrIdentifier).getStringOr ("");
                                    dc::SpatialScanCache::invalidate (fileOrId, 0, 0);
                                }

                                // Open plugin editor and trigger scan
                                appController->openPluginEditor (scanTrack, scanSlot);

                                // Drain ticks to allow plugin editor to open
                                for (int t = 0; t < 30; ++t)
                                    appController->tick();

                                pluginViewWidget->runSpatialScan();

                                // Poll for scan completion (up to ~10 seconds)
                                bool scanDone = false;
                                for (int t = 0; t < 600 && !scanDone; ++t)
                                {
                                    appController->tick();
                                    scanDone = pluginViewWidget->hasSpatialHints();
                                    if (!scanDone)
                                        std::this_thread::sleep_for (std::chrono::milliseconds (16));
                                }

                                if (!scanDone)
                                {
                                    std::cerr << "FAIL: spatial scan timed out\n";
                                    exitCode = 1;
                                }
                                else
                                {
                                    int paramCount = static_cast<int> (
                                        pluginViewWidget->getSpatialResults().size());
                                    std::cerr << "INFO: spatial scan found " << paramCount
                                              << " parameters\n";

                                    if (expectSpatialParamsGt >= 0 && paramCount <= expectSpatialParamsGt)
                                    {
                                        std::cerr << "FAIL: expected > " << expectSpatialParamsGt
                                                  << " spatial params, got " << paramCount << "\n";
                                        exitCode = 1;
                                    }
                                }
                            }
                        }
                    }
                }

                // Open browser and scan for plugins if requested
                if (browserScan)
                {
                    appController->toggleBrowser();
                    appController->tick();

                    appController->getPluginManager().scanForPlugins();

                    // Drain ticks to let browser refresh
                    for (int i = 0; i < 5; ++i)
                        appController->tick();

                    int knownCount = static_cast<int> (
                        appController->getPluginManager().getKnownPlugins().size());
                    std::cerr << "INFO: plugin scan found " << knownCount << " plugins\n";

                    if (expectKnownPluginsGt >= 0 && knownCount <= expectKnownPluginsGt)
                    {
                        std::cerr << "FAIL: expected > " << expectKnownPluginsGt
                                  << " known plugins, got " << knownCount << "\n";
                        exitCode = 1;
                    }

                    // Close browser
                    appController->toggleBrowser();
                    appController->tick();
                }

                // Teardown
                appController.reset();
                eventBridge.reset();
                eventDispatch.reset();
                renderer.reset();
                gpuBackend.reset();
                nativeWindow.reset();
                std::exit (exitCode);
            }
        },

        // ── applicationWillTerminate ──
        [&]()
        {
            // Tear down in reverse order
            appController.reset();
            eventBridge.reset();
            eventDispatch.reset();
            renderer.reset();
            gpuBackend.reset();
            nativeWindow.reset();
        }
    );

    return 0;
}

// ─── Linux entry point ──────────────────────────────────────────

#elif defined(__linux__)

int main (int argc, char* argv[])
{
    // Parse command-line flags
    bool smokeMode = false;
    std::string loadPath;
    int expectTracks = -1;
    int expectPlugins = -1;
    int scanTrack = -1;
    int scanSlot = -1;
    bool noSpatialCache = false;
    int expectSpatialParamsGt = -1;
    bool browserScan = false;
    int expectKnownPluginsGt = -1;
    bool capturePluginState = false;
    int processFrames = 0;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg (argv[i]);
        if (arg == "--smoke")
            smokeMode = true;
        else if (arg == "--load" && i + 1 < argc)
            loadPath = argv[++i];
        else if (arg == "--expect-tracks" && i + 1 < argc)
            expectTracks = std::atoi (argv[++i]);
        else if (arg == "--expect-plugins" && i + 1 < argc)
            expectPlugins = std::atoi (argv[++i]);
        else if (arg == "--scan-plugin" && i + 2 < argc)
        {
            scanTrack = std::atoi (argv[++i]);
            scanSlot = std::atoi (argv[++i]);
        }
        else if (arg == "--no-spatial-cache")
            noSpatialCache = true;
        else if (arg == "--expect-spatial-params-gt" && i + 1 < argc)
            expectSpatialParamsGt = std::atoi (argv[++i]);
        else if (arg == "--browser-scan")
            browserScan = true;
        else if (arg == "--expect-known-plugins-gt" && i + 1 < argc)
            expectKnownPluginsGt = std::atoi (argv[++i]);
        else if (arg == "--capture-plugin-state")
            capturePluginState = true;
        else if (arg == "--process-frames" && i + 1 < argc)
            processFrames = std::atoi (argv[++i]);
    }

    // Create GLFW window
    auto glfwWindow = std::make_unique<dc::platform::GlfwWindow> ("Drem Canvas", 1280, 800);

    // Set window icon for X11 (Wayland uses the .desktop file instead)
    auto exeDir = std::filesystem::canonical ("/proc/self/exe").parent_path();
    auto iconFile = exeDir / "drem-canvas.png";
    if (std::filesystem::exists (iconFile))
        glfwWindow->setWindowIcon (iconFile.string());

    // Create Skia Vulkan backend
    auto gpuBackend = std::make_unique<dc::platform::VulkanBackend> (
        glfwWindow->getHandle(),
        glfwWindow->getWidth(), glfwWindow->getHeight(),
        glfwWindow->getScale());

    auto renderer = std::make_unique<dc::gfx::Renderer> (*gpuBackend);

    // Create root widget (AppController)
    auto appController = std::make_unique<dc::ui::AppController>();
    appController->setRenderer (renderer.get());

    float w = static_cast<float> (glfwWindow->getWidth());
    float h = static_cast<float> (glfwWindow->getHeight());
    appController->setBounds (dc::gfx::Rect (0, 0, w, h));

    auto eventDispatch = std::make_unique<dc::gfx::EventDispatch> (*appController);

    // Wire GLFW callbacks to EventDispatch
    glfwWindow->onMouseDown = [&] (const dc::gfx::MouseEvent& e)
    { eventDispatch->dispatchMouseDown (e); };

    glfwWindow->onMouseUp = [&] (const dc::gfx::MouseEvent& e)
    { eventDispatch->dispatchMouseUp (e); };

    glfwWindow->onMouseMove = [&] (const dc::gfx::MouseEvent& e)
    { eventDispatch->dispatchMouseMove (e); };

    glfwWindow->onMouseDrag = [&] (const dc::gfx::MouseEvent& e)
    { eventDispatch->dispatchMouseDrag (e); };

    glfwWindow->onKeyDown = [&] (const dc::gfx::KeyEvent& e)
    { eventDispatch->dispatchKeyDown (e); };

    glfwWindow->onKeyUp = [&] (const dc::gfx::KeyEvent& e)
    { eventDispatch->dispatchKeyUp (e); };

    glfwWindow->onWheel = [&] (const dc::gfx::WheelEvent& e)
    { eventDispatch->dispatchWheel (e); };

    // Handle window resize
    glfwWindow->onResize = [&] (int newW, int newH)
    {
        auto* vulkan = static_cast<dc::platform::VulkanBackend*> (gpuBackend.get());
        vulkan->resize (newW, newH, glfwWindow->getScale());
        appController->setBounds (dc::gfx::Rect (0, 0,
            static_cast<float> (newW), static_cast<float> (newH)));
        renderer->forceNextFrame();
    };

    bool shouldQuit = false;
    glfwWindow->onClose = [&]() { shouldQuit = true; };

    // Pass native window handle for plugin editor compositing
    appController->setNativeWindowHandle (glfwWindow->getHandle());

    // Initialize the audio engine and all UI
    appController->initialise();
    glfwWindow->show();

    // Smoke mode: one tick to prove the loop works, then clean exit
    if (smokeMode)
    {
        appController->tick();

        // Load a session if requested
        if (! loadPath.empty())
        {
            appController->loadSessionFromDirectory (std::filesystem::path (loadPath));

            // Drain a few ticks — plugins load asynchronously
            for (int i = 0; i < 10; ++i)
                appController->tick();
        }

        int exitCode = 0;

        // Validate track count
        if (expectTracks >= 0)
        {
            int actual = appController->getProject().getNumTracks();
            if (actual != expectTracks)
            {
                std::cerr << "FAIL: expected " << expectTracks
                          << " tracks, got " << actual << "\n";
                exitCode = 1;
            }
        }

        // Validate plugin count
        if (expectPlugins >= 0)
        {
            int totalPlugins = 0;
            auto& project = appController->getProject();
            for (int t = 0; t < project.getNumTracks(); ++t)
            {
                dc::Track track (project.getTrack (t));
                totalPlugins += track.getNumPlugins();
            }
            if (totalPlugins != expectPlugins)
            {
                std::cerr << "FAIL: expected " << expectPlugins
                          << " plugins, got " << totalPlugins << "\n";
                exitCode = 1;
            }
        }

        // Capture and print plugin state for test fixture generation
        if (capturePluginState)
        {
            auto& project = appController->getProject();
            for (int t = 0; t < project.getNumTracks(); ++t)
            {
                auto& pluginChain = appController->getTrackPluginChain (t);
                for (int p = 0; p < static_cast<int> (pluginChain.size()); ++p)
                {
                    if (pluginChain[static_cast<size_t> (p)].plugin != nullptr)
                    {
                        auto stateStr = dc::PluginHost::savePluginState (
                            *pluginChain[static_cast<size_t> (p)].plugin);
                        std::cout << "PLUGIN_STATE track=" << t
                                  << " slot=" << p
                                  << " state=" << stateStr << "\n";
                    }
                }
            }
        }

        // Run audio frames to exercise the process() path
        if (processFrames > 0)
        {
            for (int f = 0; f < processFrames; ++f)
            {
                appController->tick();
                std::this_thread::sleep_for (std::chrono::milliseconds (10));
            }
        }

        // Run spatial scan if requested
        if (scanTrack >= 0 && scanSlot >= 0)
        {
            auto& project = appController->getProject();
            if (scanTrack >= project.getNumTracks())
            {
                std::cerr << "FAIL: scan track " << scanTrack << " out of range\n";
                exitCode = 1;
            }
            else
            {
                dc::Track track (project.getTrack (scanTrack));
                if (scanSlot >= track.getNumPlugins())
                {
                    std::cerr << "FAIL: scan slot " << scanSlot << " out of range\n";
                    exitCode = 1;
                }
                else
                {
                    auto pluginNode = track.getPlugin (scanSlot);
                    auto desc = dc::PluginHost::descriptionFromPropertyTree (pluginNode);

                    // Get the plugin instance from the track's plugin chain
                    // The plugin was already instantiated during loadSessionFromDirectory
                    auto* pluginViewWidget = appController->getPluginViewWidget();
                    if (pluginViewWidget == nullptr)
                    {
                        std::cerr << "FAIL: no PluginViewWidget available\n";
                        exitCode = 1;
                    }
                    else
                    {
                        // Invalidate cache if requested
                        if (noSpatialCache)
                        {
                            auto fileOrId = pluginNode.getProperty (dc::IDs::pluginFileOrIdentifier).getStringOr ("");
                            dc::SpatialScanCache::invalidate (fileOrId, 0, 0);
                        }

                        // Open plugin editor and trigger scan
                        appController->openPluginEditor (scanTrack, scanSlot);

                        // Drain ticks to allow plugin editor to open
                        for (int t = 0; t < 30; ++t)
                            appController->tick();

                        pluginViewWidget->runSpatialScan();

                        // Poll for scan completion (up to ~10 seconds)
                        bool scanDone = false;
                        for (int t = 0; t < 600 && !scanDone; ++t)
                        {
                            appController->tick();
                            scanDone = pluginViewWidget->hasSpatialHints();
                            if (!scanDone)
                                std::this_thread::sleep_for (std::chrono::milliseconds (16));
                        }

                        if (!scanDone)
                        {
                            std::cerr << "FAIL: spatial scan timed out\n";
                            exitCode = 1;
                        }
                        else
                        {
                            int paramCount = static_cast<int> (
                                pluginViewWidget->getSpatialResults().size());
                            std::cerr << "INFO: spatial scan found " << paramCount
                                      << " parameters\n";

                            if (expectSpatialParamsGt >= 0 && paramCount <= expectSpatialParamsGt)
                            {
                                std::cerr << "FAIL: expected > " << expectSpatialParamsGt
                                          << " spatial params, got " << paramCount << "\n";
                                exitCode = 1;
                            }
                        }
                    }
                }
            }
        }

        // Open browser and scan for plugins if requested
        if (browserScan)
        {
            appController->toggleBrowser();
            appController->tick();

            appController->getPluginManager().scanForPlugins();

            // Drain ticks to let browser refresh
            for (int i = 0; i < 5; ++i)
                appController->tick();

            int knownCount = static_cast<int> (
                appController->getPluginManager().getKnownPlugins().size());
            std::cerr << "INFO: plugin scan found " << knownCount << " plugins\n";

            if (expectKnownPluginsGt >= 0 && knownCount <= expectKnownPluginsGt)
            {
                std::cerr << "FAIL: expected > " << expectKnownPluginsGt
                          << " known plugins, got " << knownCount << "\n";
                exitCode = 1;
            }

            // Close browser
            appController->toggleBrowser();
            appController->tick();
        }

        // Teardown
        appController.reset();
        eventDispatch.reset();
        renderer.reset();
        gpuBackend.reset();
        glfwWindow.reset();
        return exitCode;
    }

    // Main loop at ~60Hz (vsync-paced).
    // GLFW pollEvents handles vsync pacing; tick() drives meter
    // updates and message queue processing each frame.
    while (! shouldQuit && ! glfwWindow->shouldClose())
    {
        glfwWindow->pollEvents();
        appController->tick();
        renderer->renderFrame (*appController);
    }

    // Tear down in reverse order
    appController.reset();
    eventDispatch.reset();
    renderer.reset();
    gpuBackend.reset();
    glfwWindow.reset();

    return 0;
}

#endif

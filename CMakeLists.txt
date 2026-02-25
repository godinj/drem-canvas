cmake_minimum_required(VERSION 3.22)

if(APPLE)
    project(DremCanvas VERSION 0.1.0 LANGUAGES C CXX OBJCXX)
else()
    project(DremCanvas VERSION 0.1.0 LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(libs/JUCE)

# yaml-cpp for YAML session serialization
include(FetchContent)
set(CMAKE_POLICY_VERSION_MINIMUM 3.5 CACHE STRING "" FORCE)
FetchContent_Declare(
    yaml-cpp
    GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
    GIT_TAG 0.8.0
)
set(YAML_CPP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_TOOLS OFF CACHE BOOL "" FORCE)
set(YAML_CPP_BUILD_CONTRIB OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(yaml-cpp)

# ─── Skia ─────────────────────────────────────────────────────
set(SKIA_DIR "${CMAKE_SOURCE_DIR}/libs/skia")
find_library(SKIA_LIB skia PATHS "${SKIA_DIR}/lib" NO_DEFAULT_PATH)
if(NOT SKIA_LIB)
    message(WARNING "Skia not found at ${SKIA_DIR}/lib — run scripts/build_skia.sh first")
endif()

# ─── libpng (required by Skia) ────────────────────────────────
find_package(PNG REQUIRED)

# ─── Platform-specific dependencies ──────────────────────────
if(APPLE)
    find_library(METAL_FRAMEWORK Metal)
    find_library(METALKIT_FRAMEWORK MetalKit)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(CORETEXT_FRAMEWORK CoreText)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)
elseif(UNIX AND NOT APPLE)
    find_package(Vulkan REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    find_package(Threads REQUIRED)
endif()

juce_add_gui_app(DremCanvas
    PRODUCT_NAME "Drem Canvas"
    COMPANY_NAME "Drem"
    BUNDLE_ID "com.drem.canvas"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon_1024.png"
    ICON_SMALL "${CMAKE_CURRENT_SOURCE_DIR}/assets/icon_256.png"
)

juce_generate_juce_header(DremCanvas)

# ─── Common sources ──────────────────────────────────────────
target_sources(DremCanvas PRIVATE
    # Application entry point
    src/Main.cpp

    # Engine
    src/engine/AudioEngine.cpp
    src/engine/TransportController.cpp
    src/engine/TrackProcessor.cpp
    src/engine/MixBusProcessor.cpp
    src/engine/MetronomeProcessor.cpp
    src/engine/MidiEngine.cpp
    src/engine/AudioRecorder.cpp
    src/engine/BounceProcessor.cpp
    src/engine/StepSequencerProcessor.cpp

    # Model
    src/model/Project.cpp
    src/model/Track.cpp
    src/model/AudioClip.cpp
    src/model/MidiClip.cpp
    src/model/TempoMap.cpp
    src/model/StepSequencer.cpp

    # Plugins
    src/plugins/PluginManager.cpp
    src/plugins/PluginHost.cpp
    src/plugins/PluginWindowManager.cpp

    # Vim
    src/vim/VimContext.cpp
    src/vim/VimEngine.cpp

    # Utils
    src/utils/AudioFileUtils.cpp
    src/utils/GitIntegration.cpp
    src/utils/UndoSystem.cpp

    # Model - Phase 8
    src/model/Arrangement.cpp
    src/model/MixerState.cpp

    # Serialization
    src/model/serialization/YAMLSerializer.cpp
    src/model/serialization/SessionWriter.cpp
    src/model/serialization/SessionReader.cpp

    # ─── Graphics Engine ───────────────────────────────────────

    # Graphics - Core
    src/graphics/core/Node.cpp
    src/graphics/core/Widget.cpp
    src/graphics/core/EventDispatch.cpp

    # Graphics - Rendering
    src/graphics/rendering/Canvas.cpp
    src/graphics/rendering/Renderer.cpp

    # Graphics - Theme
    src/graphics/theme/FontManager.cpp

    # Graphics - Rendering (continued)
    src/graphics/rendering/WaveformCache.cpp
    src/graphics/rendering/TextureCache.cpp

    # Graphics - Widgets
    src/graphics/widgets/ButtonWidget.cpp
    src/graphics/widgets/LabelWidget.cpp
    src/graphics/widgets/SliderWidget.cpp
    src/graphics/widgets/ScrollViewWidget.cpp
    src/graphics/widgets/ListBoxWidget.cpp
    src/graphics/widgets/LayoutWidget.cpp

    # UI - App
    src/ui/AppController.cpp

    # UI - Transport
    src/ui/transport/TransportBarWidget.cpp

    # UI - Vim
    src/ui/vim/VimStatusBarWidget.cpp

    # UI - Arrangement
    src/ui/arrangement/TimeRulerWidget.cpp
    src/ui/arrangement/WaveformWidget.cpp
    src/ui/arrangement/TrackLaneWidget.cpp
    src/ui/arrangement/MidiClipWidget.cpp
    src/ui/arrangement/AutomationLaneWidget.cpp
    src/ui/arrangement/ArrangementWidget.cpp

    # UI - Mixer
    src/ui/mixer/MeterWidget.cpp
    src/ui/mixer/PluginSlotListWidget.cpp
    src/ui/mixer/ChannelStripWidget.cpp
    src/ui/mixer/MixerWidget.cpp

    # UI - Step Sequencer
    src/ui/sequencer/StepButtonWidget.cpp
    src/ui/sequencer/StepGridWidget.cpp
    src/ui/sequencer/PatternSelectorWidget.cpp
    src/ui/sequencer/StepSequencerWidget.cpp

    # UI - Piano Roll
    src/ui/midieditor/PianoKeyboardWidget.cpp
    src/ui/midieditor/NoteWidget.cpp
    src/ui/midieditor/PianoRollWidget.cpp

    # UI - Browser
    src/ui/browser/BrowserWidget.cpp
)

# ─── Platform-specific sources ────────────────────────────────
if(APPLE)
    target_sources(DremCanvas PRIVATE
        src/platform/NativeWindow.mm
        src/platform/MetalView.mm
        src/platform/EventBridge.mm
        src/platform/NativeDialogs.mm
        src/graphics/rendering/MetalBackend.mm
    )
elseif(UNIX AND NOT APPLE)
    target_sources(DremCanvas PRIVATE
        src/platform/linux/GlfwWindow.cpp
        src/platform/linux/VulkanBackend.cpp
        src/platform/linux/NativeDialogs.cpp
    )
endif()

target_include_directories(DremCanvas PRIVATE
    src
    "${SKIA_DIR}/include"
    "${SKIA_DIR}"
)

# ─── Platform-specific include directories ────────────────────
if(UNIX AND NOT APPLE)
    target_include_directories(DremCanvas PRIVATE
        ${GLFW_INCLUDE_DIRS}
        ${FONTCONFIG_INCLUDE_DIRS}
    )
endif()

# ObjC++ compile flags for .mm files (Apple only)
if(APPLE)
    set_source_files_properties(
        src/platform/NativeWindow.mm
        src/platform/MetalView.mm
        src/platform/EventBridge.mm
        src/platform/NativeDialogs.mm
        src/graphics/rendering/MetalBackend.mm
        PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
endif()

# ─── Compile definitions ─────────────────────────────────────
target_compile_definitions(DremCanvas PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:DremCanvas,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:DremCanvas,JUCE_VERSION>"
    SK_GANESH=1
)

if(APPLE)
    target_compile_definitions(DremCanvas PRIVATE
        SK_METAL=1
        JUCE_PLUGINHOST_AU=1
    )
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(DremCanvas PRIVATE
        SK_VULKAN=1
        JUCE_PLUGINHOST_AU=0
        JUCE_ALSA=1
        JUCE_JACK=0
    )
endif()

# ─── Link libraries ──────────────────────────────────────────
target_link_libraries(DremCanvas PRIVATE
    juce::juce_audio_basics
    juce::juce_audio_devices
    juce::juce_audio_formats
    juce::juce_audio_processors
    juce::juce_audio_utils
    juce::juce_core
    juce::juce_data_structures
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    yaml-cpp::yaml-cpp
    ${SKIA_LIB}
    PNG::PNG
)

if(APPLE)
    target_link_libraries(DremCanvas PRIVATE
        ${METAL_FRAMEWORK}
        ${METALKIT_FRAMEWORK}
        ${QUARTZCORE_FRAMEWORK}
        ${COCOA_FRAMEWORK}
        ${COREFOUNDATION_FRAMEWORK}
        ${CORETEXT_FRAMEWORK}
        ${COREGRAPHICS_FRAMEWORK}
    )
elseif(UNIX AND NOT APPLE)
    target_link_libraries(DremCanvas PRIVATE
        Vulkan::Vulkan
        ${GLFW_LIBRARIES}
        ${FONTCONFIG_LIBRARIES}
        Threads::Threads
    )
endif()

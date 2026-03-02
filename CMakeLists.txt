cmake_minimum_required(VERSION 3.22)

if(APPLE)
    project(DremCanvas VERSION 0.1.0 LANGUAGES C CXX OBJCXX)
else()
    project(DremCanvas VERSION 0.1.0 LANGUAGES C CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(BUILD_TESTING "Build test targets" OFF)

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

# ─── VST3 SDK (plugin hosting) ───────────────────────────────
FetchContent_Declare(
    vst3sdk
    GIT_REPOSITORY https://github.com/steinbergmedia/vst3sdk.git
    GIT_TAG v3.7.11_build_10
    GIT_SHALLOW TRUE
)
FetchContent_GetProperties(vst3sdk)
if(NOT vst3sdk_POPULATED)
    FetchContent_Populate(vst3sdk)
endif()
set(VST3_SDK_DIR "${vst3sdk_SOURCE_DIR}")

# ─── Skia ─────────────────────────────────────────────────────
set(SKIA_DIR "${CMAKE_SOURCE_DIR}/libs/skia")
find_library(SKIA_LIB skia PATHS "${SKIA_DIR}/lib" NO_DEFAULT_PATH)
if(NOT SKIA_LIB)
    message(WARNING "Skia not found at ${SKIA_DIR}/lib — run scripts/build_skia.sh first")
endif()

# ─── libpng (required by Skia) ────────────────────────────────
find_package(PNG REQUIRED)

# ─── Thread support (needed by dc:: libraries) ───────────────
find_package(Threads REQUIRED)

# ─── Platform-specific dependencies ──────────────────────────
if(APPLE)
    find_library(METAL_FRAMEWORK Metal)
    find_library(METALKIT_FRAMEWORK MetalKit)
    find_library(QUARTZCORE_FRAMEWORK QuartzCore)
    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(COREFOUNDATION_FRAMEWORK CoreFoundation)
    find_library(CORETEXT_FRAMEWORK CoreText)
    find_library(COREGRAPHICS_FRAMEWORK CoreGraphics)

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(SNDFILE REQUIRED sndfile)
elseif(UNIX AND NOT APPLE)
    find_package(Vulkan REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GLFW REQUIRED glfw3)
    pkg_check_modules(FONTCONFIG REQUIRED fontconfig)
    pkg_check_modules(SNDFILE REQUIRED sndfile)

    # PortAudio: prefer system package, fall back to FetchContent
    pkg_check_modules(PORTAUDIO portaudio-2)
    if(PORTAUDIO_FOUND)
        set(DC_PORTAUDIO_LINK ${PORTAUDIO_LIBRARIES})
    else()
        message(STATUS "System portaudio-2 not found — building from source via FetchContent")
        FetchContent_Declare(
            portaudio
            GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
            GIT_TAG v19.7.0
        )
        set(PA_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(PA_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(portaudio)
        set(DC_PORTAUDIO_LINK portaudio)
    endif()

    # RtMidi: prefer system package, fall back to FetchContent
    pkg_check_modules(RTMIDI rtmidi)
    if(RTMIDI_FOUND)
        set(DC_RTMIDI_LINK ${RTMIDI_LIBRARIES})
    else()
        message(STATUS "System rtmidi not found — building from source via FetchContent")
        set(RTMIDI_BUILD_TESTING OFF CACHE BOOL "" FORCE)
        set(RTMIDI_TARGETNAME_UNINSTALL "rtmidi_uninstall" CACHE STRING "" FORCE)
        FetchContent_Declare(
            rtmidi
            GIT_REPOSITORY https://github.com/thestk/rtmidi.git
            GIT_TAG 6.0.0
        )
        FetchContent_MakeAvailable(rtmidi)
        set(DC_RTMIDI_LINK rtmidi)
    endif()
    find_package(X11 REQUIRED)
    if(NOT X11_Xcomposite_FOUND)
        message(FATAL_ERROR "X11 Xcomposite extension not found (libxcomposite-dev)")
    endif()
    if(NOT X11_Xdamage_FOUND)
        message(FATAL_ERROR "X11 Xdamage extension not found (libxdamage-dev)")
    endif()
    if(NOT X11_Xfixes_FOUND)
        message(FATAL_ERROR "X11 Xfixes extension not found (libxfixes-dev)")
    endif()
    if(NOT X11_Xtst_FOUND)
        message(FATAL_ERROR "X11 Xtst extension not found (libxtst-dev)")
    endif()
endif()

# ─── dc::foundation config header ────────────────────────────
configure_file(
    "${CMAKE_SOURCE_DIR}/src/dc/foundation/config.h.in"
    "${CMAKE_BINARY_DIR}/generated/dc/foundation/config.h"
)

# ═══════════════════════════════════════════════════════════════
# dc:: Library Targets (NO Skia, NO GUI dependencies)
# ═══════════════════════════════════════════════════════════════

# ─── dc_foundation ────────────────────────────────────────────
add_library(dc_foundation STATIC
    src/dc/foundation/base64.cpp
    src/dc/foundation/message_queue.cpp
    src/dc/foundation/worker_thread.cpp
)
target_include_directories(dc_foundation PUBLIC
    src/
    ${CMAKE_BINARY_DIR}/generated/
)
target_compile_features(dc_foundation PUBLIC cxx_std_17)
target_compile_definitions(dc_foundation PRIVATE DC_LIBRARY_BUILD)
target_link_libraries(dc_foundation PUBLIC Threads::Threads)

# ─── dc_model ─────────────────────────────────────────────────
add_library(dc_model STATIC
    src/dc/model/Variant.cpp
    src/dc/model/PropertyId.cpp
    src/dc/model/PropertyTree.cpp
    src/dc/model/UndoManager.cpp
)
target_compile_definitions(dc_model PRIVATE DC_LIBRARY_BUILD)
target_link_libraries(dc_model PUBLIC dc_foundation)

# ─── dc_midi ──────────────────────────────────────────────────
add_library(dc_midi STATIC
    src/dc/midi/MidiMessage.cpp
    src/dc/midi/MidiBuffer.cpp
    src/dc/midi/MidiSequence.cpp
)
target_compile_definitions(dc_midi PRIVATE DC_LIBRARY_BUILD)
target_link_libraries(dc_midi PUBLIC dc_foundation)

# ─── dc_audio ─────────────────────────────────────────────────
add_library(dc_audio STATIC
    src/dc/audio/AudioFileReader.cpp
    src/dc/audio/AudioFileWriter.cpp
    src/dc/audio/DiskStreamer.cpp
    src/dc/audio/ThreadedRecorder.cpp
)
target_compile_definitions(dc_audio PRIVATE DC_LIBRARY_BUILD)
target_link_libraries(dc_audio PUBLIC dc_foundation Threads::Threads)

# libsndfile (required for audio file I/O)
target_include_directories(dc_audio PRIVATE ${SNDFILE_INCLUDE_DIRS})
target_link_libraries(dc_audio PRIVATE ${SNDFILE_LIBRARIES})

# ═══════════════════════════════════════════════════════════════
# Main Application Target
# ═══════════════════════════════════════════════════════════════

# ─── Executable target ────────────────────────────────────────
add_executable(DremCanvas
    # dc:: device managers (depend on PortAudio/RtMidi, stay in main target)
    src/dc/midi/MidiDeviceManager.cpp
    src/dc/audio/PortAudioDeviceManager.cpp

    # dc::engine library
    src/dc/engine/MidiBlock.cpp
    src/dc/engine/BufferPool.cpp
    src/dc/engine/GraphExecutor.cpp
    src/dc/engine/AudioGraph.cpp
    src/dc/engine/DelayNode.cpp

    # dc::plugins library
    src/dc/plugins/VST3Module.cpp
    src/dc/plugins/PluginScanner.cpp
    src/dc/plugins/ComponentHandler.cpp
    src/dc/plugins/PluginInstance.cpp
    src/dc/plugins/PluginEditor.cpp
    src/dc/plugins/ProbeCache.cpp
    src/dc/plugins/VST3Host.cpp

    # VST3 SDK base (IID definitions)
    "${VST3_SDK_DIR}/pluginterfaces/base/coreiids.cpp"
    "${VST3_SDK_DIR}/pluginterfaces/base/funknown.cpp"
    "${VST3_SDK_DIR}/public.sdk/source/vst/vstinitiids.cpp"
    "${VST3_SDK_DIR}/public.sdk/source/common/commoniids.cpp"

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
    src/engine/MidiClipProcessor.cpp
    src/engine/MeterTapProcessor.cpp

    # Model
    src/model/Project.cpp
    src/model/Track.cpp
    src/model/AudioClip.cpp
    src/model/MidiClip.cpp
    src/model/Clipboard.cpp
    src/model/TempoMap.cpp
    src/model/StepSequencer.cpp
    src/model/RecentProjects.cpp
    src/model/GridSystem.cpp

    # Plugins
    src/plugins/PluginManager.cpp
    src/plugins/PluginHost.cpp
    src/plugins/PluginWindowManager.cpp
    src/plugins/ParameterFinderScanner.cpp
    src/plugins/PluginEditorBridge.cpp
    src/plugins/SyntheticInputProbe.cpp
    src/plugins/SpatialScanCache.cpp
    src/plugins/SyntheticMouseDrag.cpp
    src/plugins/MouseEventForwarder.cpp

    # Vim
    src/vim/VimContext.cpp
    src/vim/VimEngine.cpp
    src/vim/ActionRegistry.cpp

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
    src/graphics/widgets/ProgressBarWidget.cpp

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
    src/ui/midieditor/NoteGridWidget.cpp
    src/ui/midieditor/PianoRollRulerWidget.cpp
    src/ui/midieditor/VelocityLaneWidget.cpp
    src/ui/midieditor/CCLaneWidget.cpp
    src/ui/midieditor/PianoRollWidget.cpp

    # UI - Browser
    src/ui/browser/BrowserWidget.cpp

    # UI - Command Palette
    src/ui/palette/CommandPaletteWidget.cpp

    # UI - Virtual Keyboard
    src/ui/keyboard/VirtualKeyboardWidget.cpp

    # UI - Plugin View
    src/ui/pluginview/PluginViewWidget.cpp
    src/ui/pluginview/ParameterGridWidget.cpp
)

# ─── Platform-specific sources ────────────────────────────────
if(APPLE)
    target_sources(DremCanvas PRIVATE
        src/platform/NativeWindow.mm
        src/platform/MetalView.mm
        src/platform/EventBridge.mm
        src/platform/NativeDialogs.mm
        src/platform/MacPluginEditorBridge.mm
        src/platform/MacSyntheticInputProbe.mm
        src/graphics/rendering/MetalBackend.mm
    )
elseif(UNIX AND NOT APPLE)
    target_sources(DremCanvas PRIVATE
        src/platform/linux/GlfwWindow.cpp
        src/platform/linux/VulkanBackend.cpp
        src/platform/linux/NativeDialogs.cpp
        src/platform/linux/EmbeddedPluginEditor.cpp
        src/platform/linux/X11Reparent.cpp
        src/platform/linux/X11Compositor.cpp
        src/platform/linux/X11MouseProbe.cpp
        src/platform/linux/X11PluginEditorBridge.cpp
        src/platform/linux/X11SyntheticInputProbe.cpp
        src/platform/linux/X11SyntheticMouseDrag.cpp
        src/platform/linux/X11MouseEventForwarder.cpp
    )
endif()

# Copy icon next to binary so the app can set it at runtime (X11)
add_custom_command(TARGET DremCanvas POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_SOURCE_DIR}/assets/icon_256.png"
        "$<TARGET_FILE_DIR:DremCanvas>/drem-canvas.png"
    COMMENT "Copying app icon next to binary"
)

# ─── Install rules (Linux desktop integration) ───────────────
if(UNIX AND NOT APPLE)
    include(GNUInstallDirs)
    install(TARGETS DremCanvas RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES assets/com.drem.canvas.desktop
            DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES assets/icon_256.png
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps
            RENAME drem-canvas.png)
    install(FILES assets/icon.svg
            DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
            RENAME drem-canvas.svg)
endif()

target_include_directories(DremCanvas PRIVATE
    src
    "${SKIA_DIR}/include"
    "${SKIA_DIR}"
    "${VST3_SDK_DIR}"
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
        src/platform/MacPluginEditorBridge.mm
        src/platform/MacSyntheticInputProbe.mm
        src/graphics/rendering/MetalBackend.mm
        PROPERTIES
        COMPILE_FLAGS "-fobjc-arc"
    )
endif()

# ─── Compile definitions ─────────────────────────────────────
target_compile_definitions(DremCanvas PRIVATE
    SK_GANESH=1
    DC_APP_NAME="Drem Canvas"
    DC_APP_VERSION="${PROJECT_VERSION}"
)

if(APPLE)
    target_compile_definitions(DremCanvas PRIVATE
        SK_METAL=1
    )
elseif(UNIX AND NOT APPLE)
    target_compile_definitions(DremCanvas PRIVATE
        SK_VULKAN=1
    )
endif()

# ─── Link libraries ──────────────────────────────────────────
target_link_libraries(DremCanvas PRIVATE
    dc_foundation
    dc_model
    dc_midi
    dc_audio
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
        ${DC_PORTAUDIO_LINK}
        ${DC_RTMIDI_LINK}
        Threads::Threads
        X11::X11
        ${X11_Xcomposite_LIB}
        ${X11_Xdamage_LIB}
        ${X11_Xfixes_LIB}
        ${X11_Xtst_LIB}
    )
endif()

# ═══════════════════════════════════════════════════════════════
# Test Infrastructure (only when BUILD_TESTING=ON)
# ═══════════════════════════════════════════════════════════════

if(BUILD_TESTING)
    enable_testing()

    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG        v3.8.1
    )
    FetchContent_MakeAvailable(Catch2)

    # CRITICAL: Catch2's catch_discover_tests() macro lives in extras/
    list(APPEND CMAKE_MODULE_PATH ${Catch2_SOURCE_DIR}/extras)
    include(CTest)
    include(Catch)

    FetchContent_Declare(
        trompeloeil
        GIT_REPOSITORY https://github.com/rollbear/trompeloeil.git
        GIT_TAG        v49
    )
    FetchContent_MakeAvailable(trompeloeil)

    add_subdirectory(tests)
endif()

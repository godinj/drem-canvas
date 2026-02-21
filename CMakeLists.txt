cmake_minimum_required(VERSION 3.22)
project(DremCanvas VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_subdirectory(libs/JUCE)

juce_add_gui_app(DremCanvas
    PRODUCT_NAME "Drem Canvas"
    COMPANY_NAME "Drem"
    BUNDLE_ID "com.drem.canvas"
)

juce_generate_juce_header(DremCanvas)

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

    # Model
    src/model/Project.cpp
    src/model/Track.cpp
    src/model/AudioClip.cpp
    src/model/MidiClip.cpp
    src/model/TempoMap.cpp

    # Plugins
    src/plugins/PluginManager.cpp
    src/plugins/PluginHost.cpp
    src/plugins/PluginWindowManager.cpp

    # GUI - Main
    src/gui/MainWindow.cpp
    src/gui/MainComponent.cpp

    # GUI - Transport
    src/gui/transport/TransportBar.cpp

    # GUI - Arrangement
    src/gui/arrangement/ArrangementView.cpp
    src/gui/arrangement/TrackLane.cpp
    src/gui/arrangement/WaveformView.cpp
    src/gui/arrangement/TimeRuler.cpp
    src/gui/arrangement/MidiClipView.cpp
    src/gui/arrangement/Cursor.cpp
    src/gui/arrangement/AutomationLane.cpp

    # GUI - Mixer
    src/gui/mixer/MixerPanel.cpp
    src/gui/mixer/ChannelStrip.cpp
    src/gui/mixer/MeterComponent.cpp

    # GUI - Piano Roll
    src/gui/midieditor/PianoRollEditor.cpp
    src/gui/midieditor/PianoKeyboard.cpp
    src/gui/midieditor/NoteComponent.cpp

    # GUI - Browser
    src/gui/browser/BrowserPanel.cpp

    # GUI - Common
    src/gui/common/DremLookAndFeel.cpp

    # Utils
    src/utils/AudioFileUtils.cpp
    src/utils/UndoSystem.cpp

    # Model - Phase 8
    src/model/Arrangement.cpp
    src/model/MixerState.cpp
)

target_include_directories(DremCanvas PRIVATE src)

target_compile_definitions(DremCanvas PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_PLUGINHOST_VST3=1
    JUCE_PLUGINHOST_AU=1
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:DremCanvas,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:DremCanvas,JUCE_VERSION>"
)

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
)

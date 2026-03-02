# Agent: Audio Device Manager (PortAudio)

You are working on the `feature/sans-juce-audio-io` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 2 of the Sans-JUCE migration: implement audio device management
using PortAudio.

## Context

Read these specs before starting:
- `docs/sans-juce/05-audio-io.md` (AudioDeviceManager design)
- `docs/sans-juce/08-migration-guide.md` (Phase 2 section)
- `src/engine/AudioEngine.h/.cpp` (current JUCE AudioDeviceManager + AudioProcessorPlayer usage)

## Prerequisites

Ensure PortAudio is available:

```bash
sudo apt install portaudio19-dev
```

Add to `CMakeLists.txt`:

```cmake
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2)
target_link_libraries(DremCanvas PRIVATE ${PORTAUDIO_LIBRARIES})
target_include_directories(DremCanvas PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
```

## Deliverables

### New files (src/dc/audio/)

#### 1. AudioDeviceManager.h

Abstract interface.

- `AudioCallback`: `audioCallback(inputChannels, numIn, outputChannels, numOut, numSamples)`,
  `audioDeviceAboutToStart(sampleRate, blockSize)`, `audioDeviceStopped()`
- `AudioDeviceInfo`: name, maxInputChannels, maxOutputChannels, availableSampleRates,
  availableBufferSizes, defaultSampleRate, defaultBufferSize
- `AudioDeviceManager`: `getAvailableDevices()`, `openDevice()`, `openDefaultDevice()`,
  `closeDevice()`, `setCallback()`, `isOpen()`, `getSampleRate()`, `getBufferSize()`,
  `getCurrentDeviceName()`
- Static factory: `create()` → `unique_ptr<AudioDeviceManager>`

#### 2. PortAudioDeviceManager.h/.cpp

PortAudio implementation.

- `Pa_Initialize` in constructor, `Pa_Terminate` in destructor
- `getAvailableDevices()` enumerates `Pa_GetDeviceCount` / `Pa_GetDeviceInfo`
- `openDevice()` uses `Pa_OpenStream` with `paFloat32` format
- Static `paCallback` bridges to `AudioCallback` interface
- RAII: close stream in destructor

### Migration

#### 3. src/engine/AudioEngine.h/.cpp

Replace `juce::AudioDeviceManager` with `dc::AudioDeviceManager`, replace
`juce::AudioProcessorPlayer` with a custom `AudioCallback` implementation
that drives the existing processor graph.

**NOTE**: The `AudioProcessorGraph` itself stays as JUCE until Phase 3. Your callback
should bridge `dc::AudioDeviceManager` → `juce::AudioProcessorGraph::processBlock()`.

## Important

The audio callback is the real-time hot path. Never allocate or lock in the callback.
PortAudio delivers `float**` channel pointers directly — no format conversion needed.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset release`

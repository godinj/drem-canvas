# Agent: Audio File I/O (libsndfile)

You are working on the `feature/sans-juce-audio-io` branch of Drem Canvas, a C++17 DAW.
Your task is Phase 2 of the Sans-JUCE migration: implement audio file I/O using libsndfile.

## Context

Read these specs before starting:
- `docs/sans-juce/05-audio-io.md` (full audio I/O design)
- `docs/sans-juce/08-migration-guide.md` (Phase 2 section)
- `src/utils/AudioFileUtils.h/.cpp` (current JUCE-based implementation to replace)
- `src/graphics/rendering/WaveformCache.h/.cpp` (uses AudioFormatReader — will migrate)

## Prerequisites

Ensure libsndfile is available:

```bash
sudo apt install libsndfile1-dev
```

Add to `CMakeLists.txt`:

```cmake
pkg_check_modules(SNDFILE REQUIRED sndfile)
target_link_libraries(DremCanvas PRIVATE ${SNDFILE_LIBRARIES})
target_include_directories(DremCanvas PRIVATE ${SNDFILE_INCLUDE_DIRS})
```

## Deliverables

### New files (src/dc/audio/)

#### 1. AudioFileReader.h/.cpp

libsndfile read wrapper.

- Static factory: `open(fs::path)` → `unique_ptr<AudioFileReader>` (nullptr on failure)
- `getNumChannels()`, `getLengthInSamples()`, `getSampleRate()`
- `read(float* buffer, int64_t startFrame, int64_t numFrames)` → frames read
- `read(AudioBlock& block, int64_t startFrame, int64_t numFrames)` — de-interleaves
- `getFormatName()`, `getBitDepth()`
- Uses `sf_open` / `sf_readf_float` / `sf_seek` internally

#### 2. AudioFileWriter.h/.cpp

libsndfile write wrapper.

- `enum Format { WAV_16, WAV_24, WAV_32F, AIFF_16, AIFF_24, FLAC_16, FLAC_24 }`
- Static factory: `create(path, format, numChannels, sampleRate)` → `unique_ptr`
- `write(const float* buffer, int64_t numFrames)` — interleaved input
- `write(const AudioBlock& block, int numSamples)` — interleaves internally
- `close()`

### Migration

#### 3. src/utils/AudioFileUtils.h/.cpp

Replace `juce::AudioFormatManager` / `juce::AudioFormatReader` with `dc::AudioFileReader`.
Keep the same public API surface where possible.

#### 4. src/graphics/rendering/WaveformCache.h/.cpp

Replace reader in `loadSamples()` with `dc::AudioFileReader`.
The waveform computation (min/max pairs) stays the same.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on own line for classes/functions,
  `camelCase` methods, `PascalCase` classes
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset release`

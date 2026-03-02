# 05 — Audio Device & File I/O

> Replaces `juce::AudioDeviceManager`, `juce::AudioFormatManager`,
> `juce::AudioFormatReader`, and related classes with PortAudio and libsndfile.

**Phase**: 2 (Audio I/O + MIDI)
**Dependencies**: Phase 0 (Foundation Types)
**Related**: [02-audio-graph.md](02-audio-graph.md), [08-migration-guide.md](08-migration-guide.md)

---

## Overview

Drem Canvas uses JUCE for three distinct audio I/O concerns:

1. **Audio device management** — `juce::AudioDeviceManager` for enumerating devices,
   opening streams, and delivering audio callbacks
2. **Audio file reading** — `juce::AudioFormatManager` + `juce::AudioFormatReader` for
   loading WAV/AIFF/FLAC files
3. **Audio file writing** — `juce::AudioFormatWriter::ThreadedWriter` for recording

The replacements:
- **PortAudio** for device I/O (behind an abstraction layer)
- **libsndfile** for file read/write
- Custom `DiskStreamer` and `ThreadedRecorder` for background I/O

---

## Current JUCE Usage

### AudioDeviceManager (src/engine/AudioEngine.h)

```cpp
juce::AudioDeviceManager deviceManager;
juce::AudioProcessorPlayer player;

// Initialization
deviceManager.initialiseWithDefaultDevices(2, 2);
player.setProcessor(&graph);
deviceManager.addAudioCallback(&player);

// Query
deviceManager.getCurrentAudioDevice()->getCurrentSampleRate();
deviceManager.getCurrentAudioDevice()->getCurrentBufferSizeSamples();
```

### AudioFormatManager (src/utils/AudioFileUtils.h)

```cpp
juce::AudioFormatManager formatManager;
formatManager.registerBasicFormats();  // WAV, AIFF, FLAC, OGG

auto reader = formatManager.createReaderFor(juce::File(path));
reader->numChannels;
reader->sampleRate;
reader->lengthInSamples;
reader->read(&buffer, 0, numSamples, startSample, true, true);
```

### ThreadedWriter (src/engine/AudioRecorder.h)

```cpp
juce::TimeSliceThread writerThread{"Audio Writer"};
juce::AudioFormatWriter::ThreadedWriter* threadedWriter;

// In audio callback
threadedWriter->write(buffer.getArrayOfReadPointers(), numSamples);
```

### WaveformCache (src/graphics/rendering/WaveformCache.h)

Uses `AudioFormatReader` to load samples for waveform rendering:
```cpp
auto reader = formatManager.createReaderFor(file);
reader->read(&buffer, 0, numSamples, 0, true, true);
// Compute min/max pairs for display
```

---

## Design: `dc::AudioDeviceManager`

### Abstract Interface

```cpp
namespace dc {

/// Audio callback interface (replaces juce::AudioIODeviceCallback)
class AudioCallback
{
public:
    virtual ~AudioCallback() = default;

    /// Called on the audio thread for each block
    virtual void audioCallback(const float** inputChannels,
                               int numInputChannels,
                               float** outputChannels,
                               int numOutputChannels,
                               int numSamples) = 0;

    /// Called when the device starts/stops
    virtual void audioDeviceAboutToStart(double sampleRate,
                                         int blockSize) {}
    virtual void audioDeviceStopped() {}
};

/// Device information
struct AudioDeviceInfo
{
    std::string name;
    int maxInputChannels = 0;
    int maxOutputChannels = 0;
    std::vector<double> availableSampleRates;
    std::vector<int> availableBufferSizes;
    double defaultSampleRate = 44100.0;
    int defaultBufferSize = 512;
};

/// Abstract audio device interface
class AudioDeviceManager
{
public:
    virtual ~AudioDeviceManager() = default;

    /// Enumerate available devices
    virtual std::vector<AudioDeviceInfo> getAvailableDevices() const = 0;

    /// Open a device with the given settings
    /// Returns error string (empty on success)
    virtual std::string openDevice(const std::string& deviceName,
                                    double sampleRate,
                                    int bufferSize,
                                    int numInputChannels,
                                    int numOutputChannels) = 0;

    /// Open default device with default settings
    virtual std::string openDefaultDevice(int numInputChannels = 2,
                                           int numOutputChannels = 2) = 0;

    /// Close the current device
    virtual void closeDevice() = 0;

    /// Set the audio callback (only one at a time)
    virtual void setCallback(AudioCallback* callback) = 0;

    /// Query current state
    virtual bool isOpen() const = 0;
    virtual double getSampleRate() const = 0;
    virtual int getBufferSize() const = 0;
    virtual std::string getCurrentDeviceName() const = 0;

    /// Factory
    static std::unique_ptr<AudioDeviceManager> create();
};

} // namespace dc
```

### PortAudio Implementation

```cpp
namespace dc {

class PortAudioDeviceManager : public AudioDeviceManager
{
public:
    PortAudioDeviceManager();
    ~PortAudioDeviceManager();

    std::vector<AudioDeviceInfo> getAvailableDevices() const override;
    std::string openDevice(const std::string& deviceName,
                            double sampleRate, int bufferSize,
                            int numInputChannels,
                            int numOutputChannels) override;
    std::string openDefaultDevice(int numInputChannels,
                                   int numOutputChannels) override;
    void closeDevice() override;
    void setCallback(AudioCallback* callback) override;
    bool isOpen() const override;
    double getSampleRate() const override;
    int getBufferSize() const override;
    std::string getCurrentDeviceName() const override;

private:
    PaStream* stream_ = nullptr;
    AudioCallback* callback_ = nullptr;
    double sampleRate_ = 0;
    int bufferSize_ = 0;
    std::string deviceName_;

    /// PortAudio callback (static, bridges to AudioCallback)
    static int paCallback(const void* input, void* output,
                          unsigned long frameCount,
                          const PaStreamCallbackTimeInfo* timeInfo,
                          PaStreamCallbackFlags statusFlags,
                          void* userData);
};

} // namespace dc
```

### Integration with AudioGraph

The `AudioDeviceManager` callback drives the `AudioGraph`:

```cpp
class AudioEngineCallback : public dc::AudioCallback
{
    dc::AudioGraph& graph_;

    void audioCallback(const float** input, int numIn,
                       float** output, int numOut,
                       int numSamples) override
    {
        dc::AudioBlock inputBlock(const_cast<float**>(input), numIn, numSamples);
        dc::AudioBlock outputBlock(output, numOut, numSamples);
        dc::MidiBlock midiIn, midiOut;

        graph_.processBlock(inputBlock, midiIn, outputBlock, midiOut, numSamples);
    }
};
```

---

## Design: `dc::AudioFileReader` / `dc::AudioFileWriter`

### AudioFileReader (libsndfile)

```cpp
namespace dc {

class AudioFileReader
{
public:
    /// Open a file for reading. Returns nullptr on failure.
    static std::unique_ptr<AudioFileReader> open(
        const std::filesystem::path& path);

    ~AudioFileReader();

    int getNumChannels() const;
    int64_t getLengthInSamples() const;
    double getSampleRate() const;

    /// Read interleaved samples into buffer.
    /// Returns number of frames actually read.
    int64_t read(float* buffer, int64_t startFrame, int64_t numFrames);

    /// Read into a multi-channel AudioBlock.
    /// De-interleaves into separate channel buffers.
    int64_t read(AudioBlock& block, int64_t startFrame, int64_t numFrames);

    /// Get file format info
    std::string getFormatName() const;  // "WAV", "AIFF", "FLAC", etc.
    int getBitDepth() const;

private:
    AudioFileReader() = default;
    SNDFILE* file_ = nullptr;
    SF_INFO info_{};
    std::filesystem::path path_;
};

} // namespace dc
```

### AudioFileWriter (libsndfile)

```cpp
namespace dc {

class AudioFileWriter
{
public:
    enum class Format { WAV_16, WAV_24, WAV_32F, AIFF_16, AIFF_24, FLAC_16, FLAC_24 };

    /// Create a file for writing.
    static std::unique_ptr<AudioFileWriter> create(
        const std::filesystem::path& path,
        Format format,
        int numChannels,
        double sampleRate);

    ~AudioFileWriter();

    /// Write interleaved samples. Returns true on success.
    bool write(const float* buffer, int64_t numFrames);

    /// Write from a multi-channel AudioBlock (interleaves internally).
    bool write(const AudioBlock& block, int numSamples);

    /// Flush and close the file
    void close();

private:
    AudioFileWriter() = default;
    SNDFILE* file_ = nullptr;
    SF_INFO info_{};
    int numChannels_ = 0;
    std::vector<float> interleaveBuffer_;  // scratch buffer for de/interleave
};

} // namespace dc
```

---

## Design: `dc::DiskStreamer`

Background-threaded disk reader that pre-fills a ring buffer. Replaces the
pattern of `juce::AudioFormatReaderSource` + `juce::AudioTransportSource`.

```cpp
namespace dc {

class DiskStreamer
{
public:
    DiskStreamer();
    ~DiskStreamer();

    /// Open a file for streaming
    bool open(const std::filesystem::path& path);

    /// Close the current file
    void close();

    /// Seek to a position (in samples)
    void seek(int64_t positionInSamples);

    /// Read samples from the ring buffer into the output block.
    /// Called from the audio thread. Non-blocking.
    /// Returns number of samples actually read (may be less if buffer underrun).
    int read(AudioBlock& output, int numSamples);

    /// Start/stop the background read thread
    void start();
    void stop();

    /// Query
    int64_t getLengthInSamples() const;
    double getSampleRate() const;
    int getNumChannels() const;

private:
    std::unique_ptr<AudioFileReader> reader_;
    dc::SPSCQueue<float> ringBuffer_;  // per-channel ring buffers
    std::thread readThread_;
    std::atomic<bool> running_{false};
    std::atomic<int64_t> readPosition_{0};
    std::atomic<int64_t> seekTarget_{-1};  // -1 = no pending seek

    void readLoop();
};

} // namespace dc
```

### Ring Buffer Strategy

The ring buffer holds ~1 second of audio ahead of the current read position.
The background thread continuously fills the buffer. The audio thread drains it.

```
Background thread:                    Audio thread:
  read from disk → [ring buffer] → read from ring buffer
                    ~~~~~~~~~~~~
                    ~1s of audio
```

If the audio thread reads faster than the disk thread fills (buffer underrun),
silence is output and a warning is logged.

---

## Design: `dc::ThreadedRecorder`

Lock-free audio recording to disk. Replaces `juce::AudioFormatWriter::ThreadedWriter`.

```cpp
namespace dc {

class ThreadedRecorder
{
public:
    ThreadedRecorder();
    ~ThreadedRecorder();

    /// Start recording to a file
    bool start(const std::filesystem::path& path,
               AudioFileWriter::Format format,
               int numChannels,
               double sampleRate);

    /// Write audio from the audio thread (lock-free push to ring buffer)
    void write(const AudioBlock& block, int numSamples);

    /// Stop recording (flushes remaining data to disk)
    void stop();

    /// Is currently recording?
    bool isRecording() const;

private:
    std::unique_ptr<AudioFileWriter> writer_;
    dc::SPSCQueue<float> ringBuffer_;
    std::thread writeThread_;
    std::atomic<bool> recording_{false};

    void writeLoop();
};

} // namespace dc
```

### Audio Thread Safety

The audio thread calls `write()` which does a lock-free push to an SPSC ring
buffer. A background thread continuously drains the ring buffer to disk via
`AudioFileWriter`.

```
Audio thread:                         Background thread:
  push samples → [ring buffer] → drain to disk (AudioFileWriter)
```

---

## Waveform Cache Migration

The `WaveformCache` (`src/graphics/rendering/WaveformCache.h`) currently uses
`juce::AudioFormatReader` to load samples for waveform rendering. The migration
replaces the reader:

```cpp
// Before
auto reader = formatManager.createReaderFor(juce::File(path));
reader->read(&buffer, 0, numSamples, 0, true, true);

// After
auto reader = dc::AudioFileReader::open(path);
reader->read(block, 0, numSamples);
```

The waveform computation (min/max pairs at various zoom levels) stays the same.

---

## CMake Integration

### PortAudio

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(PORTAUDIO REQUIRED portaudio-2)
target_link_libraries(DremCanvas PRIVATE ${PORTAUDIO_LIBRARIES})
target_include_directories(DremCanvas PRIVATE ${PORTAUDIO_INCLUDE_DIRS})
```

Or via FetchContent:
```cmake
FetchContent_Declare(portaudio
    GIT_REPOSITORY https://github.com/PortAudio/portaudio.git
    GIT_TAG v19.7.0)
FetchContent_MakeAvailable(portaudio)
target_link_libraries(DremCanvas PRIVATE PortAudio)
```

### libsndfile

```cmake
find_package(PkgConfig REQUIRED)
pkg_check_modules(SNDFILE REQUIRED sndfile)
target_link_libraries(DremCanvas PRIVATE ${SNDFILE_LIBRARIES})
target_include_directories(DremCanvas PRIVATE ${SNDFILE_INCLUDE_DIRS})
```

### System packages

Debian/Ubuntu:
```bash
sudo apt install libportaudio2 portaudio19-dev libsndfile1-dev
```

Fedora:
```bash
sudo dnf install portaudio-devel libsndfile-devel
```

macOS:
```bash
brew install portaudio libsndfile
```

---

## Migration Path

### Step 1: Add PortAudio + libsndfile dependencies

Add to CMakeLists.txt, update `scripts/bootstrap.sh` and `scripts/check_deps.sh`.

### Step 2: Implement dc::AudioFileReader/Writer

Wrap libsndfile. Test with existing audio files.

### Step 3: Migrate AudioFileUtils

Replace `juce::AudioFormatManager` usage in `AudioFileUtils.h/.cpp`.

### Step 4: Migrate WaveformCache

Replace reader in `WaveformCache::loadSamples()`.

### Step 5: Implement dc::AudioDeviceManager (PortAudio)

Create `PortAudioDeviceManager`. Test device enumeration and audio output.

### Step 6: Implement dc::DiskStreamer + dc::ThreadedRecorder

Replace `AudioTransportSource` and `ThreadedWriter` patterns.

### Step 7: Migrate AudioEngine

Replace `juce::AudioDeviceManager` + `juce::AudioProcessorPlayer` with
`dc::AudioDeviceManager` + custom callback.

### Step 8: Migrate AudioRecorder

Replace `juce::AudioFormatWriter::ThreadedWriter` with `dc::ThreadedRecorder`.

---

## Files Affected

| File | JUCE APIs replaced |
|------|-------------------|
| `src/engine/AudioEngine.h/.cpp` | `AudioDeviceManager`, `AudioProcessorPlayer` |
| `src/engine/AudioRecorder.h/.cpp` | `AudioFormatWriter::ThreadedWriter`, `TimeSliceThread` |
| `src/engine/TrackProcessor.h/.cpp` | `AudioBuffer<float>` → `AudioBlock` |
| `src/utils/AudioFileUtils.h/.cpp` | `AudioFormatManager`, `AudioFormatReader` |
| `src/graphics/rendering/WaveformCache.h/.cpp` | `AudioFormatReader` |
| `src/model/serialization/SessionReader.cpp` | Audio file validation |

## Testing Strategy

1. **Device enumeration**: List available audio devices, verify names match system
2. **Playback test**: Open device, play sine wave, verify audible output
3. **File read test**: Load WAV/AIFF/FLAC files, verify sample data matches JUCE reader
4. **File write test**: Write samples to WAV, re-read, verify round-trip accuracy
5. **DiskStreamer test**: Stream a large file, verify no underruns at normal speed
6. **Recording test**: Record 10 seconds, verify output file integrity
7. **Waveform test**: Generate waveform from file, compare against JUCE baseline

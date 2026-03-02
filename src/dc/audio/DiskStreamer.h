#pragma once

#include "AudioBlock.h"
#include "AudioFileReader.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace dc {

/// Background-threaded disk reader for audio playback.
///
/// Reads audio from disk into per-channel ring buffers on a background thread.
/// The audio thread drains the ring buffers via read(), which is lock-free
/// and safe to call from the real-time thread.
///
/// Background disk reader with ring buffer for real-time playback.
class DiskStreamer
{
public:
    /// @param bufferSizeInFrames  Ring buffer capacity in frames.
    ///        Rounded up to the next power of 2. Default ~1s at 48kHz.
    explicit DiskStreamer (int bufferSizeInFrames = 48000);
    ~DiskStreamer();

    DiskStreamer (const DiskStreamer&) = delete;
    DiskStreamer& operator= (const DiskStreamer&) = delete;

    /// Open an audio file. Allocates ring buffers but does NOT start the
    /// background read thread. Returns false if the file cannot be opened.
    bool open (const std::filesystem::path& path);

    /// Close the file and release ring buffers.
    void close();

    /// Request a seek to an absolute sample position.
    /// The background thread will reposition and refill the ring buffer.
    void seek (int64_t positionInSamples);

    /// Read from ring buffers into output. Audio-thread safe (non-blocking).
    /// Returns the number of frames actually read (may be < numSamples on
    /// underrun). Missing frames are filled with silence.
    int read (AudioBlock& output, int numSamples);

    /// Start the background read thread.
    void start();

    /// Stop the background read thread and join.
    void stop();

    int64_t getLengthInSamples() const;
    double getSampleRate() const;
    int getNumChannels() const;

private:
    static size_t nextPowerOf2 (size_t v);

    void readThreadFunc();

    std::unique_ptr<AudioFileReader> reader_;

    // Per-channel ring buffers
    int numChannels_ = 0;
    size_t ringCapacity_ = 0;  // power of 2
    size_t ringMask_ = 0;      // ringCapacity_ - 1
    std::vector<std::vector<float>> ringBuffers_;

    // Ring buffer positions (frame-based, monotonically increasing)
    std::atomic<size_t> readPos_ { 0 };
    std::atomic<size_t> writePos_ { 0 };

    // Seek support
    std::atomic<int64_t> seekTarget_ { -1 };

    // Current file read position
    std::atomic<int64_t> diskPosition_ { 0 };

    // Background thread
    std::thread readThread_;
    std::atomic<bool> running_ { false };
    std::mutex mutex_;
    std::condition_variable cv_;

    int requestedBufferSize_;
};

} // namespace dc

#pragma once

#include "AudioBlock.h"
#include "AudioFileWriter.h"
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>

namespace dc {

/// Lock-free audio recorder that writes to disk on a background thread.
///
/// The audio thread pushes samples via write(), which interleaves into a ring
/// buffer without blocking. A background thread drains the ring buffer to disk
/// via AudioFileWriter. On overflow, samples are dropped (never blocks the
/// audio thread).
///
/// Background threaded audio recorder with lock-free ring buffer.
class ThreadedRecorder
{
public:
    /// @param bufferSizeInFrames  Ring buffer capacity in frames.
    ///        Rounded up to the next power of 2. Default ~1s at 48kHz.
    explicit ThreadedRecorder (int bufferSizeInFrames = 48000);
    ~ThreadedRecorder();

    ThreadedRecorder (const ThreadedRecorder&) = delete;
    ThreadedRecorder& operator= (const ThreadedRecorder&) = delete;

    /// Start recording to a file. Creates the file, allocates the ring buffer,
    /// and spawns the background write thread. Returns false on failure.
    bool start (const std::filesystem::path& path,
                AudioFileWriter::Format format,
                int numChannels,
                double sampleRate);

    /// Push audio into the ring buffer. Audio-thread safe (non-blocking).
    /// Drops samples on overflow rather than blocking.
    void write (const AudioBlock& block, int numSamples);

    /// Stop recording. Flushes remaining data to disk, joins the background
    /// thread, and closes the file.
    void stop();

    /// Returns true if currently recording.
    bool isRecording() const;

    /// Returns the total number of sample frames written to disk so far.
    int64_t getRecordedSampleCount() const;

private:
    static size_t nextPowerOf2 (size_t v);

    void writeThreadFunc();

    std::unique_ptr<AudioFileWriter> writer_;

    // Interleaved ring buffer
    int numChannels_ = 0;
    size_t ringCapacity_ = 0;   // power of 2 (in frames)
    size_t ringMask_ = 0;       // ringCapacity_ - 1
    std::vector<float> ringBuffer_;  // interleaved, capacity * numChannels floats

    // Ring buffer positions (frame-based, monotonically increasing)
    std::atomic<size_t> readPos_ { 0 };
    std::atomic<size_t> writePos_ { 0 };

    // Background thread
    std::thread writeThread_;
    std::atomic<bool> recording_ { false };
    std::mutex mutex_;
    std::condition_variable cv_;

    std::atomic<int64_t> recordedSamples_ { 0 };
    int requestedBufferSize_;
};

} // namespace dc

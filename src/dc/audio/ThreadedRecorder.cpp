#include "ThreadedRecorder.h"
#include <algorithm>
#include <cstring>

namespace dc {

size_t ThreadedRecorder::nextPowerOf2 (size_t v)
{
    v--;
    v |= v >> 1;
    v |= v >> 2;
    v |= v >> 4;
    v |= v >> 8;
    v |= v >> 16;
    v |= v >> 32;
    v++;
    return v;
}

ThreadedRecorder::ThreadedRecorder (int bufferSizeInFrames)
    : requestedBufferSize_ (bufferSizeInFrames)
{
}

ThreadedRecorder::~ThreadedRecorder()
{
    stop();
}

bool ThreadedRecorder::start (const std::filesystem::path& path,
                              AudioFileWriter::Format format,
                              int numChannels,
                              double sampleRate)
{
    stop();

    writer_ = AudioFileWriter::create (path, format, numChannels, sampleRate);
    if (writer_ == nullptr)
        return false;

    numChannels_ = numChannels;
    ringCapacity_ = nextPowerOf2 (static_cast<size_t> (requestedBufferSize_));
    ringMask_ = ringCapacity_ - 1;
    ringBuffer_.resize (ringCapacity_ * static_cast<size_t> (numChannels_), 0.0f);

    readPos_.store (0, std::memory_order_relaxed);
    writePos_.store (0, std::memory_order_relaxed);
    recordedSamples_.store (0, std::memory_order_relaxed);

    recording_.store (true, std::memory_order_release);
    writeThread_ = std::thread (&ThreadedRecorder::writeThreadFunc, this);

    return true;
}

void ThreadedRecorder::write (const AudioBlock& block, int numSamples)
{
    if (! recording_.load (std::memory_order_relaxed))
        return;

    size_t rp = readPos_.load (std::memory_order_acquire);
    size_t wp = writePos_.load (std::memory_order_relaxed);
    size_t used = wp - rp;
    size_t space = ringCapacity_ - used;

    // Drop on overflow — never block the audio thread
    int framesToWrite = std::min (numSamples, static_cast<int> (space));
    if (framesToWrite <= 0)
        return;

    int blockChannels = block.getNumChannels();

    // Interleave from AudioBlock into ring buffer
    for (int f = 0; f < framesToWrite; ++f)
    {
        size_t ringFrame = (wp + static_cast<size_t> (f)) & ringMask_;
        size_t ringBase = ringFrame * static_cast<size_t> (numChannels_);

        for (int ch = 0; ch < numChannels_; ++ch)
        {
            int srcCh = (ch < blockChannels) ? ch : 0;
            ringBuffer_[ringBase + static_cast<size_t> (ch)] =
                block.getChannel (srcCh)[f];
        }
    }

    writePos_.store (wp + static_cast<size_t> (framesToWrite), std::memory_order_release);

    // Wake background thread — data available
    cv_.notify_one();
}

void ThreadedRecorder::stop()
{
    if (! recording_.exchange (false))
        return;  // wasn't recording

    cv_.notify_one();

    if (writeThread_.joinable())
        writeThread_.join();

    // Flush any remaining data in ring buffer
    size_t rp = readPos_.load (std::memory_order_relaxed);
    size_t wp = writePos_.load (std::memory_order_relaxed);
    size_t remaining = wp - rp;

    if (remaining > 0 && writer_ != nullptr)
    {
        const int chunkSize = 1024;
        std::vector<float> chunk (static_cast<size_t> (chunkSize * numChannels_));

        while (remaining > 0)
        {
            int framesToWrite = static_cast<int> (std::min (remaining,
                                                            static_cast<size_t> (chunkSize)));

            for (int f = 0; f < framesToWrite; ++f)
            {
                size_t ringFrame = (rp + static_cast<size_t> (f)) & ringMask_;
                size_t ringBase = ringFrame * static_cast<size_t> (numChannels_);

                for (int ch = 0; ch < numChannels_; ++ch)
                    chunk[static_cast<size_t> (f * numChannels_ + ch)] =
                        ringBuffer_[ringBase + static_cast<size_t> (ch)];
            }

            writer_->write (chunk.data(), framesToWrite);
            recordedSamples_.fetch_add (framesToWrite, std::memory_order_relaxed);

            rp += static_cast<size_t> (framesToWrite);
            remaining = wp - rp;
        }

        readPos_.store (rp, std::memory_order_relaxed);
    }

    writer_->close();
    writer_.reset();
    ringBuffer_.clear();
    numChannels_ = 0;
    ringCapacity_ = 0;
    ringMask_ = 0;
}

bool ThreadedRecorder::isRecording() const
{
    return recording_.load (std::memory_order_relaxed);
}

int64_t ThreadedRecorder::getRecordedSampleCount() const
{
    return recordedSamples_.load (std::memory_order_relaxed);
}

void ThreadedRecorder::writeThreadFunc()
{
    const int chunkSize = 1024;
    std::vector<float> chunk (static_cast<size_t> (chunkSize * numChannels_));

    while (recording_.load (std::memory_order_relaxed))
    {
        size_t rp = readPos_.load (std::memory_order_relaxed);
        size_t wp = writePos_.load (std::memory_order_acquire);
        size_t available = wp - rp;

        if (available == 0)
        {
            // No data — wait for producer
            std::unique_lock<std::mutex> lock (mutex_);
            cv_.wait_for (lock, std::chrono::milliseconds (5), [this]
            {
                size_t r = readPos_.load (std::memory_order_relaxed);
                size_t w = writePos_.load (std::memory_order_acquire);
                return (w - r) > 0
                    || ! recording_.load (std::memory_order_relaxed);
            });
            continue;
        }

        int framesToWrite = static_cast<int> (std::min (available,
                                                         static_cast<size_t> (chunkSize)));

        // Copy from ring buffer into contiguous interleaved chunk
        for (int f = 0; f < framesToWrite; ++f)
        {
            size_t ringFrame = (rp + static_cast<size_t> (f)) & ringMask_;
            size_t ringBase = ringFrame * static_cast<size_t> (numChannels_);

            for (int ch = 0; ch < numChannels_; ++ch)
                chunk[static_cast<size_t> (f * numChannels_ + ch)] =
                    ringBuffer_[ringBase + static_cast<size_t> (ch)];
        }

        writer_->write (chunk.data(), framesToWrite);
        recordedSamples_.fetch_add (framesToWrite, std::memory_order_relaxed);
        readPos_.store (rp + static_cast<size_t> (framesToWrite), std::memory_order_release);
    }
}

} // namespace dc

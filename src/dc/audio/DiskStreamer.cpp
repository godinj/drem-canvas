#include "DiskStreamer.h"
#include <algorithm>
#include <cstring>

namespace dc {

size_t DiskStreamer::nextPowerOf2 (size_t v)
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

DiskStreamer::DiskStreamer (int bufferSizeInFrames)
    : requestedBufferSize_ (bufferSizeInFrames)
{
}

DiskStreamer::~DiskStreamer()
{
    stop();
    close();
}

bool DiskStreamer::open (const std::filesystem::path& path)
{
    stop();
    close();

    reader_ = AudioFileReader::open (path);
    if (reader_ == nullptr)
        return false;

    numChannels_ = reader_->getNumChannels();
    ringCapacity_ = nextPowerOf2 (static_cast<size_t> (requestedBufferSize_));
    ringMask_ = ringCapacity_ - 1;

    ringBuffers_.resize (static_cast<size_t> (numChannels_));
    for (auto& buf : ringBuffers_)
        buf.resize (ringCapacity_, 0.0f);

    readPos_.store (0, std::memory_order_relaxed);
    writePos_.store (0, std::memory_order_relaxed);
    diskPosition_.store (0, std::memory_order_relaxed);
    seekTarget_.store (-1, std::memory_order_relaxed);

    return true;
}

void DiskStreamer::close()
{
    stop();

    reader_.reset();
    ringBuffers_.clear();
    numChannels_ = 0;
    ringCapacity_ = 0;
    ringMask_ = 0;
}

void DiskStreamer::seek (int64_t positionInSamples)
{
    seekTarget_.store (positionInSamples, std::memory_order_release);

    // Wake the background thread to process the seek
    cv_.notify_one();
}

int DiskStreamer::read (AudioBlock& output, int numSamples)
{
    if (reader_ == nullptr || numChannels_ == 0)
    {
        output.clear();
        return 0;
    }

    size_t wp = writePos_.load (std::memory_order_acquire);
    size_t rp = readPos_.load (std::memory_order_relaxed);
    size_t available = wp - rp;

    int framesToRead = std::min (static_cast<int> (available), numSamples);
    int outputChannels = output.getNumChannels();

    // Copy from ring buffers to output
    for (int f = 0; f < framesToRead; ++f)
    {
        size_t ringIdx = (rp + static_cast<size_t> (f)) & ringMask_;

        for (int ch = 0; ch < outputChannels; ++ch)
        {
            int srcCh = (ch < numChannels_) ? ch : 0;
            output.getChannel (ch)[f] = ringBuffers_[static_cast<size_t> (srcCh)][ringIdx];
        }
    }

    // Fill remainder with silence on underrun
    if (framesToRead < numSamples)
    {
        for (int ch = 0; ch < outputChannels; ++ch)
            std::memset (output.getChannel (ch) + framesToRead, 0,
                         sizeof (float) * static_cast<size_t> (numSamples - framesToRead));
    }

    readPos_.store (rp + static_cast<size_t> (framesToRead), std::memory_order_release);

    // Wake background thread — buffer space now available
    cv_.notify_one();

    return framesToRead;
}

void DiskStreamer::start()
{
    if (reader_ == nullptr)
        return;

    bool expected = false;
    if (! running_.compare_exchange_strong (expected, true))
        return;  // already running

    readThread_ = std::thread (&DiskStreamer::readThreadFunc, this);
}

void DiskStreamer::stop()
{
    if (! running_.exchange (false))
        return;  // wasn't running

    cv_.notify_one();

    if (readThread_.joinable())
        readThread_.join();
}

int64_t DiskStreamer::getLengthInSamples() const
{
    if (reader_ == nullptr)
        return 0;
    return reader_->getLengthInSamples();
}

double DiskStreamer::getSampleRate() const
{
    if (reader_ == nullptr)
        return 0.0;
    return reader_->getSampleRate();
}

int DiskStreamer::getNumChannels() const
{
    return numChannels_;
}

void DiskStreamer::readThreadFunc()
{
    // Temporary interleaved buffer for AudioFileReader::read()
    const int chunkSize = 1024;
    std::vector<float> interleaved (static_cast<size_t> (chunkSize * numChannels_));

    while (running_.load (std::memory_order_relaxed))
    {
        // Handle pending seek
        int64_t seekPos = seekTarget_.exchange (-1, std::memory_order_acquire);
        if (seekPos >= 0)
        {
            diskPosition_.store (seekPos, std::memory_order_relaxed);

            // Reset ring buffer positions (consumer must not be reading during seek)
            size_t pos = writePos_.load (std::memory_order_relaxed);
            readPos_.store (pos, std::memory_order_release);
        }

        // Calculate available space in ring buffer
        size_t rp = readPos_.load (std::memory_order_acquire);
        size_t wp = writePos_.load (std::memory_order_relaxed);
        size_t used = wp - rp;
        size_t space = ringCapacity_ - used;

        if (space == 0)
        {
            // Buffer full — wait for consumer to drain
            std::unique_lock<std::mutex> lock (mutex_);
            cv_.wait_for (lock, std::chrono::milliseconds (5), [this]
            {
                size_t r = readPos_.load (std::memory_order_acquire);
                size_t w = writePos_.load (std::memory_order_relaxed);
                return (ringCapacity_ - (w - r)) > 0
                    || ! running_.load (std::memory_order_relaxed)
                    || seekTarget_.load (std::memory_order_relaxed) >= 0;
            });
            continue;
        }

        int64_t diskPos = diskPosition_.load (std::memory_order_relaxed);
        int64_t fileLength = reader_->getLengthInSamples();

        if (diskPos >= fileLength)
        {
            // Reached end of file — wait for seek or stop
            std::unique_lock<std::mutex> lock (mutex_);
            cv_.wait_for (lock, std::chrono::milliseconds (50), [this]
            {
                return ! running_.load (std::memory_order_relaxed)
                    || seekTarget_.load (std::memory_order_relaxed) >= 0;
            });
            continue;
        }

        // Read a chunk from disk
        int framesToRead = std::min (chunkSize, static_cast<int> (space));
        framesToRead = static_cast<int> (
            std::min (static_cast<int64_t> (framesToRead), fileLength - diskPos));

        int64_t framesRead = reader_->read (interleaved.data(), diskPos, framesToRead);

        if (framesRead <= 0)
            continue;

        // De-interleave into per-channel ring buffers
        for (int64_t f = 0; f < framesRead; ++f)
        {
            size_t ringIdx = (wp + static_cast<size_t> (f)) & ringMask_;
            for (int ch = 0; ch < numChannels_; ++ch)
            {
                ringBuffers_[static_cast<size_t> (ch)][ringIdx] =
                    interleaved[static_cast<size_t> (f * numChannels_ + ch)];
            }
        }

        writePos_.store (wp + static_cast<size_t> (framesRead), std::memory_order_release);
        diskPosition_.store (diskPos + framesRead, std::memory_order_relaxed);
    }
}

} // namespace dc

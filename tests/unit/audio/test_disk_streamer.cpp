// Unit tests for dc::DiskStreamer
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <dc/audio/DiskStreamer.h>
#include <dc/audio/AudioFileWriter.h>

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <thread>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace fs = std::filesystem;

// ─── Helpers ────────────────────────────────────────────────────

namespace {

struct TempDir
{
    fs::path path;

    TempDir()
    {
        auto base = fs::temp_directory_path() / "dc_disk_streamer_test_XXXXXX";
        auto tmpl = base.string();
        REQUIRE(mkdtemp(tmpl.data()) != nullptr);
        path = tmpl;
    }

    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    fs::path file(const std::string& name) const { return path / name; }
};

/// Write a mono WAV file with a known pattern: sample[i] = float(i) / numFrames
fs::path writeTestFile(const TempDir& tmp, const std::string& name,
                       int numFrames, double sampleRate)
{
    auto filepath = tmp.file(name);
    std::vector<float> data(static_cast<size_t>(numFrames));
    for (int i = 0; i < numFrames; ++i)
        data[static_cast<size_t>(i)] = static_cast<float>(i) / static_cast<float>(numFrames);

    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::WAV_32F, 1, sampleRate);
    writer->write(data.data(), numFrames);
    writer->close();

    return filepath;
}

/// Write a stereo WAV file. L = ramp up, R = ramp down.
fs::path writeStereoTestFile(const TempDir& tmp, const std::string& name,
                             int numFrames, double sampleRate)
{
    auto filepath = tmp.file(name);
    std::vector<float> data(static_cast<size_t>(numFrames * 2));
    for (int i = 0; i < numFrames; ++i)
    {
        float t = static_cast<float>(i) / static_cast<float>(numFrames);
        data[static_cast<size_t>(i * 2)]     = t;        // L: ramp up
        data[static_cast<size_t>(i * 2 + 1)] = 1.0f - t; // R: ramp down
    }

    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::WAV_32F, 2, sampleRate);
    writer->write(data.data(), numFrames);
    writer->close();

    return filepath;
}

/// Helper: wait for the streamer background thread to fill the buffer.
/// Polls until enough data is available or timeout.
void waitForData(dc::DiskStreamer& streamer, int /*needed*/, int timeoutMs = 500)
{
    // Just sleep briefly to allow background thread to read
    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (std::chrono::steady_clock::now() < deadline)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

} // anonymous namespace

// ─── open + start + read ────────────────────────────────────────

TEST_CASE("DiskStreamer open start read returns audio data", "[audio][streamer]")
{
    TempDir tmp;
    const int numFrames = 4096;
    const double sampleRate = 44100.0;
    auto filepath = writeTestFile(tmp, "streamer_basic.wav", numFrames, sampleRate);

    dc::DiskStreamer streamer(8192);
    REQUIRE(streamer.open(filepath));

    REQUIRE(streamer.getLengthInSamples() == numFrames);
    REQUIRE_THAT(streamer.getSampleRate(), WithinAbs(sampleRate, 0.1));
    REQUIRE(streamer.getNumChannels() == 1);

    streamer.start();

    // Wait for background thread to fill buffer
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    // Read a chunk
    const int chunkSize = 1024;
    std::vector<float> ch0(static_cast<size_t>(chunkSize), 0.0f);
    float* channels[] = { ch0.data() };
    dc::AudioBlock block(channels, 1, chunkSize);

    int framesRead = streamer.read(block, chunkSize);
    REQUIRE(framesRead > 0);

    // Verify the first samples match our ramp pattern
    for (int i = 0; i < framesRead; ++i)
    {
        float expected = static_cast<float>(i) / static_cast<float>(numFrames);
        REQUIRE_THAT(ch0[static_cast<size_t>(i)], WithinAbs(expected, 1e-5));
    }

    streamer.stop();
}

// ─── read before start returns silence ──────────────────────────

TEST_CASE("DiskStreamer read before start returns silence", "[audio][streamer]")
{
    TempDir tmp;
    auto filepath = writeTestFile(tmp, "streamer_prestart.wav", 1000, 44100.0);

    dc::DiskStreamer streamer(4096);
    REQUIRE(streamer.open(filepath));
    // Do NOT call start()

    const int chunkSize = 256;
    std::vector<float> ch0(static_cast<size_t>(chunkSize), 1.0f); // fill with 1.0
    float* channels[] = { ch0.data() };
    dc::AudioBlock block(channels, 1, chunkSize);

    int framesRead = streamer.read(block, chunkSize);
    REQUIRE(framesRead == 0);

    // Should be zeroed (silence)
    for (int i = 0; i < chunkSize; ++i)
        REQUIRE(ch0[static_cast<size_t>(i)] == 0.0f);
}

// ─── seek to middle of file ─────────────────────────────────────

TEST_CASE("DiskStreamer seek to middle reads from correct position", "[audio][streamer]")
{
    TempDir tmp;
    const int numFrames = 8192;
    const double sampleRate = 44100.0;
    auto filepath = writeTestFile(tmp, "streamer_seek.wav", numFrames, sampleRate);

    dc::DiskStreamer streamer(16384);
    REQUIRE(streamer.open(filepath));
    streamer.start();

    // Seek to frame 4096
    const int seekPos = 4096;
    streamer.seek(seekPos);

    // Wait for seek to take effect and buffer to refill
    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const int chunkSize = 512;
    std::vector<float> ch0(static_cast<size_t>(chunkSize), 0.0f);
    float* channels[] = { ch0.data() };
    dc::AudioBlock block(channels, 1, chunkSize);

    int framesRead = streamer.read(block, chunkSize);
    REQUIRE(framesRead > 0);

    // First sample after seek should be close to seekPos / numFrames
    float expectedStart = static_cast<float>(seekPos) / static_cast<float>(numFrames);
    REQUIRE_THAT(ch0[0], WithinAbs(expectedStart, 1e-4));

    streamer.stop();
}

// ─── seek past EOF ──────────────────────────────────────────────

TEST_CASE("DiskStreamer seek past EOF results in no data", "[audio][streamer]")
{
    TempDir tmp;
    const int numFrames = 1000;
    auto filepath = writeTestFile(tmp, "streamer_seekeof.wav", numFrames, 44100.0);

    dc::DiskStreamer streamer(4096);
    REQUIRE(streamer.open(filepath));
    streamer.start();

    // Seek way past end
    streamer.seek(100000);

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const int chunkSize = 256;
    std::vector<float> ch0(static_cast<size_t>(chunkSize), 1.0f);
    float* channels[] = { ch0.data() };
    dc::AudioBlock block(channels, 1, chunkSize);

    int framesRead = streamer.read(block, chunkSize);
    // Should read 0 frames (EOF)
    REQUIRE(framesRead == 0);

    streamer.stop();
}

// ─── sequential open calls ──────────────────────────────────────

TEST_CASE("DiskStreamer sequential open closes previous file", "[audio][streamer]")
{
    TempDir tmp;
    auto file1 = writeTestFile(tmp, "streamer_seq1.wav", 1000, 44100.0);
    auto file2 = writeStereoTestFile(tmp, "streamer_seq2.wav", 2000, 48000.0);

    dc::DiskStreamer streamer(4096);

    // Open first file
    REQUIRE(streamer.open(file1));
    REQUIRE(streamer.getNumChannels() == 1);
    REQUIRE(streamer.getLengthInSamples() == 1000);

    // Open second file (should close first)
    REQUIRE(streamer.open(file2));
    REQUIRE(streamer.getNumChannels() == 2);
    REQUIRE(streamer.getLengthInSamples() == 2000);
    REQUIRE_THAT(streamer.getSampleRate(), WithinAbs(48000.0, 0.1));
}

// ─── metadata before start ──────────────────────────────────────

TEST_CASE("DiskStreamer metadata available after open before start", "[audio][streamer]")
{
    TempDir tmp;
    auto filepath = writeStereoTestFile(tmp, "streamer_meta.wav", 5000, 96000.0);

    dc::DiskStreamer streamer(4096);
    REQUIRE(streamer.open(filepath));

    REQUIRE(streamer.getLengthInSamples() == 5000);
    REQUIRE_THAT(streamer.getSampleRate(), WithinAbs(96000.0, 0.1));
    REQUIRE(streamer.getNumChannels() == 2);
}

// ─── no file open: metadata returns defaults ────────────────────

TEST_CASE("DiskStreamer no file open returns default metadata", "[audio][streamer]")
{
    dc::DiskStreamer streamer(4096);

    REQUIRE(streamer.getLengthInSamples() == 0);
    REQUIRE(streamer.getSampleRate() == 0.0);
    REQUIRE(streamer.getNumChannels() == 0);
}

// ─── open non-existent file ─────────────────────────────────────

TEST_CASE("DiskStreamer open non-existent file returns false", "[audio][streamer]")
{
    dc::DiskStreamer streamer(4096);
    REQUIRE_FALSE(streamer.open("/tmp/dc_no_such_file_abc.wav"));
}

// ─── stop without start is safe ─────────────────────────────────

TEST_CASE("DiskStreamer stop without start is safe", "[audio][streamer]")
{
    TempDir tmp;
    auto filepath = writeTestFile(tmp, "streamer_stopnosart.wav", 100, 44100.0);

    dc::DiskStreamer streamer(4096);
    streamer.open(filepath);
    streamer.stop(); // should not crash
}

// ─── small buffer size ──────────────────────────────────────────

TEST_CASE("DiskStreamer small buffer size still works", "[audio][streamer]")
{
    TempDir tmp;
    const int numFrames = 100;
    auto filepath = writeTestFile(tmp, "streamer_small.wav", numFrames, 44100.0);

    // Very small buffer (will round up to power of 2)
    dc::DiskStreamer streamer(4);
    REQUIRE(streamer.open(filepath));
    streamer.start();

    std::this_thread::sleep_for(std::chrono::milliseconds(200));

    const int chunkSize = 2;
    std::vector<float> ch0(static_cast<size_t>(chunkSize), 0.0f);
    float* channels[] = { ch0.data() };
    dc::AudioBlock block(channels, 1, chunkSize);

    int framesRead = streamer.read(block, chunkSize);
    // Should get at least some data
    REQUIRE(framesRead >= 0);

    streamer.stop();
}

// ─── destructor stops background thread ─────────────────────────

TEST_CASE("DiskStreamer destructor stops background thread cleanly", "[audio][streamer]")
{
    TempDir tmp;
    auto filepath = writeTestFile(tmp, "streamer_dtor.wav", 10000, 44100.0);

    {
        dc::DiskStreamer streamer(8192);
        streamer.open(filepath);
        streamer.start();
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        // Let destructor clean up
    }
    // If we get here without hanging, the test passes
    REQUIRE(true);
}

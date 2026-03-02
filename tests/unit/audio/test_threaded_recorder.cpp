// Unit tests for dc::ThreadedRecorder
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <dc/audio/ThreadedRecorder.h>
#include <dc/audio/AudioFileReader.h>
#include <dc/audio/AudioBlock.h>

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
        auto base = fs::temp_directory_path() / "dc_recorder_test_XXXXXX";
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

/// Build a mono AudioBlock from a vector of samples
struct MonoBlock
{
    std::vector<float> data;
    float* channels[1];

    explicit MonoBlock(int numSamples, float fillValue = 0.0f)
        : data(static_cast<size_t>(numSamples), fillValue)
    {
        channels[0] = data.data();
    }

    dc::AudioBlock block()
    {
        return dc::AudioBlock(channels, 1, static_cast<int>(data.size()));
    }
};

/// Build a stereo AudioBlock
struct StereoBlock
{
    std::vector<float> left;
    std::vector<float> right;
    float* channels[2];

    StereoBlock(int numSamples, float leftVal, float rightVal)
        : left(static_cast<size_t>(numSamples), leftVal)
        , right(static_cast<size_t>(numSamples), rightVal)
    {
        channels[0] = left.data();
        channels[1] = right.data();
    }

    dc::AudioBlock block()
    {
        return dc::AudioBlock(channels, 2, static_cast<int>(left.size()));
    }
};

} // anonymous namespace

// ─── Record audio, stop, verify file ────────────────────────────

TEST_CASE("ThreadedRecorder record audio then verify output", "[audio][recorder]")
{
    TempDir tmp;
    auto filepath = tmp.file("recorded.wav");
    const double sampleRate = 44100.0;
    const int blockSize = 512;
    const int numBlocks = 10;
    const int totalFrames = blockSize * numBlocks;

    dc::ThreadedRecorder recorder(8192);
    REQUIRE(recorder.start(filepath,
        dc::AudioFileWriter::Format::WAV_32F, 1, sampleRate));
    REQUIRE(recorder.isRecording());

    // Write known ramp pattern: sample[i] = i / totalFrames
    for (int b = 0; b < numBlocks; ++b)
    {
        MonoBlock block(blockSize);
        for (int i = 0; i < blockSize; ++i)
        {
            int globalIdx = b * blockSize + i;
            block.data[static_cast<size_t>(i)] =
                static_cast<float>(globalIdx) / static_cast<float>(totalFrames);
        }
        recorder.write(block.block(), blockSize);

        // Brief pause to let background thread drain
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    recorder.stop();
    REQUIRE_FALSE(recorder.isRecording());

    // Verify recorded file
    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->getNumChannels() == 1);
    REQUIRE(reader->getLengthInSamples() == totalFrames);
    REQUIRE_THAT(reader->getSampleRate(), WithinAbs(sampleRate, 0.1));

    std::vector<float> readBack(static_cast<size_t>(totalFrames));
    auto framesRead = reader->read(readBack.data(), 0, totalFrames);
    REQUIRE(framesRead == totalFrames);

    // Verify samples (WAV 32-float = bit-exact)
    for (int i = 0; i < totalFrames; ++i)
    {
        float expected = static_cast<float>(i) / static_cast<float>(totalFrames);
        REQUIRE(readBack[static_cast<size_t>(i)] == expected);
    }
}

// ─── Stereo recording ───────────────────────────────────────────

TEST_CASE("ThreadedRecorder stereo recording", "[audio][recorder]")
{
    TempDir tmp;
    auto filepath = tmp.file("stereo_rec.wav");
    const int blockSize = 256;

    dc::ThreadedRecorder recorder(4096);
    REQUIRE(recorder.start(filepath,
        dc::AudioFileWriter::Format::WAV_32F, 2, 48000.0));

    // Write stereo: L=0.25, R=0.75
    StereoBlock block(blockSize, 0.25f, 0.75f);
    recorder.write(block.block(), blockSize);

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    recorder.stop();

    // Verify
    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->getNumChannels() == 2);

    std::vector<float> interleaved(static_cast<size_t>(blockSize * 2));
    reader->read(interleaved.data(), 0, blockSize);

    for (int i = 0; i < blockSize; ++i)
    {
        REQUIRE(interleaved[static_cast<size_t>(i * 2)] == 0.25f);
        REQUIRE(interleaved[static_cast<size_t>(i * 2 + 1)] == 0.75f);
    }
}

// ─── write never blocks ─────────────────────────────────────────

TEST_CASE("ThreadedRecorder write returns quickly (non-blocking)", "[audio][recorder]")
{
    TempDir tmp;
    auto filepath = tmp.file("nonblock.wav");

    dc::ThreadedRecorder recorder(4096);
    REQUIRE(recorder.start(filepath,
        dc::AudioFileWriter::Format::WAV_16, 1, 44100.0));

    MonoBlock block(512, 0.5f);

    auto startTime = std::chrono::steady_clock::now();
    for (int i = 0; i < 100; ++i)
        recorder.write(block.block(), 512);
    auto elapsed = std::chrono::steady_clock::now() - startTime;

    // 100 write calls should complete in well under 100ms
    // (if blocking, each would wait for disk I/O)
    REQUIRE(std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count() < 100);

    recorder.stop();
}

// ─── stop without start ─────────────────────────────────────────

TEST_CASE("ThreadedRecorder stop without start does not crash", "[audio][recorder]")
{
    dc::ThreadedRecorder recorder(4096);
    REQUIRE_FALSE(recorder.isRecording());
    recorder.stop(); // should not crash
    REQUIRE_FALSE(recorder.isRecording());
}

// ─── stop flushes remaining data ────────────────────────────────

TEST_CASE("ThreadedRecorder stop flushes remaining data", "[audio][recorder]")
{
    TempDir tmp;
    auto filepath = tmp.file("flush.wav");
    const int blockSize = 1024;

    dc::ThreadedRecorder recorder(8192);
    REQUIRE(recorder.start(filepath,
        dc::AudioFileWriter::Format::WAV_32F, 1, 44100.0));

    // Write a burst of data
    MonoBlock block(blockSize, 0.42f);
    recorder.write(block.block(), blockSize);

    // Stop immediately (some data may still be in ring buffer)
    recorder.stop();

    // Verify file has the correct number of frames
    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->getLengthInSamples() == blockSize);

    // Verify content
    std::vector<float> readBack(static_cast<size_t>(blockSize));
    reader->read(readBack.data(), 0, blockSize);
    for (int i = 0; i < blockSize; ++i)
        REQUIRE(readBack[static_cast<size_t>(i)] == 0.42f);
}

// ─── getRecordedSampleCount ─────────────────────────────────────

TEST_CASE("ThreadedRecorder getRecordedSampleCount reflects written frames", "[audio][recorder]")
{
    TempDir tmp;
    auto filepath = tmp.file("samplecount.wav");
    const int blockSize = 256;
    const int numBlocks = 5;

    dc::ThreadedRecorder recorder(4096);
    REQUIRE(recorder.start(filepath,
        dc::AudioFileWriter::Format::WAV_16, 1, 44100.0));

    for (int b = 0; b < numBlocks; ++b)
    {
        MonoBlock block(blockSize, 0.1f);
        recorder.write(block.block(), blockSize);
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }

    recorder.stop();

    // After stop + flush, count should equal total written
    REQUIRE(recorder.getRecordedSampleCount() == blockSize * numBlocks);
}

// ─── write when not recording is ignored ────────────────────────

TEST_CASE("ThreadedRecorder write when not recording is ignored", "[audio][recorder]")
{
    dc::ThreadedRecorder recorder(4096);
    // Not started — write should silently do nothing
    MonoBlock block(256, 0.5f);
    recorder.write(block.block(), 256); // should not crash
}

// ─── destructor stops and flushes ───────────────────────────────

TEST_CASE("ThreadedRecorder destructor stops recording cleanly", "[audio][recorder]")
{
    TempDir tmp;
    auto filepath = tmp.file("dtor.wav");

    {
        dc::ThreadedRecorder recorder(4096);
        recorder.start(filepath,
            dc::AudioFileWriter::Format::WAV_16, 1, 44100.0);

        MonoBlock block(512, 0.3f);
        recorder.write(block.block(), 512);

        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        // Destructor should stop + flush
    }

    // File should exist and be readable
    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->getLengthInSamples() > 0);
}

// ─── start after stop (re-use) ──────────────────────────────────

TEST_CASE("ThreadedRecorder can be re-used after stop", "[audio][recorder]")
{
    TempDir tmp;
    auto filepath1 = tmp.file("reuse1.wav");
    auto filepath2 = tmp.file("reuse2.wav");

    dc::ThreadedRecorder recorder(4096);

    // First recording
    REQUIRE(recorder.start(filepath1,
        dc::AudioFileWriter::Format::WAV_32F, 1, 44100.0));
    MonoBlock block1(256, 0.1f);
    recorder.write(block1.block(), 256);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    recorder.stop();

    // Second recording
    REQUIRE(recorder.start(filepath2,
        dc::AudioFileWriter::Format::WAV_32F, 1, 48000.0));
    MonoBlock block2(512, 0.9f);
    recorder.write(block2.block(), 512);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    recorder.stop();

    // Verify both files
    auto reader1 = dc::AudioFileReader::open(filepath1);
    REQUIRE(reader1 != nullptr);
    REQUIRE(reader1->getLengthInSamples() == 256);

    auto reader2 = dc::AudioFileReader::open(filepath2);
    REQUIRE(reader2 != nullptr);
    REQUIRE(reader2->getLengthInSamples() == 512);
    REQUIRE_THAT(reader2->getSampleRate(), WithinAbs(48000.0, 0.1));
}

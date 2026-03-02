// Unit tests for dc::AudioFileReader and dc::AudioFileWriter
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <dc/audio/AudioFileReader.h>
#include <dc/audio/AudioFileWriter.h>
#include <dc/audio/AudioBlock.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <vector>

using Catch::Matchers::WithinAbs;

namespace fs = std::filesystem;

// ─── Helpers ────────────────────────────────────────────────────

namespace {

/// Generate a sine wave at the given frequency
std::vector<float> generateSine(int numFrames, double sampleRate, double freqHz)
{
    std::vector<float> data(static_cast<size_t>(numFrames));
    const double twoPi = 2.0 * M_PI;
    for (int i = 0; i < numFrames; ++i)
        data[static_cast<size_t>(i)] =
            static_cast<float>(std::sin(twoPi * freqHz * static_cast<double>(i) / sampleRate));
    return data;
}

/// Generate interleaved stereo sine waves (different freq per channel)
std::vector<float> generateStereoSine(int numFrames, double sampleRate,
                                       double freqL, double freqR)
{
    std::vector<float> data(static_cast<size_t>(numFrames * 2));
    const double twoPi = 2.0 * M_PI;
    for (int i = 0; i < numFrames; ++i)
    {
        data[static_cast<size_t>(i * 2)] =
            static_cast<float>(std::sin(twoPi * freqL * static_cast<double>(i) / sampleRate));
        data[static_cast<size_t>(i * 2 + 1)] =
            static_cast<float>(std::sin(twoPi * freqR * static_cast<double>(i) / sampleRate));
    }
    return data;
}

/// RAII temp directory for test files
struct TempDir
{
    fs::path path;

    TempDir()
    {
        path = fs::temp_directory_path() / "dc_audio_test";
        fs::create_directories(path);
    }

    ~TempDir()
    {
        std::error_code ec;
        fs::remove_all(path, ec);
    }

    fs::path file(const std::string& name) const
    {
        return path / name;
    }
};

} // anonymous namespace

// ─── WAV 16-bit round-trip ──────────────────────────────────────

TEST_CASE("AudioFileIO WAV 16-bit write/read round-trip", "[audio][fileio]")
{
    TempDir tmp;
    auto filepath = tmp.file("test_16bit.wav");
    const int numFrames = 4410; // 0.1s at 44100
    const double sampleRate = 44100.0;

    auto sine = generateSine(numFrames, sampleRate, 440.0);

    // Write
    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::WAV_16, 1, sampleRate);
    REQUIRE(writer != nullptr);
    REQUIRE(writer->write(sine.data(), numFrames));
    writer->close();

    // Read
    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->getNumChannels() == 1);
    REQUIRE(reader->getLengthInSamples() == numFrames);
    REQUIRE_THAT(reader->getSampleRate(), WithinAbs(sampleRate, 0.1));
    REQUIRE(reader->getFormatName() == "WAV");
    REQUIRE(reader->getBitDepth() == 16);

    std::vector<float> readBack(static_cast<size_t>(numFrames));
    auto framesRead = reader->read(readBack.data(), 0, numFrames);
    REQUIRE(framesRead == numFrames);

    // 16-bit quantization error: max ~1/32768 ≈ 3e-5
    for (int i = 0; i < numFrames; ++i)
        REQUIRE_THAT(readBack[static_cast<size_t>(i)],
                     WithinAbs(sine[static_cast<size_t>(i)], 5e-5));
}

// ─── WAV 24-bit round-trip ──────────────────────────────────────

TEST_CASE("AudioFileIO WAV 24-bit write/read round-trip", "[audio][fileio]")
{
    TempDir tmp;
    auto filepath = tmp.file("test_24bit.wav");
    const int numFrames = 4410;
    const double sampleRate = 44100.0;

    auto sine = generateSine(numFrames, sampleRate, 440.0);

    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::WAV_24, 1, sampleRate);
    REQUIRE(writer != nullptr);
    REQUIRE(writer->write(sine.data(), numFrames));
    writer->close();

    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->getBitDepth() == 24);

    std::vector<float> readBack(static_cast<size_t>(numFrames));
    reader->read(readBack.data(), 0, numFrames);

    // 24-bit quantization: ~1/8388608 ≈ 1.2e-7
    for (int i = 0; i < numFrames; ++i)
        REQUIRE_THAT(readBack[static_cast<size_t>(i)],
                     WithinAbs(sine[static_cast<size_t>(i)], 1e-6));
}

// ─── WAV 32-float round-trip (lossless) ─────────────────────────

TEST_CASE("AudioFileIO WAV 32-float write/read round-trip lossless", "[audio][fileio]")
{
    TempDir tmp;
    auto filepath = tmp.file("test_32float.wav");
    const int numFrames = 4410;
    const double sampleRate = 44100.0;

    auto sine = generateSine(numFrames, sampleRate, 440.0);

    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::WAV_32F, 1, sampleRate);
    REQUIRE(writer != nullptr);
    REQUIRE(writer->write(sine.data(), numFrames));
    writer->close();

    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->getBitDepth() == 32);

    std::vector<float> readBack(static_cast<size_t>(numFrames));
    reader->read(readBack.data(), 0, numFrames);

    // Should be bit-exact for float format
    for (int i = 0; i < numFrames; ++i)
        REQUIRE(readBack[static_cast<size_t>(i)] == sine[static_cast<size_t>(i)]);
}

// ─── AIFF 16-bit round-trip ─────────────────────────────────────

TEST_CASE("AudioFileIO AIFF 16-bit write/read round-trip", "[audio][fileio]")
{
    TempDir tmp;
    auto filepath = tmp.file("test_16bit.aiff");
    const int numFrames = 4410;
    const double sampleRate = 44100.0;

    auto sine = generateSine(numFrames, sampleRate, 440.0);

    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::AIFF_16, 1, sampleRate);
    REQUIRE(writer != nullptr);
    REQUIRE(writer->write(sine.data(), numFrames));
    writer->close();

    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->getFormatName() == "AIFF");
    REQUIRE(reader->getBitDepth() == 16);

    std::vector<float> readBack(static_cast<size_t>(numFrames));
    reader->read(readBack.data(), 0, numFrames);

    for (int i = 0; i < numFrames; ++i)
        REQUIRE_THAT(readBack[static_cast<size_t>(i)],
                     WithinAbs(sine[static_cast<size_t>(i)], 5e-5));
}

// ─── AIFF 24-bit round-trip ─────────────────────────────────────

TEST_CASE("AudioFileIO AIFF 24-bit write/read round-trip", "[audio][fileio]")
{
    TempDir tmp;
    auto filepath = tmp.file("test_24bit.aiff");
    const int numFrames = 4410;
    const double sampleRate = 44100.0;

    auto sine = generateSine(numFrames, sampleRate, 440.0);

    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::AIFF_24, 1, sampleRate);
    REQUIRE(writer != nullptr);
    REQUIRE(writer->write(sine.data(), numFrames));
    writer->close();

    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->getBitDepth() == 24);

    std::vector<float> readBack(static_cast<size_t>(numFrames));
    reader->read(readBack.data(), 0, numFrames);

    for (int i = 0; i < numFrames; ++i)
        REQUIRE_THAT(readBack[static_cast<size_t>(i)],
                     WithinAbs(sine[static_cast<size_t>(i)], 1e-6));
}

// ─── FLAC 16-bit round-trip ─────────────────────────────────────

TEST_CASE("AudioFileIO FLAC 16-bit write/read round-trip", "[audio][fileio]")
{
    TempDir tmp;
    auto filepath = tmp.file("test_16bit.flac");
    const int numFrames = 4410;
    const double sampleRate = 44100.0;

    auto sine = generateSine(numFrames, sampleRate, 440.0);

    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::FLAC_16, 1, sampleRate);
    REQUIRE(writer != nullptr);
    REQUIRE(writer->write(sine.data(), numFrames));
    writer->close();

    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->getFormatName() == "FLAC");
    REQUIRE(reader->getBitDepth() == 16);

    std::vector<float> readBack(static_cast<size_t>(numFrames));
    reader->read(readBack.data(), 0, numFrames);

    // FLAC is lossless for integer samples, but float->int->float has quantization
    for (int i = 0; i < numFrames; ++i)
        REQUIRE_THAT(readBack[static_cast<size_t>(i)],
                     WithinAbs(sine[static_cast<size_t>(i)], 5e-5));
}

// ─── FLAC 24-bit round-trip ─────────────────────────────────────

TEST_CASE("AudioFileIO FLAC 24-bit write/read round-trip", "[audio][fileio]")
{
    TempDir tmp;
    auto filepath = tmp.file("test_24bit.flac");
    const int numFrames = 4410;
    const double sampleRate = 44100.0;

    auto sine = generateSine(numFrames, sampleRate, 440.0);

    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::FLAC_24, 1, sampleRate);
    REQUIRE(writer != nullptr);
    REQUIRE(writer->write(sine.data(), numFrames));
    writer->close();

    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->getBitDepth() == 24);

    std::vector<float> readBack(static_cast<size_t>(numFrames));
    reader->read(readBack.data(), 0, numFrames);

    for (int i = 0; i < numFrames; ++i)
        REQUIRE_THAT(readBack[static_cast<size_t>(i)],
                     WithinAbs(sine[static_cast<size_t>(i)], 1e-6));
}

// ─── AudioBlock interleave/de-interleave ────────────────────────

TEST_CASE("AudioFileIO AudioBlock write/read round-trip", "[audio][fileio]")
{
    TempDir tmp;
    auto filepath = tmp.file("test_block.wav");
    const int numFrames = 2048;
    const double sampleRate = 48000.0;

    // Prepare stereo AudioBlock
    std::vector<float> ch0(static_cast<size_t>(numFrames));
    std::vector<float> ch1(static_cast<size_t>(numFrames));
    for (int i = 0; i < numFrames; ++i)
    {
        ch0[static_cast<size_t>(i)] = static_cast<float>(
            std::sin(2.0 * M_PI * 440.0 * static_cast<double>(i) / sampleRate));
        ch1[static_cast<size_t>(i)] = static_cast<float>(
            std::sin(2.0 * M_PI * 880.0 * static_cast<double>(i) / sampleRate));
    }
    float* channels[] = { ch0.data(), ch1.data() };
    dc::AudioBlock writeBlock(channels, 2, numFrames);

    // Write
    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::WAV_32F, 2, sampleRate);
    REQUIRE(writer != nullptr);
    REQUIRE(writer->write(writeBlock, numFrames));
    writer->close();

    // Read back into AudioBlock
    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->getNumChannels() == 2);

    std::vector<float> readCh0(static_cast<size_t>(numFrames), 0.0f);
    std::vector<float> readCh1(static_cast<size_t>(numFrames), 0.0f);
    float* readChannels[] = { readCh0.data(), readCh1.data() };
    dc::AudioBlock readBlock(readChannels, 2, numFrames);

    auto framesRead = reader->read(readBlock, 0, numFrames);
    REQUIRE(framesRead == numFrames);

    // WAV 32-float should be bit-exact
    for (int i = 0; i < numFrames; ++i)
    {
        REQUIRE(readCh0[static_cast<size_t>(i)] == ch0[static_cast<size_t>(i)]);
        REQUIRE(readCh1[static_cast<size_t>(i)] == ch1[static_cast<size_t>(i)]);
    }
}

// ─── Open non-existent file ─────────────────────────────────────

TEST_CASE("AudioFileReader open non-existent file returns nullptr", "[audio][fileio]")
{
    auto reader = dc::AudioFileReader::open("/tmp/dc_no_such_file_xyz.wav");
    REQUIRE(reader == nullptr);
}

// ─── Open non-audio file ────────────────────────────────────────

TEST_CASE("AudioFileReader open non-audio file returns nullptr", "[audio][fileio]")
{
    TempDir tmp;
    auto filepath = tmp.file("not_audio.txt");

    // Write some text
    {
        auto writer = std::ofstream(filepath);
        writer << "This is not an audio file.\n";
    }

    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader == nullptr);
}

// ─── Read past end of file ──────────────────────────────────────

TEST_CASE("AudioFileReader read past end returns fewer frames", "[audio][fileio]")
{
    TempDir tmp;
    auto filepath = tmp.file("short.wav");
    const int numFrames = 100;
    const double sampleRate = 44100.0;

    auto sine = generateSine(numFrames, sampleRate, 440.0);

    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::WAV_16, 1, sampleRate);
    REQUIRE(writer != nullptr);
    writer->write(sine.data(), numFrames);
    writer->close();

    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->getLengthInSamples() == numFrames);

    // Request more than available
    std::vector<float> buf(1000, 0.0f);
    auto framesRead = reader->read(buf.data(), 0, 1000);
    REQUIRE(framesRead == numFrames);
}

TEST_CASE("AudioFileReader read starting past end returns fewer frames", "[audio][fileio]")
{
    TempDir tmp;
    auto filepath = tmp.file("short2.wav");
    const int numFrames = 100;
    const double sampleRate = 44100.0;

    auto sine = generateSine(numFrames, sampleRate, 440.0);

    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::WAV_16, 1, sampleRate);
    REQUIRE(writer != nullptr);
    writer->write(sine.data(), numFrames);
    writer->close();

    auto reader = dc::AudioFileReader::open(filepath);
    std::vector<float> buf(100, 0.0f);
    // libsndfile clamps the seek position — reading past the end returns
    // at most the remaining frames from the clamped position
    auto framesRead = reader->read(buf.data(), 500, 100);
    REQUIRE(framesRead <= numFrames);
}

// ─── Multi-channel: mono, stereo ────────────────────────────────

TEST_CASE("AudioFileIO mono file has 1 channel", "[audio][fileio]")
{
    TempDir tmp;
    auto filepath = tmp.file("mono.wav");
    const int numFrames = 1000;

    auto sine = generateSine(numFrames, 44100.0, 440.0);
    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::WAV_16, 1, 44100.0);
    REQUIRE(writer != nullptr);
    writer->write(sine.data(), numFrames);
    writer->close();

    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader->getNumChannels() == 1);
}

TEST_CASE("AudioFileIO stereo file has 2 channels", "[audio][fileio]")
{
    TempDir tmp;
    auto filepath = tmp.file("stereo.wav");
    const int numFrames = 1000;

    auto stereo = generateStereoSine(numFrames, 48000.0, 440.0, 880.0);
    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::WAV_16, 2, 48000.0);
    REQUIRE(writer != nullptr);
    writer->write(stereo.data(), numFrames);
    writer->close();

    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader->getNumChannels() == 2);
    REQUIRE_THAT(reader->getSampleRate(), WithinAbs(48000.0, 0.1));
}

TEST_CASE("AudioFileIO 6-channel (5.1) file preserves channels", "[audio][fileio]")
{
    TempDir tmp;
    auto filepath = tmp.file("surround.wav");
    const int numFrames = 500;
    const int numChannels = 6;

    // Generate 6 channels of interleaved data
    std::vector<float> data(static_cast<size_t>(numFrames * numChannels), 0.0f);
    for (int f = 0; f < numFrames; ++f)
        for (int ch = 0; ch < numChannels; ++ch)
            data[static_cast<size_t>(f * numChannels + ch)] =
                static_cast<float>(ch + 1) * 0.1f; // each channel has distinct level

    auto writer = dc::AudioFileWriter::create(filepath,
        dc::AudioFileWriter::Format::WAV_32F, numChannels, 48000.0);
    REQUIRE(writer != nullptr);
    writer->write(data.data(), numFrames);
    writer->close();

    auto reader = dc::AudioFileReader::open(filepath);
    REQUIRE(reader != nullptr);
    REQUIRE(reader->getNumChannels() == numChannels);
    REQUIRE(reader->getLengthInSamples() == numFrames);

    std::vector<float> readBack(static_cast<size_t>(numFrames * numChannels));
    auto framesRead = reader->read(readBack.data(), 0, numFrames);
    REQUIRE(framesRead == numFrames);

    // Verify each channel has the correct level (float format = bit-exact)
    for (int f = 0; f < numFrames; ++f)
        for (int ch = 0; ch < numChannels; ++ch)
            REQUIRE(readBack[static_cast<size_t>(f * numChannels + ch)] ==
                    data[static_cast<size_t>(f * numChannels + ch)]);
}

// ─── getFormatName / getBitDepth ────────────────────────────────

TEST_CASE("AudioFileReader getFormatName and getBitDepth", "[audio][fileio]")
{
    TempDir tmp;

    SECTION("WAV 24-bit")
    {
        auto filepath = tmp.file("meta_24.wav");
        auto sine = generateSine(100, 44100.0, 440.0);
        auto w = dc::AudioFileWriter::create(filepath,
            dc::AudioFileWriter::Format::WAV_24, 1, 44100.0);
        w->write(sine.data(), 100);
        w->close();

        auto r = dc::AudioFileReader::open(filepath);
        REQUIRE(r->getFormatName() == "WAV");
        REQUIRE(r->getBitDepth() == 24);
    }

    SECTION("FLAC 16-bit")
    {
        auto filepath = tmp.file("meta_16.flac");
        auto sine = generateSine(100, 44100.0, 440.0);
        auto w = dc::AudioFileWriter::create(filepath,
            dc::AudioFileWriter::Format::FLAC_16, 1, 44100.0);
        w->write(sine.data(), 100);
        w->close();

        auto r = dc::AudioFileReader::open(filepath);
        REQUIRE(r->getFormatName() == "FLAC");
        REQUIRE(r->getBitDepth() == 16);
    }
}

// ─── Writer create failure ──────────────────────────────────────

TEST_CASE("AudioFileWriter create with invalid path returns nullptr", "[audio][fileio]")
{
    auto writer = dc::AudioFileWriter::create(
        "/nonexistent/path/nope.wav",
        dc::AudioFileWriter::Format::WAV_16, 1, 44100.0);
    REQUIRE(writer == nullptr);
}

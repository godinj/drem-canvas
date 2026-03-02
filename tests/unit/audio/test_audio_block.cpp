// Unit tests for dc::AudioBlock
#include <catch2/catch_test_macros.hpp>
#include <dc/audio/AudioBlock.h>

#include <cmath>
#include <vector>

// ─── Helper: allocate channel data ──────────────────────────────

struct TestBuffer
{
    std::vector<std::vector<float>> channels;
    std::vector<float*> ptrs;

    TestBuffer(int numChannels, int numSamples, float fillValue = 0.0f)
    {
        channels.resize(static_cast<size_t>(numChannels),
                        std::vector<float>(static_cast<size_t>(numSamples), fillValue));
        ptrs.resize(static_cast<size_t>(numChannels));
        for (int ch = 0; ch < numChannels; ++ch)
            ptrs[static_cast<size_t>(ch)] = channels[static_cast<size_t>(ch)].data();
    }

    float** data() { return ptrs.data(); }
};

// ─── getChannel returns correct pointer ─────────────────────────

TEST_CASE("AudioBlock getChannel returns correct pointer", "[audio][block]")
{
    TestBuffer buf(2, 128);
    dc::AudioBlock block(buf.data(), 2, 128);

    REQUIRE(block.getChannel(0) == buf.ptrs[0]);
    REQUIRE(block.getChannel(1) == buf.ptrs[1]);
}

TEST_CASE("AudioBlock getChannel on const block returns const pointer", "[audio][block]")
{
    TestBuffer buf(2, 128);
    const dc::AudioBlock block(buf.data(), 2, 128);

    const float* ch0 = block.getChannel(0);
    REQUIRE(ch0 == buf.ptrs[0]);
}

// ─── getNumChannels / getNumSamples ─────────────────────────────

TEST_CASE("AudioBlock getNumChannels and getNumSamples match constructor", "[audio][block]")
{
    TestBuffer buf(4, 256);
    dc::AudioBlock block(buf.data(), 4, 256);

    REQUIRE(block.getNumChannels() == 4);
    REQUIRE(block.getNumSamples() == 256);
}

// ─── clear ──────────────────────────────────────────────────────

TEST_CASE("AudioBlock clear zeros all samples", "[audio][block]")
{
    TestBuffer buf(2, 64, 1.0f); // fill with 1.0
    dc::AudioBlock block(buf.data(), 2, 64);

    // Verify non-zero before clear
    REQUIRE(buf.channels[0][0] == 1.0f);
    REQUIRE(buf.channels[1][0] == 1.0f);

    block.clear();

    for (int ch = 0; ch < 2; ++ch)
        for (int s = 0; s < 64; ++s)
            REQUIRE(block.getChannel(ch)[s] == 0.0f);
}

TEST_CASE("AudioBlock clear with non-trivial data", "[audio][block]")
{
    TestBuffer buf(3, 128);
    // Fill with various values
    for (int ch = 0; ch < 3; ++ch)
        for (int s = 0; s < 128; ++s)
            buf.channels[static_cast<size_t>(ch)][static_cast<size_t>(s)] =
                static_cast<float>(ch * 128 + s) / 384.0f;

    dc::AudioBlock block(buf.data(), 3, 128);
    block.clear();

    for (int ch = 0; ch < 3; ++ch)
        for (int s = 0; s < 128; ++s)
            REQUIRE(block.getChannel(ch)[s] == 0.0f);
}

// ─── Zero channels ──────────────────────────────────────────────

TEST_CASE("AudioBlock zero channels valid construction", "[audio][block]")
{
    float** nullChannels = nullptr;
    dc::AudioBlock block(nullChannels, 0, 128);
    REQUIRE(block.getNumChannels() == 0);
    REQUIRE(block.getNumSamples() == 128);
    // clear() on zero channels should not crash
    block.clear();
}

// ─── Zero samples ───────────────────────────────────────────────

TEST_CASE("AudioBlock zero samples valid construction", "[audio][block]")
{
    TestBuffer buf(2, 0);
    dc::AudioBlock block(buf.data(), 2, 0);
    REQUIRE(block.getNumChannels() == 2);
    REQUIRE(block.getNumSamples() == 0);
    // clear() on zero samples should not crash
    block.clear();
}

// ─── Non-owning: modifications visible through original pointers

TEST_CASE("AudioBlock is non-owning: writes visible through original", "[audio][block]")
{
    TestBuffer buf(1, 4);
    dc::AudioBlock block(buf.data(), 1, 4);

    // Write through block
    block.getChannel(0)[0] = 0.5f;
    block.getChannel(0)[1] = -0.25f;
    block.getChannel(0)[2] = 1.0f;
    block.getChannel(0)[3] = -1.0f;

    // Read through original buffer
    REQUIRE(buf.channels[0][0] == 0.5f);
    REQUIRE(buf.channels[0][1] == -0.25f);
    REQUIRE(buf.channels[0][2] == 1.0f);
    REQUIRE(buf.channels[0][3] == -1.0f);
}

// ─── float* const* constructor ──────────────────────────────────

TEST_CASE("AudioBlock constructed from float* const* works", "[audio][block]")
{
    TestBuffer buf(2, 64, 0.42f);
    float* const* constPtrs = buf.data();
    dc::AudioBlock block(constPtrs, 2, 64);

    REQUIRE(block.getNumChannels() == 2);
    REQUIRE(block.getNumSamples() == 64);
    REQUIRE(block.getChannel(0)[0] == 0.42f);
}

// ─── Mono and multi-channel ─────────────────────────────────────

TEST_CASE("AudioBlock mono channel", "[audio][block]")
{
    TestBuffer buf(1, 512);
    dc::AudioBlock block(buf.data(), 1, 512);

    REQUIRE(block.getNumChannels() == 1);
    block.getChannel(0)[0] = 0.99f;
    REQUIRE(block.getChannel(0)[0] == 0.99f);
}

TEST_CASE("AudioBlock six channels (5.1 surround)", "[audio][block]")
{
    TestBuffer buf(6, 256, 0.1f);
    dc::AudioBlock block(buf.data(), 6, 256);

    REQUIRE(block.getNumChannels() == 6);
    for (int ch = 0; ch < 6; ++ch)
        REQUIRE(block.getChannel(ch)[0] == 0.1f);
}

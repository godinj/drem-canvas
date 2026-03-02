// Unit tests for dc::MidiSequence
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <dc/midi/MidiSequence.h>

#include <cstring>
#include <vector>

using Catch::Matchers::WithinAbs;

// ─── addEvent maintains sorted order ────────────────────────────

TEST_CASE("MidiSequence addEvent maintains sorted order", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 3.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 64, 0.8f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 67, 0.8f), 2.0);

    REQUIRE(seq.getNumEvents() == 3);
    REQUIRE_THAT(seq.getEvent(0).timeInBeats, WithinAbs(1.0, 1e-9));
    REQUIRE_THAT(seq.getEvent(1).timeInBeats, WithinAbs(2.0, 1e-9));
    REQUIRE_THAT(seq.getEvent(2).timeInBeats, WithinAbs(3.0, 1e-9));
}

TEST_CASE("MidiSequence addEvent at same time preserves order", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 64, 0.7f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 67, 0.6f), 1.0);

    REQUIRE(seq.getNumEvents() == 3);
    // All at same time
    for (int i = 0; i < 3; ++i)
        REQUIRE_THAT(seq.getEvent(i).timeInBeats, WithinAbs(1.0, 1e-9));
}

// ─── updateMatchedPairs ─────────────────────────────────────────

TEST_CASE("MidiSequence updateMatchedPairs links noteOn/Off", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOff(1, 60), 2.0);

    seq.updateMatchedPairs();

    REQUIRE(seq.getEvent(0).matchedPairIndex == 1);
    REQUIRE(seq.getEvent(1).matchedPairIndex == 0);
}

TEST_CASE("MidiSequence updateMatchedPairs: unmatched noteOn stays -1", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    // No noteOff

    seq.updateMatchedPairs();

    REQUIRE(seq.getEvent(0).matchedPairIndex == -1);
}

TEST_CASE("MidiSequence updateMatchedPairs: multiple notes", "[midi][sequence]")
{
    dc::MidiSequence seq;
    // Note 60: on at 1.0, off at 2.0
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    // Note 64: on at 1.5, off at 2.5
    seq.addEvent(dc::MidiMessage::noteOn(1, 64, 0.7f), 1.5);
    seq.addEvent(dc::MidiMessage::noteOff(1, 60), 2.0);
    seq.addEvent(dc::MidiMessage::noteOff(1, 64), 2.5);

    seq.updateMatchedPairs();

    // Event 0 (noteOn 60 @ 1.0) -> Event 2 (noteOff 60 @ 2.0)
    REQUIRE(seq.getEvent(0).matchedPairIndex == 2);
    REQUIRE(seq.getEvent(2).matchedPairIndex == 0);

    // Event 1 (noteOn 64 @ 1.5) -> Event 3 (noteOff 64 @ 2.5)
    REQUIRE(seq.getEvent(1).matchedPairIndex == 3);
    REQUIRE(seq.getEvent(3).matchedPairIndex == 1);
}

TEST_CASE("MidiSequence updateMatchedPairs: velocity-0 noteOn matches as noteOff", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    // noteOn with velocity 0 is treated as noteOff
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.0f), 2.0);

    seq.updateMatchedPairs();

    REQUIRE(seq.getEvent(0).matchedPairIndex == 1);
    REQUIRE(seq.getEvent(1).matchedPairIndex == 0);
}

TEST_CASE("MidiSequence updateMatchedPairs: first unmatched noteOff matches", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOff(1, 60), 2.0);
    seq.addEvent(dc::MidiMessage::noteOff(1, 60), 3.0); // extra noteOff

    seq.updateMatchedPairs();

    // First noteOff matches
    REQUIRE(seq.getEvent(0).matchedPairIndex == 1);
    REQUIRE(seq.getEvent(1).matchedPairIndex == 0);
    // Second noteOff is unmatched
    REQUIRE(seq.getEvent(2).matchedPairIndex == -1);
}

TEST_CASE("MidiSequence updateMatchedPairs: different channels not paired", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOff(2, 60), 2.0); // different channel

    seq.updateMatchedPairs();

    REQUIRE(seq.getEvent(0).matchedPairIndex == -1);
    REQUIRE(seq.getEvent(1).matchedPairIndex == -1);
}

// ─── getEventsInRange ───────────────────────────────────────────

TEST_CASE("MidiSequence getEventsInRange returns correct range", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 64, 0.7f), 2.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 67, 0.6f), 3.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 72, 0.5f), 4.0);

    auto [first, last] = seq.getEventsInRange(1.5, 3.5);

    // Events at 2.0 and 3.0 are in [1.5, 3.5)
    REQUIRE(first == 1);
    REQUIRE(last == 3);
}

TEST_CASE("MidiSequence getEventsInRange inclusive at start", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 64, 0.7f), 2.0);

    auto [first, last] = seq.getEventsInRange(1.0, 2.0);
    // [1.0, 2.0) => only event at 1.0
    REQUIRE(first == 0);
    REQUIRE(last == 1);
}

TEST_CASE("MidiSequence getEventsInRange with no events in range", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 64, 0.7f), 5.0);

    auto [first, last] = seq.getEventsInRange(2.0, 4.0);
    REQUIRE(first == last); // empty range
}

TEST_CASE("MidiSequence getEventsInRange empty sequence", "[midi][sequence]")
{
    dc::MidiSequence seq;
    auto [first, last] = seq.getEventsInRange(0.0, 10.0);
    REQUIRE(first == 0);
    REQUIRE(last == 0);
}

// ─── removeEvent ────────────────────────────────────────────────

TEST_CASE("MidiSequence removeEvent removes correct event", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 64, 0.7f), 2.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 67, 0.6f), 3.0);

    seq.removeEvent(1); // remove middle event

    REQUIRE(seq.getNumEvents() == 2);
    REQUIRE(seq.getEvent(0).message.getNoteNumber() == 60);
    REQUIRE(seq.getEvent(1).message.getNoteNumber() == 67);
}

TEST_CASE("MidiSequence removeEvent with invalid index is no-op", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);

    seq.removeEvent(-1); // no crash
    seq.removeEvent(5);  // no crash
    REQUIRE(seq.getNumEvents() == 1);
}

// ─── sort ───────────────────────────────────────────────────────

TEST_CASE("MidiSequence sort re-sorts after manual manipulation", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 64, 0.7f), 2.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 67, 0.6f), 3.0);

    // Manually modify timestamp to break sort
    seq.getEvent(0).timeInBeats = 5.0;

    seq.sort();

    REQUIRE_THAT(seq.getEvent(0).timeInBeats, WithinAbs(2.0, 1e-9));
    REQUIRE_THAT(seq.getEvent(1).timeInBeats, WithinAbs(3.0, 1e-9));
    REQUIRE_THAT(seq.getEvent(2).timeInBeats, WithinAbs(5.0, 1e-9));
}

// ─── clear ──────────────────────────────────────────────────────

TEST_CASE("MidiSequence clear removes all events", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 64, 0.7f), 2.0);
    REQUIRE(seq.getNumEvents() == 2);

    seq.clear();
    REQUIRE(seq.getNumEvents() == 0);
}

// ─── getEvents returns const reference ──────────────────────────

TEST_CASE("MidiSequence getEvents returns const reference", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOn(1, 64, 0.7f), 2.0);

    const auto& events = seq.getEvents();
    REQUIRE(events.size() == 2);
    REQUIRE_THAT(events[0].timeInBeats, WithinAbs(1.0, 1e-9));
    REQUIRE_THAT(events[1].timeInBeats, WithinAbs(2.0, 1e-9));
}

// ─── Binary serialization round-trip ────────────────────────────

TEST_CASE("MidiSequence binary serialization round-trip", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOff(1, 60), 2.0);
    seq.addEvent(dc::MidiMessage::controllerEvent(2, 7, 100), 1.5);
    seq.addEvent(dc::MidiMessage::noteOn(3, 72, 0.5f), 3.0);
    seq.addEvent(dc::MidiMessage::noteOff(3, 72), 4.0);

    auto binary = seq.toBinary();
    REQUIRE_FALSE(binary.empty());

    auto restored = dc::MidiSequence::fromBinary(binary);

    REQUIRE(restored.getNumEvents() == seq.getNumEvents());

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        REQUIRE_THAT(restored.getEvent(i).timeInBeats,
                     WithinAbs(seq.getEvent(i).timeInBeats, 1e-9));

        auto& origMsg = seq.getEvent(i).message;
        auto& restoredMsg = restored.getEvent(i).message;

        REQUIRE(restoredMsg.getRawDataSize() == origMsg.getRawDataSize());
        REQUIRE(std::memcmp(restoredMsg.getRawData(),
                            origMsg.getRawData(),
                            static_cast<size_t>(origMsg.getRawDataSize())) == 0);
    }
}

TEST_CASE("MidiSequence binary round-trip preserves matched pairs", "[midi][sequence]")
{
    dc::MidiSequence seq;
    seq.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    seq.addEvent(dc::MidiMessage::noteOff(1, 60), 2.0);
    seq.updateMatchedPairs();

    auto binary = seq.toBinary();
    auto restored = dc::MidiSequence::fromBinary(binary);

    // fromBinary calls updateMatchedPairs internally
    REQUIRE(restored.getEvent(0).matchedPairIndex == 1);
    REQUIRE(restored.getEvent(1).matchedPairIndex == 0);
}

TEST_CASE("MidiSequence fromBinary with empty data returns empty sequence", "[midi][sequence]")
{
    std::vector<uint8_t> empty;
    auto seq = dc::MidiSequence::fromBinary(empty);
    REQUIRE(seq.getNumEvents() == 0);
}

TEST_CASE("MidiSequence fromBinary with truncated data returns partial sequence", "[midi][sequence]")
{
    // Create valid binary, then truncate
    dc::MidiSequence original;
    original.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 1.0);
    original.addEvent(dc::MidiMessage::noteOff(1, 60), 2.0);

    auto binary = original.toBinary();

    // Truncate to just the header (version + numEvents = 8 bytes)
    binary.resize(8);
    auto seq = dc::MidiSequence::fromBinary(binary);
    REQUIRE(seq.getNumEvents() == 0);
}

// ─── Legacy format deserialization ──────────────────────────────

TEST_CASE("MidiSequence legacy format deserialization", "[midi][sequence]")
{
    // Build a legacy-format binary blob:
    // Big-endian double (timestamp) + big-endian int32 (msg size) + raw MIDI bytes
    // Repeat for each event.

    std::vector<uint8_t> legacy;

    // Helper to write big-endian double
    auto writeBEDouble = [&legacy](double val)
    {
        uint8_t bytes[8];
        std::memcpy(bytes, &val, 8);
        // Swap to big-endian
        for (int i = 7; i >= 0; --i)
            legacy.push_back(bytes[i]);
    };

    // Helper to write big-endian int32
    auto writeBEInt32 = [&legacy](int32_t val)
    {
        legacy.push_back(static_cast<uint8_t>((val >> 24) & 0xFF));
        legacy.push_back(static_cast<uint8_t>((val >> 16) & 0xFF));
        legacy.push_back(static_cast<uint8_t>((val >> 8) & 0xFF));
        legacy.push_back(static_cast<uint8_t>(val & 0xFF));
    };

    // Event 1: noteOn ch1 note60 vel100 at beat 1.5
    writeBEDouble(1.5);
    writeBEInt32(3); // 3 bytes
    legacy.push_back(0x90); // noteOn ch1
    legacy.push_back(60);
    legacy.push_back(100);

    // Event 2: noteOff ch1 note60 at beat 3.0
    writeBEDouble(3.0);
    writeBEInt32(3);
    legacy.push_back(0x80); // noteOff ch1
    legacy.push_back(60);
    legacy.push_back(0);

    // The first 4 bytes of the legacy data will NOT equal kCurrentVersion (1),
    // so fromBinary should fall into the legacy path.
    auto seq = dc::MidiSequence::fromBinary(legacy);

    REQUIRE(seq.getNumEvents() == 2);
    REQUIRE_THAT(seq.getEvent(0).timeInBeats, WithinAbs(1.5, 1e-9));
    REQUIRE_THAT(seq.getEvent(1).timeInBeats, WithinAbs(3.0, 1e-9));

    REQUIRE(seq.getEvent(0).message.isNoteOn());
    REQUIRE(seq.getEvent(0).message.getNoteNumber() == 60);
    REQUIRE(seq.getEvent(0).message.getRawVelocity() == 100);

    REQUIRE(seq.getEvent(1).message.isNoteOff());
    REQUIRE(seq.getEvent(1).message.getNoteNumber() == 60);
}

// ─── Stress: many events maintain sorted order ──────────────────

TEST_CASE("MidiSequence many events stay sorted", "[midi][sequence]")
{
    dc::MidiSequence seq;

    // Add events in reverse order
    for (int i = 100; i >= 0; --i)
        seq.addEvent(dc::MidiMessage::noteOn(1, i % 128, 0.5f),
                     static_cast<double>(i) * 0.25);

    REQUIRE(seq.getNumEvents() == 101);

    // Verify sorted
    for (int i = 1; i < seq.getNumEvents(); ++i)
        REQUIRE(seq.getEvent(i).timeInBeats >= seq.getEvent(i - 1).timeInBeats);
}

// Unit tests for dc::MidiBuffer
#include <catch2/catch_test_macros.hpp>
#include <dc/midi/MidiBuffer.h>

#include <vector>

// ─── addEvent + iterate ─────────────────────────────────────────

TEST_CASE("MidiBuffer addEvent then iterate retrieves correct event", "[midi][buffer]")
{
    dc::MidiBuffer buf;
    auto msg = dc::MidiMessage::noteOn(1, 60, 0.8f);
    buf.addEvent(msg, 100);

    REQUIRE(buf.getNumEvents() == 1);
    REQUIRE_FALSE(buf.isEmpty());

    auto it = buf.begin();
    REQUIRE(it != buf.end());
    auto event = *it;
    REQUIRE(event.sampleOffset == 100);
    REQUIRE(event.message.isNoteOn());
    REQUIRE(event.message.getNoteNumber() == 60);
}

TEST_CASE("MidiBuffer multiple events in order", "[midi][buffer]")
{
    dc::MidiBuffer buf;
    buf.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 0);
    buf.addEvent(dc::MidiMessage::noteOff(1, 60), 100);
    buf.addEvent(dc::MidiMessage::controllerEvent(1, 7, 127), 200);

    REQUIRE(buf.getNumEvents() == 3);

    std::vector<dc::MidiBuffer::Event> events;
    for (auto it = buf.begin(); it != buf.end(); ++it)
        events.push_back(*it);

    REQUIRE(events.size() == 3);
    REQUIRE(events[0].sampleOffset == 0);
    REQUIRE(events[0].message.isNoteOn());
    REQUIRE(events[1].sampleOffset == 100);
    REQUIRE(events[1].message.isNoteOff());
    REQUIRE(events[2].sampleOffset == 200);
    REQUIRE(events[2].message.isController());
}

// ─── Multiple events at same offset ────────────────────────────

TEST_CASE("MidiBuffer multiple events at same offset preserved in order", "[midi][buffer]")
{
    dc::MidiBuffer buf;
    buf.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 256);
    buf.addEvent(dc::MidiMessage::noteOn(1, 64, 0.7f), 256);
    buf.addEvent(dc::MidiMessage::noteOn(1, 67, 0.6f), 256);

    REQUIRE(buf.getNumEvents() == 3);

    std::vector<dc::MidiBuffer::Event> events;
    for (auto it = buf.begin(); it != buf.end(); ++it)
        events.push_back(*it);

    REQUIRE(events.size() == 3);
    // All at same offset
    REQUIRE(events[0].sampleOffset == 256);
    REQUIRE(events[1].sampleOffset == 256);
    REQUIRE(events[2].sampleOffset == 256);
    // Insertion order preserved
    REQUIRE(events[0].message.getNoteNumber() == 60);
    REQUIRE(events[1].message.getNoteNumber() == 64);
    REQUIRE(events[2].message.getNoteNumber() == 67);
}

// ─── clear / isEmpty ────────────────────────────────────────────

TEST_CASE("MidiBuffer clear empties buffer", "[midi][buffer]")
{
    dc::MidiBuffer buf;
    buf.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 0);
    buf.addEvent(dc::MidiMessage::noteOff(1, 60), 100);
    REQUIRE(buf.getNumEvents() == 2);

    buf.clear();
    REQUIRE(buf.isEmpty());
    REQUIRE(buf.getNumEvents() == 0);
}

TEST_CASE("MidiBuffer newly constructed is empty", "[midi][buffer]")
{
    dc::MidiBuffer buf;
    REQUIRE(buf.isEmpty());
    REQUIRE(buf.getNumEvents() == 0);
}

// ─── Iterator: empty buffer ─────────────────────────────────────

TEST_CASE("MidiBuffer empty buffer begin equals end", "[midi][buffer]")
{
    dc::MidiBuffer buf;
    REQUIRE_FALSE(buf.begin() != buf.end());
}

TEST_CASE("MidiBuffer range-for on empty buffer executes zero times", "[midi][buffer]")
{
    dc::MidiBuffer buf;
    int count = 0;
    for (auto it = buf.begin(); it != buf.end(); ++it)
        ++count;
    REQUIRE(count == 0);
}

// ─── Iterator reconstructs MidiMessage ──────────────────────────

TEST_CASE("MidiBuffer iterator reconstructs MidiMessage from flat bytes", "[midi][buffer]")
{
    dc::MidiBuffer buf;
    auto original = dc::MidiMessage::controllerEvent(5, 74, 100);
    buf.addEvent(original, 512);

    auto event = *buf.begin();
    REQUIRE(event.message.isController());
    REQUIRE(event.message.getChannel() == 5);
    REQUIRE(event.message.getControllerNumber() == 74);
    REQUIRE(event.message.getControllerValue() == 100);
    REQUIRE(event.message.getRawDataSize() == original.getRawDataSize());
}

// ─── Negative sampleOffset ──────────────────────────────────────

TEST_CASE("MidiBuffer negative sampleOffset accepted", "[midi][buffer]")
{
    dc::MidiBuffer buf;
    buf.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), -10);

    REQUIRE(buf.getNumEvents() == 1);
    auto event = *buf.begin();
    REQUIRE(event.sampleOffset == -10);
}

// ─── Large sampleOffset ────────────────────────────────────────

TEST_CASE("MidiBuffer large sampleOffset no overflow", "[midi][buffer]")
{
    dc::MidiBuffer buf;
    int largeOffset = 2000000000; // near int32 max
    buf.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), largeOffset);

    REQUIRE(buf.getNumEvents() == 1);
    auto event = *buf.begin();
    REQUIRE(event.sampleOffset == largeOffset);
}

// ─── SysEx event in buffer ──────────────────────────────────────

TEST_CASE("MidiBuffer SysEx event stored and retrieved correctly", "[midi][buffer]")
{
    uint8_t sysex[] = { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 };
    auto msg = dc::MidiMessage(sysex, 6);

    dc::MidiBuffer buf;
    buf.addEvent(msg, 42);

    REQUIRE(buf.getNumEvents() == 1);
    auto event = *buf.begin();
    REQUIRE(event.sampleOffset == 42);
    REQUIRE(event.message.isSysEx());
    REQUIRE(event.message.getRawDataSize() == 6);
}

// ─── Mixed message types ────────────────────────────────────────

TEST_CASE("MidiBuffer mixed message types interleaved correctly", "[midi][buffer]")
{
    dc::MidiBuffer buf;
    buf.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 0);

    uint8_t sysex[] = { 0xF0, 0x01, 0x02, 0x03, 0x04, 0xF7 };
    buf.addEvent(dc::MidiMessage(sysex, 6), 50);

    buf.addEvent(dc::MidiMessage::controllerEvent(2, 1, 64), 100);

    REQUIRE(buf.getNumEvents() == 3);

    std::vector<dc::MidiBuffer::Event> events;
    for (auto it = buf.begin(); it != buf.end(); ++it)
        events.push_back(*it);

    REQUIRE(events[0].message.isNoteOn());
    REQUIRE(events[0].message.getRawDataSize() == 3);

    REQUIRE(events[1].message.isSysEx());
    REQUIRE(events[1].message.getRawDataSize() == 6);

    REQUIRE(events[2].message.isController());
    REQUIRE(events[2].message.getRawDataSize() == 3);
}

// ─── Constructor with initial capacity ──────────────────────────

TEST_CASE("MidiBuffer with initial capacity works normally", "[midi][buffer]")
{
    dc::MidiBuffer buf(1024);
    REQUIRE(buf.isEmpty());
    buf.addEvent(dc::MidiMessage::noteOn(1, 60, 0.8f), 0);
    REQUIRE(buf.getNumEvents() == 1);
}

// ─── Stress: many events ────────────────────────────────────────

TEST_CASE("MidiBuffer handles many events", "[midi][buffer]")
{
    dc::MidiBuffer buf;
    const int count = 1000;

    for (int i = 0; i < count; ++i)
        buf.addEvent(dc::MidiMessage::noteOn(1, i % 128, 0.5f), i * 10);

    REQUIRE(buf.getNumEvents() == count);

    int idx = 0;
    for (auto it = buf.begin(); it != buf.end(); ++it, ++idx)
    {
        auto event = *it;
        REQUIRE(event.sampleOffset == idx * 10);
        REQUIRE(event.message.getNoteNumber() == idx % 128);
    }
    REQUIRE(idx == count);
}

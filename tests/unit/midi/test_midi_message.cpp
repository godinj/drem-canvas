// Unit tests for dc::MidiMessage
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <dc/midi/MidiMessage.h>

#include <cstring>

using Catch::Matchers::WithinAbs;

// ─── Factory: noteOn ────────────────────────────────────────────

TEST_CASE("MidiMessage noteOn produces correct status byte", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(1, 60, 0.8f);
    REQUIRE(msg.isNoteOn());
    REQUIRE_FALSE(msg.isNoteOff());
    REQUIRE(msg.isNoteOnOrOff());

    // Status byte: 0x90 | (channel-1) => 0x90
    auto raw = msg.getRawData();
    REQUIRE((raw[0] & 0xF0) == 0x90);
    REQUIRE((raw[0] & 0x0F) == 0); // channel 1 => nibble 0
}

TEST_CASE("MidiMessage noteOn channel 10", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(10, 36, 1.0f);
    REQUIRE(msg.getChannel() == 10);
    REQUIRE(msg.getNoteNumber() == 36);
    REQUIRE(msg.getRawVelocity() == 127);
}

TEST_CASE("MidiMessage noteOn velocity scaling", "[midi][message]")
{
    // velocity 0.5 => raw ~64
    auto msg = dc::MidiMessage::noteOn(1, 60, 0.5f);
    int raw = msg.getRawVelocity();
    REQUIRE(raw >= 63);
    REQUIRE(raw <= 64);

    // Round-trip: getVelocity() should be close to 0.5
    REQUIRE_THAT(msg.getVelocity(), WithinAbs(0.5, 0.01));
}

// ─── Factory: noteOff ───────────────────────────────────────────

TEST_CASE("MidiMessage noteOff produces correct status byte", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOff(1, 60, 0.0f);
    REQUIRE(msg.isNoteOff());
    REQUIRE_FALSE(msg.isNoteOn());
    REQUIRE(msg.isNoteOnOrOff());

    auto raw = msg.getRawData();
    REQUIRE((raw[0] & 0xF0) == 0x80);
}

TEST_CASE("MidiMessage noteOff default velocity", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOff(3, 72);
    REQUIRE(msg.isNoteOff());
    REQUIRE(msg.getChannel() == 3);
    REQUIRE(msg.getNoteNumber() == 72);
    REQUIRE(msg.getRawVelocity() == 0);
}

// ─── velocity-0 noteOn is noteOff ───────────────────────────────

TEST_CASE("MidiMessage noteOn with velocity 0 is treated as noteOff", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(1, 60, 0.0f);

    // Status byte is 0x90, but velocity is 0 => isNoteOff should be true
    REQUIRE(msg.isNoteOff());
    REQUIRE_FALSE(msg.isNoteOn()); // isNoteOn checks vel != 0
    REQUIRE(msg.isNoteOnOrOff());
}

// ─── Factory: controllerEvent ───────────────────────────────────

TEST_CASE("MidiMessage controllerEvent produces correct bytes", "[midi][message]")
{
    auto msg = dc::MidiMessage::controllerEvent(2, 7, 100); // ch2 CC7=100
    REQUIRE(msg.isController());
    REQUIRE(msg.getChannel() == 2);
    REQUIRE(msg.getControllerNumber() == 7);
    REQUIRE(msg.getControllerValue() == 100);

    auto raw = msg.getRawData();
    REQUIRE((raw[0] & 0xF0) == 0xB0);
    REQUIRE((raw[0] & 0x0F) == 1); // channel 2 => nibble 1
}

// ─── Factory: programChange ─────────────────────────────────────

TEST_CASE("MidiMessage programChange produces correct bytes", "[midi][message]")
{
    auto msg = dc::MidiMessage::programChange(5, 42);
    REQUIRE(msg.isProgramChange());
    REQUIRE(msg.getChannel() == 5);
    REQUIRE(msg.getProgramChangeNumber() == 42);

    auto raw = msg.getRawData();
    REQUIRE((raw[0] & 0xF0) == 0xC0);
}

// ─── Factory: pitchWheel ────────────────────────────────────────

TEST_CASE("MidiMessage pitchWheel produces 14-bit value", "[midi][message]")
{
    // Center value = 8192
    auto msg = dc::MidiMessage::pitchWheel(1, 8192);
    REQUIRE(msg.isPitchWheel());
    REQUIRE(msg.getChannel() == 1);
    REQUIRE(msg.getPitchWheelValue() == 8192);

    auto raw = msg.getRawData();
    REQUIRE((raw[0] & 0xF0) == 0xE0);

    // Verify 14-bit encoding: LSB = value & 0x7F, MSB = (value >> 7) & 0x7F
    REQUIRE(raw[1] == (8192 & 0x7F));
    REQUIRE(raw[2] == ((8192 >> 7) & 0x7F));
}

TEST_CASE("MidiMessage pitchWheel min and max", "[midi][message]")
{
    auto min = dc::MidiMessage::pitchWheel(1, 0);
    REQUIRE(min.getPitchWheelValue() == 0);

    auto max = dc::MidiMessage::pitchWheel(1, 16383);
    REQUIRE(max.getPitchWheelValue() == 16383);
}

// ─── Factory: aftertouch ────────────────────────────────────────

TEST_CASE("MidiMessage aftertouch (poly) produces correct bytes", "[midi][message]")
{
    auto msg = dc::MidiMessage::aftertouch(3, 60, 80);
    REQUIRE(msg.isAftertouch());
    REQUIRE(msg.getChannel() == 3);
    REQUIRE(msg.getNoteNumber() == 60);

    auto raw = msg.getRawData();
    REQUIRE((raw[0] & 0xF0) == 0xA0);
    REQUIRE(raw[2] == 80);
}

// ─── Factory: channelPressure ───────────────────────────────────

TEST_CASE("MidiMessage channelPressure produces correct bytes", "[midi][message]")
{
    auto msg = dc::MidiMessage::channelPressure(1, 100);
    REQUIRE(msg.isChannelPressure());
    REQUIRE(msg.getChannel() == 1);

    auto raw = msg.getRawData();
    REQUIRE((raw[0] & 0xF0) == 0xD0);
    REQUIRE(raw[1] == 100);
}

// ─── Factory: allNotesOff / allSoundOff ─────────────────────────

TEST_CASE("MidiMessage allNotesOff is CC 123 value 0", "[midi][message]")
{
    auto msg = dc::MidiMessage::allNotesOff(4);
    REQUIRE(msg.isController());
    REQUIRE(msg.getChannel() == 4);
    REQUIRE(msg.getControllerNumber() == 123);
    REQUIRE(msg.getControllerValue() == 0);
}

TEST_CASE("MidiMessage allSoundOff is CC 120 value 0", "[midi][message]")
{
    auto msg = dc::MidiMessage::allSoundOff(4);
    REQUIRE(msg.isController());
    REQUIRE(msg.getChannel() == 4);
    REQUIRE(msg.getControllerNumber() == 120);
    REQUIRE(msg.getControllerValue() == 0);
}

// ─── Channel clamping ───────────────────────────────────────────

TEST_CASE("MidiMessage channel clamped: 0 becomes 1", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(0, 60, 0.8f);
    // channelByte(0) = max(0, min(15, 0-1)) = max(0, min(15, -1)) = 0
    // so getChannel() = 0 + 1 = 1
    REQUIRE(msg.getChannel() == 1);
}

TEST_CASE("MidiMessage channel clamped: 17 becomes 16", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(17, 60, 0.8f);
    // channelByte(17) = max(0, min(15, 16)) = 15
    // getChannel() = 15 + 1 = 16
    REQUIRE(msg.getChannel() == 16);
}

TEST_CASE("MidiMessage channel 16 is valid maximum", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(16, 60, 0.8f);
    REQUIRE(msg.getChannel() == 16);
}

// ─── Note clamping ──────────────────────────────────────────────

TEST_CASE("MidiMessage note number clamped: negative becomes 0", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(1, -5, 0.8f);
    REQUIRE(msg.getNoteNumber() == 0);
}

TEST_CASE("MidiMessage note number clamped: 128+ becomes 127", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(1, 200, 0.8f);
    REQUIRE(msg.getNoteNumber() == 127);
}

// ─── Velocity clamping ──────────────────────────────────────────

TEST_CASE("MidiMessage velocity clamped: negative becomes 0", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(1, 60, -0.5f);
    REQUIRE(msg.getRawVelocity() == 0);
}

TEST_CASE("MidiMessage velocity clamped: >1.0 becomes 127", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(1, 60, 2.0f);
    REQUIRE(msg.getRawVelocity() == 127);
}

// ─── getVelocity vs getRawVelocity ─────────────────────────────

TEST_CASE("MidiMessage getVelocity returns float [0,1]", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(1, 60, 1.0f);
    REQUIRE_THAT(msg.getVelocity(), WithinAbs(1.0, 0.001));
    REQUIRE(msg.getRawVelocity() == 127);

    auto msg2 = dc::MidiMessage::noteOn(1, 60, 0.0f);
    REQUIRE_THAT(msg2.getVelocity(), WithinAbs(0.0, 0.001));
    REQUIRE(msg2.getRawVelocity() == 0);
}

// ─── Raw data round-trip ────────────────────────────────────────

TEST_CASE("MidiMessage raw data round-trip for channel messages", "[midi][message]")
{
    auto original = dc::MidiMessage::noteOn(5, 72, 0.6f);
    auto reconstructed = dc::MidiMessage(original.getRawData(), original.getRawDataSize());

    REQUIRE(reconstructed.isNoteOn());
    REQUIRE(reconstructed.getChannel() == original.getChannel());
    REQUIRE(reconstructed.getNoteNumber() == original.getNoteNumber());
    REQUIRE(reconstructed.getRawVelocity() == original.getRawVelocity());
    REQUIRE(reconstructed.getRawDataSize() == original.getRawDataSize());
}

TEST_CASE("MidiMessage raw data round-trip for controller", "[midi][message]")
{
    auto original = dc::MidiMessage::controllerEvent(3, 11, 90);
    auto reconstructed = dc::MidiMessage(original.getRawData(), original.getRawDataSize());

    REQUIRE(reconstructed.isController());
    REQUIRE(reconstructed.getControllerNumber() == 11);
    REQUIRE(reconstructed.getControllerValue() == 90);
}

// ─── SysEx ──────────────────────────────────────────────────────

TEST_CASE("MidiMessage SysEx stored on heap", "[midi][message]")
{
    // Standard SysEx: F0 <data...> F7
    uint8_t sysex[] = { 0xF0, 0x7E, 0x7F, 0x09, 0x01, 0xF7 };
    auto msg = dc::MidiMessage(sysex, 6);

    REQUIRE(msg.isSysEx());
    REQUIRE(msg.getRawDataSize() == 6);

    // Verify round-trip
    auto* raw = msg.getRawData();
    REQUIRE(raw[0] == 0xF0);
    REQUIRE(raw[5] == 0xF7);
    REQUIRE(std::memcmp(raw, sysex, 6) == 0);
}

TEST_CASE("MidiMessage large SysEx round-trip", "[midi][message]")
{
    // Build a larger SysEx message (> 3 bytes triggers heap)
    std::vector<uint8_t> data(128);
    data[0] = 0xF0;
    for (int i = 1; i < 127; ++i)
        data[static_cast<size_t>(i)] = static_cast<uint8_t>(i & 0x7F);
    data[127] = 0xF7;

    auto msg = dc::MidiMessage(data.data(), static_cast<int>(data.size()));
    REQUIRE(msg.isSysEx());
    REQUIRE(msg.getRawDataSize() == 128);

    auto reconstructed = dc::MidiMessage(msg.getRawData(), msg.getRawDataSize());
    REQUIRE(reconstructed.isSysEx());
    REQUIRE(reconstructed.getRawDataSize() == 128);
    REQUIRE(std::memcmp(reconstructed.getRawData(), data.data(), 128) == 0);
}

// ─── Empty message ──────────────────────────────────────────────

TEST_CASE("MidiMessage default constructed is empty", "[midi][message]")
{
    dc::MidiMessage msg;
    REQUIRE(msg.getRawDataSize() == 0);
    REQUIRE_FALSE(msg.isNoteOn());
    REQUIRE_FALSE(msg.isNoteOff());
    REQUIRE_FALSE(msg.isController());
    REQUIRE_FALSE(msg.isProgramChange());
    REQUIRE_FALSE(msg.isPitchWheel());
    REQUIRE_FALSE(msg.isChannelPressure());
    REQUIRE_FALSE(msg.isAftertouch());
    REQUIRE_FALSE(msg.isSysEx());
}

// ─── Mutations ──────────────────────────────────────────────────

TEST_CASE("MidiMessage setChannel mutates channel", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(1, 60, 0.8f);
    REQUIRE(msg.getChannel() == 1);

    msg.setChannel(10);
    REQUIRE(msg.getChannel() == 10);
    REQUIRE(msg.isNoteOn()); // type unchanged
    REQUIRE(msg.getNoteNumber() == 60); // note unchanged
}

TEST_CASE("MidiMessage setNoteNumber mutates note", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(1, 60, 0.8f);
    msg.setNoteNumber(72);
    REQUIRE(msg.getNoteNumber() == 72);
    REQUIRE(msg.getChannel() == 1); // channel unchanged
}

TEST_CASE("MidiMessage setNoteNumber clamps", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(1, 60, 0.8f);
    msg.setNoteNumber(200);
    REQUIRE(msg.getNoteNumber() == 127);

    msg.setNoteNumber(-10);
    REQUIRE(msg.getNoteNumber() == 0);
}

TEST_CASE("MidiMessage setVelocity mutates velocity", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(1, 60, 0.5f);
    msg.setVelocity(1.0f);
    REQUIRE(msg.getRawVelocity() == 127);
    REQUIRE_THAT(msg.getVelocity(), WithinAbs(1.0, 0.001));
}

TEST_CASE("MidiMessage setVelocity clamps", "[midi][message]")
{
    auto msg = dc::MidiMessage::noteOn(1, 60, 0.5f);
    msg.setVelocity(-1.0f);
    REQUIRE(msg.getRawVelocity() == 0);

    msg.setVelocity(5.0f);
    REQUIRE(msg.getRawVelocity() == 127);
}

// ─── Copy semantics ─────────────────────────────────────────────

TEST_CASE("MidiMessage copy produces identical message", "[midi][message]")
{
    auto original = dc::MidiMessage::noteOn(3, 48, 0.7f);
    dc::MidiMessage copy = original;

    REQUIRE(copy.isNoteOn());
    REQUIRE(copy.getChannel() == original.getChannel());
    REQUIRE(copy.getNoteNumber() == original.getNoteNumber());
    REQUIRE(copy.getRawVelocity() == original.getRawVelocity());
    REQUIRE(copy.getRawDataSize() == original.getRawDataSize());
}

TEST_CASE("MidiMessage copy of SysEx is independent", "[midi][message]")
{
    uint8_t sysex[] = { 0xF0, 0x01, 0x02, 0x03, 0xF7 };
    auto original = dc::MidiMessage(sysex, 5);
    dc::MidiMessage copy = original;

    REQUIRE(copy.isSysEx());
    REQUIRE(copy.getRawDataSize() == 5);
    REQUIRE(std::memcmp(copy.getRawData(), sysex, 5) == 0);
}

// ─── Query: type predicates are mutually exclusive ──────────────

TEST_CASE("MidiMessage type queries are correct for each factory", "[midi][message]")
{
    auto noteOn = dc::MidiMessage::noteOn(1, 60, 0.8f);
    REQUIRE(noteOn.isNoteOn());
    REQUIRE_FALSE(noteOn.isController());
    REQUIRE_FALSE(noteOn.isProgramChange());
    REQUIRE_FALSE(noteOn.isPitchWheel());
    REQUIRE_FALSE(noteOn.isChannelPressure());
    REQUIRE_FALSE(noteOn.isAftertouch());

    auto cc = dc::MidiMessage::controllerEvent(1, 1, 64);
    REQUIRE(cc.isController());
    REQUIRE_FALSE(cc.isNoteOn());
    REQUIRE_FALSE(cc.isNoteOff());

    auto pc = dc::MidiMessage::programChange(1, 0);
    REQUIRE(pc.isProgramChange());
    REQUIRE_FALSE(pc.isController());

    auto pw = dc::MidiMessage::pitchWheel(1, 8192);
    REQUIRE(pw.isPitchWheel());
    REQUIRE_FALSE(pw.isController());

    auto cp = dc::MidiMessage::channelPressure(1, 64);
    REQUIRE(cp.isChannelPressure());
    REQUIRE_FALSE(cp.isAftertouch());

    auto at = dc::MidiMessage::aftertouch(1, 60, 64);
    REQUIRE(at.isAftertouch());
    REQUIRE_FALSE(at.isChannelPressure());
}

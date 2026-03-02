#pragma once

#if defined(DC_LIBRARY_BUILD) && defined(JUCE_AUDIO_BASICS_H_INCLUDED)
  #error "dc::midi must not depend on JUCE audio basics — Phase 2 boundary violation. " \
         "See docs/sans-juce/06-midi-subsystem.md"
#endif

#include <cstdint>
#include <vector>

namespace dc {

/// Compact MIDI message with 3-byte inline storage for channel messages
/// and heap-allocated storage for SysEx.
class MidiMessage
{
public:
    MidiMessage() = default;
    MidiMessage(uint8_t status, uint8_t data1, uint8_t data2 = 0);
    MidiMessage(const uint8_t* data, int size);

    // --- Factory methods ---
    static MidiMessage noteOn(int channel, int noteNumber, float velocity);
    static MidiMessage noteOff(int channel, int noteNumber, float velocity = 0.0f);
    static MidiMessage controllerEvent(int channel, int controller, int value);
    static MidiMessage programChange(int channel, int program);
    static MidiMessage pitchWheel(int channel, int value);
    static MidiMessage channelPressure(int channel, int pressure);
    static MidiMessage aftertouch(int channel, int noteNumber, int pressure);
    static MidiMessage allNotesOff(int channel);
    static MidiMessage allSoundOff(int channel);

    // --- Queries ---
    bool isNoteOn() const;
    bool isNoteOff() const;
    bool isNoteOnOrOff() const;
    bool isController() const;
    bool isProgramChange() const;
    bool isPitchWheel() const;
    bool isChannelPressure() const;
    bool isAftertouch() const;
    bool isSysEx() const;

    int getChannel() const;             // 1-16
    int getNoteNumber() const;          // 0-127
    float getVelocity() const;          // 0.0-1.0
    int getRawVelocity() const;         // 0-127
    int getControllerNumber() const;
    int getControllerValue() const;
    int getProgramChangeNumber() const;
    int getPitchWheelValue() const;

    // --- Raw access ---
    const uint8_t* getRawData() const;
    int getRawDataSize() const;

    // --- Mutation ---
    void setChannel(int channel);
    void setNoteNumber(int noteNumber);
    void setVelocity(float velocity);

private:
    uint8_t data_[3] = {0, 0, 0};
    int size_ = 0;
    std::vector<uint8_t> sysex_;
};

} // namespace dc

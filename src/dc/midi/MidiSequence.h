#pragma once

#include "dc/midi/MidiMessage.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace dc {

struct TimedMidiEvent
{
    double timeInBeats = 0.0;
    MidiMessage message;
    int matchedPairIndex = -1;
};

/// Sorted vector of timestamped MIDI events for the model layer.
/// Sorted vector of timestamped MIDI events.
class MidiSequence
{
public:
    MidiSequence() = default;

    /// Add an event (maintains sorted order by timeInBeats)
    void addEvent(const MidiMessage& msg, double timeInBeats);

    /// Remove an event by index
    void removeEvent(int index);

    /// Get event by index
    const TimedMidiEvent& getEvent(int index) const;
    TimedMidiEvent& getEvent(int index);

    /// Number of events
    int getNumEvents() const;

    /// Clear all events
    void clear();

    /// Sort events by timestamp (called after bulk modifications)
    void sort();

    /// Match noteOn/noteOff pairs (sets matchedPairIndex)
    void updateMatchedPairs();

    /// Get events in a time range (for playback).
    /// Returns indices [first, last) where startBeats <= time < endBeats.
    std::pair<int, int> getEventsInRange(double startBeats,
                                          double endBeats) const;

    // --- Serialization ---

    /// Serialize to binary (for base64 encoding in YAML)
    std::vector<uint8_t> toBinary() const;

    /// Deserialize from binary (handles both new and legacy format)
    static MidiSequence fromBinary(const std::vector<uint8_t>& data);

    /// Direct access
    const std::vector<TimedMidiEvent>& getEvents() const;

private:
    std::vector<TimedMidiEvent> events_;

    static constexpr uint32_t kCurrentVersion = 1;
};

} // namespace dc

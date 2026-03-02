#pragma once

#include "dc/midi/MidiMessage.h"

#include <cstdint>
#include <cstring>
#include <vector>

namespace dc {

/// Flat byte buffer for passing timestamped MIDI events through the audio graph.
/// Storage layout per event: [int32 sampleOffset][int16 size][uint8 data...]
class MidiBuffer
{
public:
    MidiBuffer() = default;
    explicit MidiBuffer(int initialCapacity);

    /// Add an event at the given sample offset
    void addEvent(const MidiMessage& msg, int sampleOffset);

    /// Clear all events
    void clear();

    /// Number of events
    int getNumEvents() const;

    /// Whether the buffer is empty
    bool isEmpty() const;

    // --- Iteration ---

    struct Event
    {
        int sampleOffset;
        MidiMessage message;
    };

    class Iterator
    {
    public:
        Iterator(const uint8_t* ptr, const uint8_t* end)
            : ptr_(ptr), end_(end) {}

        Event operator*() const;
        Iterator& operator++();
        bool operator!=(const Iterator& other) const;

    private:
        const uint8_t* ptr_;
        const uint8_t* end_;
    };

    Iterator begin() const;
    Iterator end() const;

private:
    std::vector<uint8_t> data_;
    int numEvents_ = 0;
};

} // namespace dc

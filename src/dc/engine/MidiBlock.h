#pragma once

#include "dc/midi/MidiBuffer.h"

namespace dc {

/// Non-owning view over a dc::MidiBuffer.
/// Thin wrapper that delegates to the underlying buffer.
class MidiBlock
{
public:
    MidiBlock();
    explicit MidiBlock (MidiBuffer& buffer);

    MidiBuffer::Iterator begin() const;
    MidiBuffer::Iterator end() const;

    void addEvent (const MidiMessage& msg, int sampleOffset);
    void clear();
    int getNumEvents() const;
    bool isEmpty() const;

    /// Get the underlying buffer (for passing to legacy code)
    MidiBuffer* getBuffer() { return buffer_; }

private:
    MidiBuffer* buffer_ = nullptr;
    MidiBuffer ownedBuffer_;  // used when constructed with default ctor
};

} // namespace dc

#pragma once

#include "dc/midi/MidiBuffer.h"

namespace dc {

/// Non-owning view over a dc::MidiBuffer.
/// Thin wrapper that delegates to the underlying buffer.
///
/// When default-constructed, the block owns an internal buffer and
/// buffer_ points to it.  Copy/move operations must fix up buffer_
/// so it never dangles to the source object's ownedBuffer_.
class MidiBlock
{
public:
    MidiBlock();
    explicit MidiBlock (MidiBuffer& buffer);

    MidiBlock (const MidiBlock& other);
    MidiBlock& operator= (const MidiBlock& other);

    MidiBlock (MidiBlock&& other) noexcept;
    MidiBlock& operator= (MidiBlock&& other) noexcept;

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

    bool ownsBuffer() const { return buffer_ == &ownedBuffer_; }
};

} // namespace dc

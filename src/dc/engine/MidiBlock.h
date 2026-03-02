#pragma once
#include "dc/midi/MidiBuffer.h"
#include "dc/midi/MidiMessage.h"

namespace dc {

class MidiBlock
{
public:
    MidiBlock() = default;
    explicit MidiBlock (MidiBuffer& buffer) : buffer_ (&buffer) {}

    MidiBuffer::Iterator begin() const
    {
        if (buffer_)
            return buffer_->begin();
        return MidiBuffer::Iterator (nullptr, nullptr);
    }

    MidiBuffer::Iterator end() const
    {
        if (buffer_)
            return buffer_->end();
        return MidiBuffer::Iterator (nullptr, nullptr);
    }

    void addEvent (const MidiMessage& msg, int sampleOffset)
    {
        if (buffer_)
            buffer_->addEvent (msg, sampleOffset);
    }

    void clear()
    {
        if (buffer_)
            buffer_->clear();
    }

    int getNumEvents() const
    {
        return buffer_ ? buffer_->getNumEvents() : 0;
    }

    bool isEmpty() const
    {
        return ! buffer_ || buffer_->isEmpty();
    }

private:
    MidiBuffer* buffer_ = nullptr;
};

} // namespace dc

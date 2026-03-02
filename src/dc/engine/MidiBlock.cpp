#include "dc/engine/MidiBlock.h"

namespace dc {

MidiBlock::MidiBlock()
    : buffer_ (&ownedBuffer_)
{
}

MidiBlock::MidiBlock (MidiBuffer& buffer)
    : buffer_ (&buffer)
{
}

MidiBlock::MidiBlock (const MidiBlock& other)
    : ownedBuffer_ (other.ownedBuffer_)
{
    // If the source owned its buffer, point at our own copy.
    // If it was a non-owning view, copy the external pointer.
    buffer_ = other.ownsBuffer() ? &ownedBuffer_ : other.buffer_;
}

MidiBlock& MidiBlock::operator= (const MidiBlock& other)
{
    if (this != &other)
    {
        ownedBuffer_ = other.ownedBuffer_;
        buffer_ = other.ownsBuffer() ? &ownedBuffer_ : other.buffer_;
    }
    return *this;
}

MidiBlock::MidiBlock (MidiBlock&& other) noexcept
    : ownedBuffer_ (std::move (other.ownedBuffer_))
{
    buffer_ = other.ownsBuffer() ? &ownedBuffer_ : other.buffer_;
}

MidiBlock& MidiBlock::operator= (MidiBlock&& other) noexcept
{
    if (this != &other)
    {
        ownedBuffer_ = std::move (other.ownedBuffer_);
        buffer_ = other.ownsBuffer() ? &ownedBuffer_ : other.buffer_;
    }
    return *this;
}

MidiBuffer::Iterator MidiBlock::begin() const
{
    return buffer_->begin();
}

MidiBuffer::Iterator MidiBlock::end() const
{
    return buffer_->end();
}

void MidiBlock::addEvent (const MidiMessage& msg, int sampleOffset)
{
    buffer_->addEvent (msg, sampleOffset);
}

void MidiBlock::clear()
{
    buffer_->clear();
}

int MidiBlock::getNumEvents() const
{
    return buffer_->getNumEvents();
}

bool MidiBlock::isEmpty() const
{
    return buffer_->isEmpty();
}

} // namespace dc

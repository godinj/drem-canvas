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

#include "dc/midi/MidiBuffer.h"

namespace dc {

MidiBuffer::MidiBuffer(int initialCapacity)
{
    data_.reserve(static_cast<size_t>(initialCapacity));
}

void MidiBuffer::addEvent(const MidiMessage& msg, int sampleOffset)
{
    auto msgSize = static_cast<int16_t>(msg.getRawDataSize());
    auto oldSize = data_.size();
    data_.resize(oldSize + sizeof(int32_t) + sizeof(int16_t) + static_cast<size_t>(msgSize));

    auto* dst = data_.data() + oldSize;
    auto offset32 = static_cast<int32_t>(sampleOffset);
    std::memcpy(dst, &offset32, sizeof(int32_t));
    dst += sizeof(int32_t);
    std::memcpy(dst, &msgSize, sizeof(int16_t));
    dst += sizeof(int16_t);
    std::memcpy(dst, msg.getRawData(), static_cast<size_t>(msgSize));

    ++numEvents_;
}

void MidiBuffer::clear()
{
    data_.clear();
    numEvents_ = 0;
}

int MidiBuffer::getNumEvents() const
{
    return numEvents_;
}

bool MidiBuffer::isEmpty() const
{
    return numEvents_ == 0;
}

// --- Iterator ---

MidiBuffer::Event MidiBuffer::Iterator::operator*() const
{
    Event e;
    int32_t offset;
    std::memcpy(&offset, ptr_, sizeof(int32_t));
    e.sampleOffset = offset;

    int16_t msgSize;
    std::memcpy(&msgSize, ptr_ + sizeof(int32_t), sizeof(int16_t));

    const auto* msgData = ptr_ + sizeof(int32_t) + sizeof(int16_t);
    e.message = MidiMessage(msgData, static_cast<int>(msgSize));
    return e;
}

MidiBuffer::Iterator& MidiBuffer::Iterator::operator++()
{
    int16_t msgSize;
    std::memcpy(&msgSize, ptr_ + sizeof(int32_t), sizeof(int16_t));
    ptr_ += sizeof(int32_t) + sizeof(int16_t) + static_cast<size_t>(msgSize);
    return *this;
}

bool MidiBuffer::Iterator::operator!=(const Iterator& other) const
{
    return ptr_ != other.ptr_;
}

MidiBuffer::Iterator MidiBuffer::begin() const
{
    return Iterator(data_.data(), data_.data() + data_.size());
}

MidiBuffer::Iterator MidiBuffer::end() const
{
    auto* e = data_.data() + data_.size();
    return Iterator(e, e);
}

} // namespace dc

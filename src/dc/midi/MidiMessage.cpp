#include "dc/midi/MidiMessage.h"

#include <algorithm>
#include <cstring>

namespace dc {

MidiMessage::MidiMessage(uint8_t status, uint8_t data1, uint8_t data2)
    : size_(3)
{
    data_[0] = status;
    data_[1] = data1;
    data_[2] = data2;
}

MidiMessage::MidiMessage(const uint8_t* data, int size)
{
    if (size <= 3)
    {
        size_ = size;
        std::memcpy(data_, data, static_cast<size_t>(size));
    }
    else
    {
        // SysEx or other long message — heap allocate
        size_ = size;
        sysex_.assign(data, data + size);
    }
}

// --- Factory methods ---

static uint8_t clampByte(int v)
{
    return static_cast<uint8_t>(std::max(0, std::min(127, v)));
}

static uint8_t channelByte(int channel)
{
    return static_cast<uint8_t>(std::max(0, std::min(15, channel - 1)));
}

MidiMessage MidiMessage::noteOn(int channel, int noteNumber, float velocity)
{
    int vel = static_cast<int>(velocity * 127.0f + 0.5f);
    return MidiMessage(
        static_cast<uint8_t>(0x90 | channelByte(channel)),
        clampByte(noteNumber),
        clampByte(vel));
}

MidiMessage MidiMessage::noteOff(int channel, int noteNumber, float velocity)
{
    int vel = static_cast<int>(velocity * 127.0f + 0.5f);
    return MidiMessage(
        static_cast<uint8_t>(0x80 | channelByte(channel)),
        clampByte(noteNumber),
        clampByte(vel));
}

MidiMessage MidiMessage::controllerEvent(int channel, int controller, int value)
{
    return MidiMessage(
        static_cast<uint8_t>(0xB0 | channelByte(channel)),
        clampByte(controller),
        clampByte(value));
}

MidiMessage MidiMessage::programChange(int channel, int program)
{
    return MidiMessage(
        static_cast<uint8_t>(0xC0 | channelByte(channel)),
        clampByte(program),
        0);
}

MidiMessage MidiMessage::pitchWheel(int channel, int value)
{
    int clamped = std::max(0, std::min(16383, value));
    return MidiMessage(
        static_cast<uint8_t>(0xE0 | channelByte(channel)),
        static_cast<uint8_t>(clamped & 0x7F),
        static_cast<uint8_t>((clamped >> 7) & 0x7F));
}

MidiMessage MidiMessage::channelPressure(int channel, int pressure)
{
    return MidiMessage(
        static_cast<uint8_t>(0xD0 | channelByte(channel)),
        clampByte(pressure),
        0);
}

MidiMessage MidiMessage::aftertouch(int channel, int noteNumber, int pressure)
{
    return MidiMessage(
        static_cast<uint8_t>(0xA0 | channelByte(channel)),
        clampByte(noteNumber),
        clampByte(pressure));
}

MidiMessage MidiMessage::allNotesOff(int channel)
{
    // CC 123, value 0
    return controllerEvent(channel, 123, 0);
}

MidiMessage MidiMessage::allSoundOff(int channel)
{
    // CC 120, value 0
    return controllerEvent(channel, 120, 0);
}

// --- Queries ---

bool MidiMessage::isNoteOn() const
{
    return size_ >= 3
        && (data_[0] & 0xF0) == 0x90
        && data_[2] != 0;
}

bool MidiMessage::isNoteOff() const
{
    if (size_ < 3) return false;
    uint8_t status = data_[0] & 0xF0;
    return status == 0x80 || (status == 0x90 && data_[2] == 0);
}

bool MidiMessage::isNoteOnOrOff() const
{
    if (size_ < 3) return false;
    uint8_t status = data_[0] & 0xF0;
    return status == 0x80 || status == 0x90;
}

bool MidiMessage::isController() const
{
    return size_ >= 3 && (data_[0] & 0xF0) == 0xB0;
}

bool MidiMessage::isProgramChange() const
{
    return size_ >= 2 && (data_[0] & 0xF0) == 0xC0;
}

bool MidiMessage::isPitchWheel() const
{
    return size_ >= 3 && (data_[0] & 0xF0) == 0xE0;
}

bool MidiMessage::isChannelPressure() const
{
    return size_ >= 2 && (data_[0] & 0xF0) == 0xD0;
}

bool MidiMessage::isAftertouch() const
{
    return size_ >= 3 && (data_[0] & 0xF0) == 0xA0;
}

bool MidiMessage::isSysEx() const
{
    auto* d = getRawData();
    return size_ > 0 && d[0] == 0xF0;
}

int MidiMessage::getChannel() const
{
    return (data_[0] & 0x0F) + 1;
}

int MidiMessage::getNoteNumber() const
{
    return data_[1];
}

float MidiMessage::getVelocity() const
{
    return static_cast<float>(data_[2]) / 127.0f;
}

int MidiMessage::getRawVelocity() const
{
    return data_[2];
}

int MidiMessage::getControllerNumber() const
{
    return data_[1];
}

int MidiMessage::getControllerValue() const
{
    return data_[2];
}

int MidiMessage::getProgramChangeNumber() const
{
    return data_[1];
}

int MidiMessage::getPitchWheelValue() const
{
    return data_[1] | (data_[2] << 7);
}

// --- Raw access ---

const uint8_t* MidiMessage::getRawData() const
{
    if (!sysex_.empty())
        return sysex_.data();
    return data_;
}

int MidiMessage::getRawDataSize() const
{
    return size_;
}

// --- Mutation ---

void MidiMessage::setChannel(int channel)
{
    data_[0] = (data_[0] & 0xF0) | channelByte(channel);
    if (!sysex_.empty() && !sysex_.empty())
        sysex_[0] = data_[0];
}

void MidiMessage::setNoteNumber(int noteNumber)
{
    data_[1] = clampByte(noteNumber);
    if (!sysex_.empty() && sysex_.size() > 1)
        sysex_[1] = data_[1];
}

void MidiMessage::setVelocity(float velocity)
{
    int vel = static_cast<int>(velocity * 127.0f + 0.5f);
    data_[2] = clampByte(vel);
    if (!sysex_.empty() && sysex_.size() > 2)
        sysex_[2] = data_[2];
}

} // namespace dc

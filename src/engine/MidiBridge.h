#pragma once

// Bridge: convert dc::MidiMessage/MidiBuffer to juce equivalents at processBlock boundaries.
// This file is deleted when processors migrate to AudioNode (Phase 3).

#include <JuceHeader.h>
#include "dc/midi/MidiMessage.h"
#include "dc/midi/MidiBuffer.h"

namespace dc { namespace bridge {

inline juce::MidiMessage toJuce (const dc::MidiMessage& msg)
{
    return juce::MidiMessage (msg.getRawData(), msg.getRawDataSize());
}

inline dc::MidiMessage fromJuce (const juce::MidiMessage& msg)
{
    return dc::MidiMessage (msg.getRawData(), msg.getRawDataSize());
}

inline dc::MidiBuffer fromJuce (const juce::MidiBuffer& juceBuf)
{
    dc::MidiBuffer buf;
    for (const auto metadata : juceBuf)
        buf.addEvent (dc::MidiMessage (metadata.data, metadata.numBytes),
                      metadata.samplePosition);
    return buf;
}

inline void toJuce (const dc::MidiBuffer& dcBuf, juce::MidiBuffer& juceBuf)
{
    for (auto it = dcBuf.begin(); it != dcBuf.end(); ++it)
    {
        auto event = *it;
        juceBuf.addEvent (juce::MidiMessage (event.message.getRawData(),
                                              event.message.getRawDataSize()),
                          event.sampleOffset);
    }
}

}} // namespace dc::bridge

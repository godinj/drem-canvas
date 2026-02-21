#include "MidiClip.h"

namespace dc
{

namespace
{
    const juce::Identifier midiDataId ("midiData");
}

MidiClip::MidiClip (const juce::ValueTree& s)
    : state (s)
{
    jassert (state.hasType (IDs::MIDI_CLIP));
}

int64_t MidiClip::getStartPosition() const
{
    return static_cast<int64_t> (static_cast<juce::int64> (state.getProperty (IDs::startPosition, 0)));
}

void MidiClip::setStartPosition (int64_t pos, juce::UndoManager* um)
{
    state.setProperty (IDs::startPosition, static_cast<juce::int64> (pos), um);
}

int64_t MidiClip::getLength() const
{
    return static_cast<int64_t> (static_cast<juce::int64> (state.getProperty (IDs::length, 0)));
}

void MidiClip::setLength (int64_t len, juce::UndoManager* um)
{
    state.setProperty (IDs::length, static_cast<juce::int64> (len), um);
}

juce::MidiMessageSequence MidiClip::getMidiSequence() const
{
    juce::MidiMessageSequence result;

    juce::String base64Data = state.getProperty (midiDataId, juce::String());
    if (base64Data.isEmpty())
        return result;

    juce::MemoryBlock block;
    if (! block.fromBase64Encoding (base64Data))
        return result;

    juce::MemoryInputStream stream (block, false);

    while (! stream.isExhausted())
    {
        double timestamp = stream.readDouble();
        int messageSize = stream.readInt();

        if (messageSize <= 0 || messageSize > 1024)
            break;

        juce::MemoryBlock msgData ((size_t) messageSize);
        if (stream.read (msgData.getData(), messageSize) != messageSize)
            break;

        auto msg = juce::MidiMessage (msgData.getData(), messageSize);
        msg.setTimeStamp (timestamp);
        result.addEvent (msg);
    }

    return result;
}

void MidiClip::setMidiSequence (const juce::MidiMessageSequence& seq, juce::UndoManager* um)
{
    juce::MemoryOutputStream stream;

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        const auto* event = seq.getEventPointer (i);
        const auto& msg = event->message;

        stream.writeDouble (msg.getTimeStamp());
        stream.writeInt (msg.getRawDataSize());
        stream.write (msg.getRawData(), (size_t) msg.getRawDataSize());
    }

    juce::MemoryBlock block (stream.getData(), stream.getDataSize());
    juce::String base64Data = block.toBase64Encoding();

    state.setProperty (midiDataId, base64Data, um);
}

} // namespace dc

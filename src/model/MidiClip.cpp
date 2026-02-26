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

void MidiClip::expandNotesToChildren()
{
    // Remove existing NOTE and CC_POINT children
    for (int i = state.getNumChildren() - 1; i >= 0; --i)
    {
        auto child = state.getChild (i);
        if (child.hasType (juce::Identifier ("NOTE"))
            || child.hasType (juce::Identifier ("CC_POINT")))
            state.removeChild (i, nullptr);
    }

    // Decode base64 → MidiMessageSequence → NOTE + CC_POINT children
    auto seq = getMidiSequence();
    seq.updateMatchedPairs();

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        const auto* event = seq.getEventPointer (i);
        const auto& msg = event->message;

        if (msg.isNoteOn())
        {
            double startBeat = msg.getTimeStamp();
            double lengthBeats = 0.25; // default

            if (event->noteOffObject != nullptr)
                lengthBeats = event->noteOffObject->message.getTimeStamp() - startBeat;

            if (lengthBeats <= 0.0)
                lengthBeats = 0.25;

            juce::ValueTree noteChild ("NOTE");
            noteChild.setProperty ("noteNumber", msg.getNoteNumber(), nullptr);
            noteChild.setProperty ("startBeat", startBeat, nullptr);
            noteChild.setProperty ("lengthBeats", lengthBeats, nullptr);
            noteChild.setProperty ("velocity", msg.getVelocity(), nullptr);

            state.appendChild (noteChild, nullptr);
        }
        else if (msg.isController())
        {
            juce::ValueTree ccPoint ("CC_POINT");
            ccPoint.setProperty ("ccNumber", msg.getControllerNumber(), nullptr);
            ccPoint.setProperty ("beat", msg.getTimeStamp(), nullptr);
            ccPoint.setProperty ("value", msg.getControllerValue(), nullptr);

            state.appendChild (ccPoint, nullptr);
        }
    }
}

void MidiClip::collapseChildrenToMidiData (juce::UndoManager* um)
{
    juce::MidiMessageSequence seq;

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild (i);

        if (child.hasType (juce::Identifier ("NOTE")))
        {
            int noteNum = static_cast<int> (child.getProperty ("noteNumber", 60));
            auto startBeat = static_cast<double> (child.getProperty ("startBeat", 0.0));
            auto lengthBeats = static_cast<double> (child.getProperty ("lengthBeats", 0.25));
            int vel = static_cast<int> (child.getProperty ("velocity", 100));

            auto noteOn = juce::MidiMessage::noteOn (1, noteNum, (juce::uint8) vel);
            noteOn.setTimeStamp (startBeat);
            seq.addEvent (noteOn);

            auto noteOff = juce::MidiMessage::noteOff (1, noteNum);
            noteOff.setTimeStamp (startBeat + lengthBeats);
            seq.addEvent (noteOff);
        }
        else if (child.hasType (juce::Identifier ("CC_POINT")))
        {
            int ccNum = static_cast<int> (child.getProperty ("ccNumber", 1));
            auto beat = static_cast<double> (child.getProperty ("beat", 0.0));
            int value = static_cast<int> (child.getProperty ("value", 0));

            auto ccMsg = juce::MidiMessage::controllerEvent (1, ccNum, value);
            ccMsg.setTimeStamp (beat);
            seq.addEvent (ccMsg);
        }
    }

    seq.updateMatchedPairs();
    setMidiSequence (seq, um);
}

juce::ValueTree MidiClip::addNote (int noteNumber, double startBeat, double lengthBeats,
                                    int velocity, juce::UndoManager* um)
{
    juce::ValueTree noteChild ("NOTE");
    noteChild.setProperty ("noteNumber", noteNumber, um);
    noteChild.setProperty ("startBeat", startBeat, um);
    noteChild.setProperty ("lengthBeats", lengthBeats, um);
    noteChild.setProperty ("velocity", velocity, um);

    state.appendChild (noteChild, um);
    collapseChildrenToMidiData (um);
    return noteChild;
}

void MidiClip::removeNote (int childIndex, juce::UndoManager* um)
{
    if (childIndex >= 0 && childIndex < state.getNumChildren())
    {
        state.removeChild (childIndex, um);
        collapseChildrenToMidiData (um);
    }
}

} // namespace dc

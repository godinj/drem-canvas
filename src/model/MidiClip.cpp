#include "MidiClip.h"
#include "dc/foundation/assert.h"
#include "dc/foundation/base64.h"
#include "dc/midi/MidiMessage.h"

namespace dc
{

namespace
{
    const dc::PropertyId midiDataId ("midiData");
    const dc::PropertyId noteTypeId ("NOTE");
    const dc::PropertyId ccPointTypeId ("CC_POINT");
    const dc::PropertyId noteNumberPropId ("noteNumber");
    const dc::PropertyId startBeatPropId ("startBeat");
    const dc::PropertyId lengthBeatsPropId ("lengthBeats");
    const dc::PropertyId velocityPropId ("velocity");
    const dc::PropertyId ccNumberPropId ("ccNumber");
    const dc::PropertyId beatPropId ("beat");
    const dc::PropertyId valuePropId ("value");
}

MidiClip::MidiClip (const PropertyTree& s)
    : state (s)
{
    dc_assert (state.getType() == IDs::MIDI_CLIP);
}

int64_t MidiClip::getStartPosition() const
{
    return state.getProperty (IDs::startPosition).getIntOr (0);
}

void MidiClip::setStartPosition (int64_t pos, UndoManager* um)
{
    state.setProperty (IDs::startPosition, Variant (pos), um);
}

int64_t MidiClip::getLength() const
{
    return state.getProperty (IDs::length).getIntOr (0);
}

void MidiClip::setLength (int64_t len, UndoManager* um)
{
    state.setProperty (IDs::length, Variant (len), um);
}

MidiSequence MidiClip::getMidiSequence() const
{
    std::string base64Data = state.getProperty (midiDataId).getStringOr ("");
    if (base64Data.empty())
        return {};

    auto binary = dc::base64Decode (base64Data);
    if (binary.empty())
        return {};

    return MidiSequence::fromBinary (binary);
}

void MidiClip::setMidiSequence (const MidiSequence& seq, UndoManager* um)
{
    auto binary = seq.toBinary();
    auto base64Data = dc::base64Encode (binary);

    state.setProperty (midiDataId, Variant (base64Data), um);
}

void MidiClip::expandNotesToChildren()
{
    // Remove existing NOTE and CC_POINT children
    for (int i = state.getNumChildren() - 1; i >= 0; --i)
    {
        auto child = state.getChild (i);
        if (child.getType() == noteTypeId
            || child.getType() == ccPointTypeId)
            state.removeChild (i, nullptr);
    }

    // Decode base64 → MidiSequence → NOTE + CC_POINT children
    auto seq = getMidiSequence();
    seq.updateMatchedPairs();

    for (int i = 0; i < seq.getNumEvents(); ++i)
    {
        const auto& event = seq.getEvent (i);
        const auto& msg = event.message;

        if (msg.isNoteOn())
        {
            double startBeat = event.timeInBeats;
            double lengthBeats = 0.25; // default

            if (event.matchedPairIndex >= 0)
                lengthBeats = seq.getEvent (event.matchedPairIndex).timeInBeats - startBeat;

            if (lengthBeats <= 0.0)
                lengthBeats = 0.25;

            PropertyTree noteChild (noteTypeId);
            noteChild.setProperty (noteNumberPropId, Variant (msg.getNoteNumber()), nullptr);
            noteChild.setProperty (startBeatPropId, Variant (startBeat), nullptr);
            noteChild.setProperty (lengthBeatsPropId, Variant (lengthBeats), nullptr);
            noteChild.setProperty (velocityPropId, Variant (msg.getVelocity()), nullptr);

            state.addChild (noteChild, -1, nullptr);
        }
        else if (msg.isController())
        {
            PropertyTree ccPoint (ccPointTypeId);
            ccPoint.setProperty (ccNumberPropId, Variant (msg.getControllerNumber()), nullptr);
            ccPoint.setProperty (beatPropId, Variant (event.timeInBeats), nullptr);
            ccPoint.setProperty (valuePropId, Variant (msg.getControllerValue()), nullptr);

            state.addChild (ccPoint, -1, nullptr);
        }
    }
}

void MidiClip::collapseChildrenToMidiData (UndoManager* um)
{
    MidiSequence seq;

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild (i);

        if (child.getType() == noteTypeId)
        {
            int noteNum = static_cast<int> (child.getProperty (noteNumberPropId).getIntOr (60));
            auto startBeat = child.getProperty (startBeatPropId).getDoubleOr (0.0);
            auto lengthBeats = child.getProperty (lengthBeatsPropId).getDoubleOr (0.25);
            int vel = static_cast<int> (child.getProperty (velocityPropId).getIntOr (100));

            float velocity = static_cast<float> (vel) / 127.0f;
            auto noteOn = dc::MidiMessage::noteOn (1, noteNum, velocity);
            seq.addEvent (noteOn, startBeat);

            auto noteOff = dc::MidiMessage::noteOff (1, noteNum);
            seq.addEvent (noteOff, startBeat + lengthBeats);
        }
        else if (child.getType() == ccPointTypeId)
        {
            int ccNum = static_cast<int> (child.getProperty (ccNumberPropId).getIntOr (1));
            auto beat = child.getProperty (beatPropId).getDoubleOr (0.0);
            int value = static_cast<int> (child.getProperty (valuePropId).getIntOr (0));

            auto ccMsg = dc::MidiMessage::controllerEvent (1, ccNum, value);
            seq.addEvent (ccMsg, beat);
        }
    }

    seq.updateMatchedPairs();
    setMidiSequence (seq, um);
}

PropertyTree MidiClip::addNote (int noteNumber, double startBeat, double lengthBeats,
                                int velocity, UndoManager* um)
{
    PropertyTree noteChild (noteTypeId);
    noteChild.setProperty (noteNumberPropId, Variant (noteNumber), um);
    noteChild.setProperty (startBeatPropId, Variant (startBeat), um);
    noteChild.setProperty (lengthBeatsPropId, Variant (lengthBeats), um);
    noteChild.setProperty (velocityPropId, Variant (velocity), um);

    state.addChild (noteChild, -1, um);
    collapseChildrenToMidiData (um);
    return noteChild;
}

void MidiClip::removeNote (int childIndex, UndoManager* um)
{
    if (childIndex >= 0 && childIndex < state.getNumChildren())
    {
        state.removeChild (childIndex, um);
        collapseChildrenToMidiData (um);
    }
}

} // namespace dc

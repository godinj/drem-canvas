#include "MidiClipProcessor.h"
#include <cmath>

namespace dc
{

MidiClipProcessor::MidiClipProcessor (TransportController& transport)
    : transportController (transport)
{
}

void MidiClipProcessor::prepare (double sampleRate, int /*maxBlockSize*/)
{
    currentSampleRate = sampleRate;
    numPendingNoteOffs = 0;
}

void MidiClipProcessor::injectLiveMidi (const dc::MidiMessage& msg)
{
    liveMidiFifo.push (msg);
}

void MidiClipProcessor::drainLiveMidiFifo (MidiBlock& midi)
{
    dc::MidiMessage msg;
    while (liveMidiFifo.pop (msg))
        midi.addEvent (msg, 0);
}

void MidiClipProcessor::process (AudioBlock& audio, MidiBlock& midi, int numSamples)
{
    audio.clear();

    // Always drain live MIDI — allows playing even when transport is stopped
    drainLiveMidiFifo (midi);

    if (! transportController.isPlaying())
    {
        // Send note-offs for any remaining notes
        for (int i = 0; i < numPendingNoteOffs; ++i)
            midi.addEvent (dc::MidiMessage::noteOff (pendingNoteOffs[i].channel,
                                                      pendingNoteOffs[i].noteNumber), 0);
        numPendingNoteOffs = 0;
        return;
    }

    // Swap to new snapshot if available
    if (newDataReady.load())
    {
        readIndex.store (writeIndex.load());
        writeIndex.store (1 - readIndex.load());
        newDataReady.store (false);
    }

    const auto& snap = snapshots[readIndex.load()];
    if (snap.numEvents == 0)
        return;

    const int64_t blockStart = transportController.getPositionInSamples();
    const int64_t blockEnd = blockStart + numSamples;

    // Flush pending note-offs on position discontinuity (loop wrap or seek)
    if (blockStart < previousBlockStart && numPendingNoteOffs > 0)
    {
        for (int i = 0; i < numPendingNoteOffs; ++i)
            midi.addEvent (dc::MidiMessage::noteOff (pendingNoteOffs[i].channel,
                                                      pendingNoteOffs[i].noteNumber), 0);
        numPendingNoteOffs = 0;
    }
    previousBlockStart = blockStart;

    // Process pending note-offs first
    processNoteOffs (midi, blockStart, numSamples);

    // Scan events for note-ons and note-offs in this block range
    for (int e = 0; e < snap.numEvents; ++e)
    {
        const auto& evt = snap.events[e];

        // Events are sorted by onSample — if past the block, stop scanning
        if (evt.onSample >= blockEnd)
            break;

        // Note-on: emit if onSample falls in [blockStart, blockEnd)
        if (evt.onSample >= blockStart && evt.onSample < blockEnd)
        {
            int offset = static_cast<int> (evt.onSample - blockStart);
            int vel = std::clamp (evt.velocity, 1, 127);

            midi.addEvent (
                dc::MidiMessage::noteOn (evt.channel, evt.noteNumber,
                                          static_cast<float> (vel) / 127.0f),
                offset);

            // Schedule note-off
            addNoteOff (evt.noteNumber, evt.channel, evt.offSample);
        }

        // Note-off: if offSample falls in this block but onSample was in a prior block
        if (evt.onSample < blockStart && evt.offSample >= blockStart && evt.offSample < blockEnd)
        {
            int offset = static_cast<int> (evt.offSample - blockStart);
            midi.addEvent (
                dc::MidiMessage::noteOff (evt.channel, evt.noteNumber),
                offset);
        }
    }
}

void MidiClipProcessor::updateSnapshot (const MidiTrackSnapshot& snapshot)
{
    int wi = 1 - readIndex.load();
    snapshots[wi] = snapshot;
    writeIndex.store (wi);
    newDataReady.store (true);
}

void MidiClipProcessor::addNoteOff (int noteNumber, int channel, int64_t offSample)
{
    if (numPendingNoteOffs < maxPendingNoteOffs)
    {
        pendingNoteOffs[numPendingNoteOffs++] = { noteNumber, channel, offSample };
    }
}

void MidiClipProcessor::processNoteOffs (MidiBlock& midi,
                                           int64_t blockStart, int numSamples)
{
    int64_t blockEnd = blockStart + numSamples;
    int remaining = 0;

    for (int i = 0; i < numPendingNoteOffs; ++i)
    {
        auto& noff = pendingNoteOffs[i];

        if (noff.offSample < blockEnd)
        {
            int offset = static_cast<int> (std::max (noff.offSample - blockStart, int64_t (0)));
            offset = std::min (offset, numSamples - 1);
            midi.addEvent (
                dc::MidiMessage::noteOff (noff.channel, noff.noteNumber),
                offset);
        }
        else
        {
            pendingNoteOffs[remaining++] = noff;
        }
    }

    numPendingNoteOffs = remaining;
}

} // namespace dc

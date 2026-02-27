#include "MidiClipProcessor.h"
#include <cmath>

namespace dc
{

MidiClipProcessor::MidiClipProcessor (TransportController& transport)
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      transportController (transport)
{
}

void MidiClipProcessor::prepareToPlay (double sampleRate, int /*maximumExpectedSamplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    numPendingNoteOffs = 0;
}

void MidiClipProcessor::releaseResources()
{
}

void MidiClipProcessor::injectLiveMidi (const juce::MidiMessage& msg)
{
    const auto scope = liveMidiFifo.write (1);
    if (scope.blockSize1 > 0)
        liveMidiBuffer[static_cast<size_t> (scope.startIndex1)] = msg;
    else if (scope.blockSize2 > 0)
        liveMidiBuffer[static_cast<size_t> (scope.startIndex2)] = msg;
}

void MidiClipProcessor::drainLiveMidiFifo (juce::MidiBuffer& midiMessages)
{
    int numReady = liveMidiFifo.getNumReady();
    if (numReady == 0)
        return;

    const auto scope = liveMidiFifo.read (numReady);
    for (int i = 0; i < scope.blockSize1; ++i)
        midiMessages.addEvent (liveMidiBuffer[static_cast<size_t> (scope.startIndex1 + i)], 0);
    for (int i = 0; i < scope.blockSize2; ++i)
        midiMessages.addEvent (liveMidiBuffer[static_cast<size_t> (scope.startIndex2 + i)], 0);
}

void MidiClipProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    // Always drain live MIDI — allows playing even when transport is stopped
    drainLiveMidiFifo (midiMessages);

    if (! transportController.isPlaying())
    {
        // Send note-offs for any remaining notes
        for (int i = 0; i < numPendingNoteOffs; ++i)
            midiMessages.addEvent (juce::MidiMessage::noteOff (pendingNoteOffs[i].channel,
                                                                pendingNoteOffs[i].noteNumber),
                                   0);
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

    const int numSamples = buffer.getNumSamples();
    const int64_t blockStart = transportController.getPositionInSamples();
    const int64_t blockEnd = blockStart + numSamples;

    // Process pending note-offs first
    processNoteOffs (midiMessages, blockStart, numSamples);

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
            int vel = juce::jlimit (1, 127, evt.velocity);

            midiMessages.addEvent (
                juce::MidiMessage::noteOn (evt.channel, evt.noteNumber,
                                           static_cast<juce::uint8> (vel)),
                offset);

            // Schedule note-off
            addNoteOff (evt.noteNumber, evt.channel, evt.offSample);
        }

        // Note-off: if offSample falls in this block but onSample was in a prior block
        if (evt.onSample < blockStart && evt.offSample >= blockStart && evt.offSample < blockEnd)
        {
            int offset = static_cast<int> (evt.offSample - blockStart);
            midiMessages.addEvent (
                juce::MidiMessage::noteOff (evt.channel, evt.noteNumber),
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

void MidiClipProcessor::processNoteOffs (juce::MidiBuffer& midiMessages,
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
            midiMessages.addEvent (
                juce::MidiMessage::noteOff (noff.channel, noff.noteNumber),
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

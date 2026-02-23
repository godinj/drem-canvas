#include "StepSequencerProcessor.h"
#include <cmath>

namespace dc
{

StepSequencerProcessor::StepSequencerProcessor (TransportController& transport)
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      transportController (transport)
{
}

void StepSequencerProcessor::prepareToPlay (double sampleRate, int /*maximumExpectedSamplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    previousStepPosition = 0.0;
    numPendingNoteOffs = 0;
}

void StepSequencerProcessor::releaseResources()
{
}

void StepSequencerProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                            juce::MidiBuffer& midiMessages)
{
    buffer.clear();

    if (! transportController.isPlaying())
    {
        currentStep.store (-1);
        previousStepPosition = 0.0;
        // Send note-offs for any remaining notes
        for (int i = 0; i < numPendingNoteOffs; ++i)
            midiMessages.addEvent (juce::MidiMessage::noteOff (pendingNoteOffs[i].channel,
                                                                pendingNoteOffs[i].noteNumber),
                                   0);
        numPendingNoteOffs = 0;
        return;
    }

    // Swap to new pattern if available
    if (newDataReady.load())
    {
        readIndex.store (writeIndex.load());
        writeIndex.store (1 - readIndex.load());
        newDataReady.store (false);
    }

    const auto& pattern = snapshots[readIndex.load()];
    if (pattern.numRows == 0 || pattern.numSteps == 0)
        return;

    const double currentTempo = tempo.load();
    const int numSamples = buffer.getNumSamples();
    const int64_t blockStartSample = transportController.getPositionInSamples();

    // Process pending note-offs
    processNoteOffs (midiMessages, blockStartSample, numSamples);

    // Steps per second: (tempo / 60) * stepDivision
    const double stepsPerSecond = (currentTempo / 60.0) * static_cast<double> (pattern.stepDivision);

    for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
    {
        const int64_t samplePos = blockStartSample + sampleIdx;
        const double timeInSeconds = static_cast<double> (samplePos) / currentSampleRate;
        double stepPosition = timeInSeconds * stepsPerSecond;

        // Apply swing to even-numbered steps (1, 3, 5, ...)
        // Swing shifts odd 16th notes later
        // We detect if we're on an "even step boundary" in the raw position

        // Detect step boundary crossing
        double currentStepFloor = std::floor (stepPosition);
        double previousStepFloor = std::floor (previousStepPosition);

        if (currentStepFloor > previousStepFloor && stepPosition >= 0.0)
        {
            int stepIndex = static_cast<int> (currentStepFloor) % pattern.numSteps;
            currentStep.store (stepIndex);

            // Calculate step duration in samples for note-off scheduling
            double stepDurationSec = 1.0 / stepsPerSecond;

            // Fire notes for each row
            for (int r = 0; r < pattern.numRows; ++r)
            {
                const auto& row = pattern.rows[r];

                // Skip muted rows
                if (row.mute)
                    continue;

                // If any row is soloed, only play soloed rows
                if (pattern.hasSoloedRow && ! row.solo)
                    continue;

                const auto& step = row.steps[stepIndex];

                if (! step.active)
                    continue;

                // Probability check
                if (step.probability < 1.0)
                {
                    double rand = static_cast<double> (juce::Random::getSystemRandom().nextFloat());
                    if (rand > step.probability)
                        continue;
                }

                int vel = juce::jlimit (1, 127, step.velocity);
                int channel = 10; // MIDI drum channel

                // Note on
                midiMessages.addEvent (
                    juce::MidiMessage::noteOn (channel, row.noteNumber, static_cast<juce::uint8> (vel)),
                    sampleIdx);

                // Schedule note-off
                double noteDurationSec = stepDurationSec * step.noteLength;
                int64_t offSample = samplePos + static_cast<int64_t> (noteDurationSec * currentSampleRate);
                addNoteOff (row.noteNumber, channel, offSample);
            }
        }

        previousStepPosition = stepPosition;
    }
}

void StepSequencerProcessor::updatePatternSnapshot (const PatternSnapshot& snapshot)
{
    int wi = 1 - readIndex.load();
    snapshots[wi] = snapshot;
    writeIndex.store (wi);
    newDataReady.store (true);
}

void StepSequencerProcessor::addNoteOff (int noteNumber, int channel, int64_t offSample)
{
    if (numPendingNoteOffs < maxPendingNoteOffs)
    {
        pendingNoteOffs[numPendingNoteOffs++] = { noteNumber, channel, offSample };
    }
}

void StepSequencerProcessor::processNoteOffs (juce::MidiBuffer& midiMessages,
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

#include "MetronomeProcessor.h"
#include <cmath>

namespace dc
{

MetronomeProcessor::MetronomeProcessor (TransportController& transport)
    : AudioProcessor (BusesProperties()
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      transportController (transport)
{
}

void MetronomeProcessor::prepareToPlay (double sampleRate, int /*maximumExpectedSamplesPerBlock*/)
{
    currentSampleRate = sampleRate;
    clickSampleLength = static_cast<int> (sampleRate * 0.02); // 20ms click
    clickSamplePos = clickSampleLength; // Start in "not clicking" state
    previousBeatPosition = 0.0;
}

void MetronomeProcessor::releaseResources()
{
    // Nothing to release
}

void MetronomeProcessor::processBlock (juce::AudioBuffer<float>& buffer,
                                        juce::MidiBuffer& /*midiMessages*/)
{
    if (! enabled.load() || ! transportController.isPlaying())
    {
        buffer.clear();
        return;
    }

    const double currentTempo = tempo.load();
    const float currentVolume = volume.load();
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    const int64_t posInSamples = transportController.getPositionInSamples();

    buffer.clear();

    for (int sampleIdx = 0; sampleIdx < numSamples; ++sampleIdx)
    {
        // Calculate current beat position
        double beatPosition = static_cast<double> (posInSamples + sampleIdx)
                              / (currentSampleRate * 60.0 / currentTempo);

        // Detect beat boundary crossing
        double currentBeatFloor = std::floor (beatPosition);
        double previousBeatFloor = std::floor (previousBeatPosition);

        if (currentBeatFloor > previousBeatFloor && beatPosition >= 0.0)
        {
            // A new beat has started
            clickSamplePos = 0;

            // Determine if this is a downbeat (beat 0 of a bar)
            int beatInBar = static_cast<int> (currentBeatFloor) % beatsPerBar.load();
            isDownbeat = (beatInBar == 0);
        }

        previousBeatPosition = beatPosition;

        // Generate click sound if we're within the click duration
        if (clickSamplePos < clickSampleLength)
        {
            double freq = isDownbeat ? clickFrequency : clickFrequencyOff;

            // Generate sine wave
            double phase = 2.0 * juce::MathConstants<double>::pi * freq
                           * static_cast<double> (clickSamplePos) / currentSampleRate;
            float sample = static_cast<float> (std::sin (phase));

            // Apply quick decay envelope
            float envelope = 1.0f - static_cast<float> (clickSamplePos)
                             / static_cast<float> (clickSampleLength);
            envelope = envelope * envelope; // Quadratic decay for snappier sound

            sample *= envelope * currentVolume;

            // Boost downbeat slightly
            if (isDownbeat)
                sample *= 1.3f;

            // Write to all channels
            for (int ch = 0; ch < numChannels; ++ch)
                buffer.addSample (ch, sampleIdx, sample);

            ++clickSamplePos;
        }
    }
}

} // namespace dc

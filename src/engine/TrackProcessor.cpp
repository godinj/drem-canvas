#include "TrackProcessor.h"
#include <cmath>

namespace dc
{

TrackProcessor::TrackProcessor (TransportController& transport)
    : transportController (transport)
{
    formatManager.registerBasicFormats();
}

TrackProcessor::~TrackProcessor()
{
    transportSource.setSource (nullptr);
    readerSource.reset();
}

bool TrackProcessor::loadFile (const juce::File& file)
{
    auto* reader = formatManager.createReaderFor (file);

    if (reader == nullptr)
        return false;

    readerSource = std::make_unique<juce::AudioFormatReaderSource> (reader, true);
    transportSource.setSource (readerSource.get(), 0, nullptr, reader->sampleRate);

    return true;
}

void TrackProcessor::clearFile()
{
    transportSource.setSource (nullptr);
    readerSource.reset();
}

void TrackProcessor::prepareToPlay (double sampleRate, int maximumExpectedSamplesPerBlock)
{
    transportSource.prepareToPlay (maximumExpectedSamplesPerBlock, sampleRate);
}

void TrackProcessor::releaseResources()
{
    transportSource.releaseResources();
}

void TrackProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    if (muted.load())
    {
        buffer.clear();
        return;
    }

    // Sync transport source position with the transport controller
    double sr = transportController.getSampleRate();

    if (sr > 0.0)
    {
        double positionInSeconds = static_cast<double> (transportController.getPositionInSamples()) / sr;
        double currentPos = transportSource.getCurrentPosition();

        // Only seek if positions differ significantly (avoid constant seeking noise)
        if (std::abs (currentPos - positionInSeconds) > 0.01)
            transportSource.setPosition (positionInSeconds);
    }

    // Start or stop the transport source based on transport controller state
    if (transportController.isPlaying() && readerSource != nullptr)
    {
        if (! transportSource.isPlaying())
            transportSource.start();
    }
    else
    {
        if (transportSource.isPlaying())
            transportSource.stop();
    }

    // Get the next audio block from the transport source
    juce::AudioSourceChannelInfo channelInfo (buffer);
    transportSource.getNextAudioBlock (channelInfo);

    // Apply gain and pan
    float currentGain = gain.load();
    float currentPan  = pan.load();

    // Equal-power panning
    // pan ranges from -1.0 (full left) to 1.0 (full right), 0.0 = center
    float angle   = currentPan * juce::MathConstants<float>::pi * 0.25f + juce::MathConstants<float>::pi * 0.25f;
    float leftAmp  = currentGain * std::cos (angle);
    float rightAmp = currentGain * std::sin (angle);

    int numChannels = buffer.getNumChannels();

    if (numChannels >= 1)
        buffer.applyGain (0, 0, buffer.getNumSamples(), leftAmp);

    if (numChannels >= 2)
        buffer.applyGain (1, 0, buffer.getNumSamples(), rightAmp);
}

int64_t TrackProcessor::getFileLengthInSamples() const
{
    if (readerSource != nullptr)
        return readerSource->getTotalLength();

    return 0;
}

} // namespace dc

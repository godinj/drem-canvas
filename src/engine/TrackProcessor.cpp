#include "TrackProcessor.h"
#include "dc/audio/AudioBlock.h"
#include "dc/foundation/types.h"
#include <cmath>

namespace dc
{

TrackProcessor::TrackProcessor (TransportController& transport)
    : AudioProcessor (BusesProperties()
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      transportController (transport)
{
}

TrackProcessor::~TrackProcessor()
{
    if (diskStreamer)
    {
        diskStreamer->stop();
        diskStreamer.reset();
    }
}

bool TrackProcessor::loadFile (const std::filesystem::path& file)
{
    clearFile();

    diskStreamer = std::make_unique<dc::DiskStreamer>();

    if (! diskStreamer->open (file))
    {
        diskStreamer.reset();
        return false;
    }

    diskStreamer->start();
    lastSeekPosition = -1;
    return true;
}

void TrackProcessor::clearFile()
{
    if (diskStreamer)
    {
        diskStreamer->stop();
        diskStreamer.reset();
    }

    lastSeekPosition = -1;
}

void TrackProcessor::prepareToPlay (double /*sampleRate*/, int /*maximumExpectedSamplesPerBlock*/)
{
    // DiskStreamer manages its own background thread — nothing to prepare
}

void TrackProcessor::releaseResources()
{
    if (diskStreamer)
        diskStreamer->stop();
}

void TrackProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& /*midiMessages*/)
{
    if (muted.load() || diskStreamer == nullptr)
    {
        buffer.clear();
        return;
    }

    // Seek if transport position changed
    int64_t posInSamples = transportController.getPositionInSamples();

    if (! transportController.isPlaying())
    {
        buffer.clear();
        lastSeekPosition = -1;
        return;
    }

    // Seek on position mismatch (e.g. user scrub, loop, etc.)
    if (lastSeekPosition < 0 || posInSamples != lastSeekPosition)
        diskStreamer->seek (posInSamples);

    // Read from DiskStreamer into the juce::AudioBuffer via dc::AudioBlock wrapper
    dc::AudioBlock block (buffer.getArrayOfWritePointers(),
                          buffer.getNumChannels(),
                          buffer.getNumSamples());
    block.clear();
    diskStreamer->read (block, buffer.getNumSamples());

    lastSeekPosition = posInSamples + buffer.getNumSamples();

    // Apply gain and pan
    float currentGain = gain.load();
    float currentPan  = pan.load();

    // Equal-power panning
    // pan ranges from -1.0 (full left) to 1.0 (full right), 0.0 = center
    float angle   = currentPan * dc::pi<float> * 0.25f + dc::pi<float> * 0.25f;
    float leftAmp  = currentGain * std::cos (angle);
    float rightAmp = currentGain * std::sin (angle);

    int numChannels = buffer.getNumChannels();

    if (numChannels >= 1)
        buffer.applyGain (0, 0, buffer.getNumSamples(), leftAmp);

    if (numChannels >= 2)
        buffer.applyGain (1, 0, buffer.getNumSamples(), rightAmp);

    // Update peak meters
    if (numChannels >= 1)
    {
        float mag = buffer.getMagnitude (0, 0, buffer.getNumSamples());
        float old = peakLeft.load();
        peakLeft.store (std::max (mag, old * 0.95f));
    }

    if (numChannels >= 2)
    {
        float mag = buffer.getMagnitude (1, 0, buffer.getNumSamples());
        float old = peakRight.load();
        peakRight.store (std::max (mag, old * 0.95f));
    }
}

int64_t TrackProcessor::getFileLengthInSamples() const
{
    if (diskStreamer)
        return diskStreamer->getLengthInSamples();

    return 0;
}

} // namespace dc

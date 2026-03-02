#include "TrackProcessor.h"
#include "dc/audio/AudioBlock.h"
#include "dc/foundation/types.h"
#include <cmath>
#include <algorithm>

namespace dc
{

TrackProcessor::TrackProcessor (TransportController& transport)
    : transportController (transport)
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

void TrackProcessor::prepare (double /*sampleRate*/, int /*maxBlockSize*/)
{
    // DiskStreamer manages its own background thread — nothing to prepare
}

void TrackProcessor::release()
{
    if (diskStreamer)
        diskStreamer->stop();
}

void TrackProcessor::process (AudioBlock& audio, MidiBlock& /*midi*/, int numSamples)
{
    if (muted.load() || diskStreamer == nullptr)
    {
        audio.clear();
        return;
    }

    // Seek if transport position changed
    int64_t posInSamples = transportController.getPositionInSamples();

    if (! transportController.isPlaying())
    {
        audio.clear();
        lastSeekPosition = -1;
        return;
    }

    // Seek on position mismatch (e.g. user scrub, loop, etc.)
    if (lastSeekPosition < 0 || posInSamples != lastSeekPosition)
        diskStreamer->seek (posInSamples);

    // Read from DiskStreamer directly into the AudioBlock
    audio.clear();
    diskStreamer->read (audio, numSamples);

    lastSeekPosition = posInSamples + numSamples;

    // Apply gain and pan
    float currentGain = gain.load();
    float currentPan  = pan.load();

    // Equal-power panning
    // pan ranges from -1.0 (full left) to 1.0 (full right), 0.0 = center
    float angle   = currentPan * dc::pi<float> * 0.25f + dc::pi<float> * 0.25f;
    float leftAmp  = currentGain * std::cos (angle);
    float rightAmp = currentGain * std::sin (angle);

    int numChannels = audio.getNumChannels();

    if (numChannels >= 1)
    {
        float* data = audio.getChannel (0);
        for (int i = 0; i < numSamples; ++i)
            data[i] *= leftAmp;
    }

    if (numChannels >= 2)
    {
        float* data = audio.getChannel (1);
        for (int i = 0; i < numSamples; ++i)
            data[i] *= rightAmp;
    }

    // Update peak meters
    if (numChannels >= 1)
    {
        const float* data = audio.getChannel (0);
        float mag = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            mag = std::max (mag, std::abs (data[i]));
        float old = peakLeft.load();
        peakLeft.store (std::max (mag, old * 0.95f));
    }

    if (numChannels >= 2)
    {
        const float* data = audio.getChannel (1);
        float mag = 0.0f;
        for (int i = 0; i < numSamples; ++i)
            mag = std::max (mag, std::abs (data[i]));
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

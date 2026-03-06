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
}

int64_t TrackProcessor::getFileLengthInSamples() const
{
    if (diskStreamer)
        return diskStreamer->getLengthInSamples();

    return 0;
}

} // namespace dc

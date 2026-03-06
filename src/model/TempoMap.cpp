#include "TempoMap.h"
#include "dc/foundation/assert.h"
#include <cmath>
#include <cstdio>

namespace dc
{

TempoMap::TempoMap() {}

void TempoMap::setTempo (double bpm)
{
    dc_assert (bpm > 0.0);
    if (bpm < 20.0)
        bpm = 20.0;
    else if (bpm > 300.0)
        bpm = 300.0;
    tempo = bpm;
}

void TempoMap::setTimeSig (int numerator, int denominator)
{
    dc_assert (numerator > 0 && denominator > 0);
    timeSigNum = numerator;
    timeSigDen = denominator;
}

double TempoMap::samplesToBeats (int64_t samples, double sampleRate) const
{
    double seconds = static_cast<double> (samples) / sampleRate;
    return seconds * tempo / 60.0;
}

int64_t TempoMap::beatsToSamples (double beats, double sampleRate) const
{
    double seconds = beats * 60.0 / tempo;
    return static_cast<int64_t> (std::round (seconds * sampleRate));
}

double TempoMap::samplesToSeconds (int64_t samples, double sampleRate) const
{
    return static_cast<double> (samples) / sampleRate;
}

int64_t TempoMap::secondsToSamples (double seconds, double sampleRate) const
{
    return static_cast<int64_t> (std::round (seconds * sampleRate));
}

double TempoMap::beatsToSeconds (double beats) const
{
    return beats * 60.0 / tempo;
}

double TempoMap::secondsToBeats (double seconds) const
{
    return seconds * tempo / 60.0;
}

TempoMap::BarBeatPosition TempoMap::samplesToBarBeat (int64_t samples, double sampleRate) const
{
    double totalBeats = samplesToBeats (samples, sampleRate);

    // Each bar has timeSigNum beats
    double beatsPerBar = static_cast<double> (timeSigNum);

    int bar = static_cast<int> (std::floor (totalBeats / beatsPerBar)) + 1;
    double beatInBar = std::fmod (totalBeats, beatsPerBar);

    if (beatInBar < 0.0)
        beatInBar += beatsPerBar;

    int beat = static_cast<int> (std::floor (beatInBar)) + 1;
    double tick = beatInBar - std::floor (beatInBar);

    return { bar, beat, tick };
}

std::string TempoMap::formatBarBeat (const BarBeatPosition& pos) const
{
    // Display tick as 0-999 range (like standard DAW tick display)
    int tickDisplay = static_cast<int> (std::round (pos.tick * 960.0));

    char buf[32];
    std::snprintf (buf, sizeof (buf), "%d.%d.%03d", pos.bar, pos.beat, tickDisplay);
    return std::string (buf);
}

} // namespace dc

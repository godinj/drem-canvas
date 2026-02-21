#include "TempoMap.h"
#include <cmath>

namespace dc
{

TempoMap::TempoMap() {}

void TempoMap::setTempo (double bpm)
{
    jassert (bpm > 0.0);
    tempo = bpm;
}

void TempoMap::setTimeSig (int numerator, int denominator)
{
    jassert (numerator > 0 && denominator > 0);
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

juce::String TempoMap::formatBarBeat (const BarBeatPosition& pos) const
{
    // Display tick as 0-999 range (like standard DAW tick display)
    int tickDisplay = static_cast<int> (std::round (pos.tick * 960.0));

    return juce::String (pos.bar) + "." + juce::String (pos.beat) + "."
           + juce::String (tickDisplay).paddedLeft ('0', 3);
}

} // namespace dc

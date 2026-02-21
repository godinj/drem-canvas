#pragma once
#include <JuceHeader.h>

namespace dc
{

class TempoMap
{
public:
    TempoMap();

    void setTempo (double bpm);
    double getTempo() const { return tempo; }

    void setTimeSig (int numerator, int denominator);
    int getTimeSigNumerator() const { return timeSigNum; }
    int getTimeSigDenominator() const { return timeSigDen; }

    // Conversion utilities
    double samplesToBeats (int64_t samples, double sampleRate) const;
    int64_t beatsToSamples (double beats, double sampleRate) const;
    double samplesToSeconds (int64_t samples, double sampleRate) const;
    int64_t secondsToSamples (double seconds, double sampleRate) const;
    double beatsToSeconds (double beats) const;
    double secondsToBeats (double seconds) const;

    // Bar/beat display
    struct BarBeatPosition
    {
        int bar;
        int beat;
        double tick; // fractional beat
    };

    BarBeatPosition samplesToBarBeat (int64_t samples, double sampleRate) const;
    juce::String formatBarBeat (const BarBeatPosition& pos) const;

private:
    double tempo = 120.0;
    int timeSigNum = 4;
    int timeSigDen = 4;
};

} // namespace dc

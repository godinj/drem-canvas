#pragma once
#include <JuceHeader.h>
#include "TempoMap.h"

namespace dc
{

class GridSystem
{
public:
    explicit GridSystem (const TempoMap& tempoMap);

    // Grid division: subdivisions per beat (1=quarter, 2=eighth, 4=sixteenth, etc.)
    int getGridDivision() const { return gridDivision; }
    void adjustGridDivision (int delta);

    // Grid unit size in samples
    int64_t getGridUnitInSamples (double sampleRate) const;

    // Snap position down to nearest grid boundary
    int64_t snapFloor (int64_t pos, double sampleRate) const;

    // Snap position to nearest grid boundary
    int64_t snapNearest (int64_t pos, double sampleRate) const;

    // Move position by N grid units (can be negative)
    int64_t moveByGridUnits (int64_t pos, int count, double sampleRate) const;

    // Format position as "Bar.Beat.Sub"
    juce::String formatGridPosition (int64_t pos, double sampleRate) const;

    // Grid division display name
    juce::String getGridDivisionName() const;

    const TempoMap& getTempoMap() const { return tempoMap; }

private:
    const TempoMap& tempoMap;
    int gridDivision = 4; // 1/16 notes by default (4 subdivisions per beat)
};

} // namespace dc

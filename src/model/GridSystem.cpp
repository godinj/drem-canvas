#include "GridSystem.h"
#include <cmath>
#include <algorithm>

namespace dc
{

GridSystem::GridSystem (const TempoMap& tm)
    : tempoMap (tm)
{
}

void GridSystem::adjustGridDivision (int delta)
{
    // Cycle through powers of 2: 1, 2, 4, 8, 16
    static const int divisions[] = { 1, 2, 4, 8, 16 };
    static const int numDivisions = 5;

    int currentIdx = 0;
    for (int i = 0; i < numDivisions; ++i)
    {
        if (divisions[i] == gridDivision)
        {
            currentIdx = i;
            break;
        }
    }

    int newIdx = std::clamp (currentIdx + delta, 0, numDivisions - 1);
    gridDivision = divisions[newIdx];
}

int64_t GridSystem::getGridUnitInSamples (double sampleRate) const
{
    // One beat = sampleRate * 60.0 / tempo
    // One grid unit = one beat / gridDivision
    double samplesPerBeat = sampleRate * 60.0 / tempoMap.getTempo();
    return static_cast<int64_t> (std::round (samplesPerBeat / gridDivision));
}

int64_t GridSystem::snapFloor (int64_t pos, double sampleRate) const
{
    if (pos <= 0)
        return 0;

    int64_t gridUnit = getGridUnitInSamples (sampleRate);
    if (gridUnit <= 0)
        return pos;

    return (pos / gridUnit) * gridUnit;
}

int64_t GridSystem::snapNearest (int64_t pos, double sampleRate) const
{
    if (pos <= 0)
        return 0;

    int64_t gridUnit = getGridUnitInSamples (sampleRate);
    if (gridUnit <= 0)
        return pos;

    int64_t lower = (pos / gridUnit) * gridUnit;
    int64_t upper = lower + gridUnit;

    return (pos - lower <= upper - pos) ? lower : upper;
}

int64_t GridSystem::moveByGridUnits (int64_t pos, int count, double sampleRate) const
{
    int64_t gridUnit = getGridUnitInSamples (sampleRate);
    if (gridUnit <= 0)
        return pos;

    int64_t result = pos + static_cast<int64_t> (count) * gridUnit;
    return std::max (result, static_cast<int64_t> (0));
}

juce::String GridSystem::formatGridPosition (int64_t pos, double sampleRate) const
{
    auto bbp = tempoMap.samplesToBarBeat (pos, sampleRate);

    // Calculate subdivision within the beat
    int sub = static_cast<int> (std::floor (bbp.tick * gridDivision)) + 1;
    sub = std::clamp (sub, 1, gridDivision);

    return juce::String (bbp.bar) + "." + juce::String (bbp.beat) + "." + juce::String (sub);
}

juce::String GridSystem::getGridDivisionName() const
{
    switch (gridDivision)
    {
        case 1:  return "1/4";
        case 2:  return "1/8";
        case 4:  return "1/16";
        case 8:  return "1/32";
        case 16: return "1/64";
        default: return "1/" + juce::String (gridDivision * 4);
    }
}

} // namespace dc

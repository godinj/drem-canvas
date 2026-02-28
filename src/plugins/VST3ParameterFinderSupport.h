#pragma once

#include <vector>
#include <map>


namespace dc
{

/**
 * Abstract interface for querying IParameterFinder support on a VST3 editor.
 *
 * JUCE's VST3PluginWindow inherits from this so that external code can
 * discover IParameterFinder support via dynamic_cast without pulling in
 * VST3 SDK headers. Uses unsigned int instead of Steinberg::Vst::ParamID
 * (which is uint32) to keep the interface SDK-agnostic.
 */
class VST3ParameterFinderSupport
{
public:
    virtual ~VST3ParameterFinderSupport() = default;

    /** Returns true if the plugin's IPlugView supports IParameterFinder. */
    virtual bool hasParameterFinder() const = 0;

    /** Query which parameter is at (xPos, yPos) in native plugin coordinates.
        Returns true if a parameter was found, with its ParamID in resultParamId. */
    virtual bool findParameterAt (int xPos, int yPos, unsigned int& resultParamId) = 0;

    /** Resolve a ParamID from IParameterFinder to the JUCE parameter index.
        Returns -1 if the ParamID could not be mapped. Tries direct lookup first,
        then falls back to querying the IEditController and matching by name. */
    virtual int resolveFinderParamIndex (unsigned int finderParamId) = 0;

    /** Attempt to resolve a finder ParamID to a JUCE parameter index by
        nudging the value and detecting which JUCE parameter changes.
        Returns -1 if no mapping could be established. */
    virtual int resolveFinderParamByWiggle (unsigned int /*finderParamId*/) { return -1; }

    /** Reverse wiggle: nudge each JUCE parameter and check if the finder
        param's controller value changes. Useful when finder ParamIDs are
        outside the controller's parameter space (e.g. kiloHearts plugins).
        Populates outMapping with finderParamId → juceIndex pairs.
        Returns the number of successful mappings. */
    virtual int resolveByReverseWiggle (
        const std::vector<unsigned int>& /*finderParamIds*/,
        std::map<unsigned int, int>& /*outMapping*/) { return 0; }

    /** Enable performEdit snooping. While active, the host context records
        the ParamID from each performEdit callback. */
    virtual void beginEditSnoop() {}

    /** Disable snooping and return the last captured ParamID, or 0xFFFFFFFF
        if no performEdit was received. */
    virtual unsigned int endEditSnoop() { return 0xFFFFFFFF; }

    /** Resolve a captured performEdit ParamID to a JUCE parameter index.
        Returns -1 if the ParamID is not in the JUCE parameter map. */
    virtual int resolveParamIDToIndex (unsigned int /*paramId*/) { return -1; }
};

} // namespace dc

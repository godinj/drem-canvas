#pragma once

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
};

} // namespace dc

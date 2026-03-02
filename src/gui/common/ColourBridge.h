#pragma once

// Bridge: convert dc::Colour to the colour type used by juce::Graphics.
// This file is deleted with the entire gui/ directory in Phase 5.

#include <JuceHeader.h>
#include "dc/foundation/types.h"

namespace dc { namespace bridge {

inline auto toJuce (dc::Colour c)  { return juce::Colour (c.argb); } // juce::Graphics bridge
inline auto toJuce (uint32_t argb) { return juce::Colour (argb); }  // juce::Graphics bridge

}} // namespace dc::bridge

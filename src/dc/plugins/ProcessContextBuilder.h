#pragma once
#include <pluginterfaces/vst/ivstprocesscontext.h>

namespace dc { class TransportController; }

namespace dc
{

struct ProcessContextBuilder
{
    // Populate ctx from transport state. Called per-block from PluginInstance::process().
    // Must be lock-free (reads atomics only).
    static void populate (Steinberg::Vst::ProcessContext& ctx,
                          const TransportController& transport,
                          int numSamples);
};

} // namespace dc

#include "ProcessContextBuilder.h"
#include "engine/TransportController.h"
#include <cmath>

namespace dc
{

void ProcessContextBuilder::populate (Steinberg::Vst::ProcessContext& ctx,
                                      const TransportController& transport,
                                      int /*numSamples*/)
{
    using namespace Steinberg::Vst;

    // Zero-initialise all fields
    ctx = {};

    // State flags — always valid
    ctx.state = ProcessContext::kTempoValid
              | ProcessContext::kTimeSigValid
              | ProcessContext::kProjectTimeMusicValid
              | ProcessContext::kBarPositionValid;

    if (transport.isPlaying())
        ctx.state |= ProcessContext::kPlaying;

    // Sample rate & tempo
    ctx.sampleRate = transport.getSampleRate();
    ctx.tempo      = transport.getTempo();

    // Time signature
    ctx.timeSigNumerator   = transport.getTimeSigNumerator();
    ctx.timeSigDenominator = transport.getTimeSigDenominator();

    // Position in samples
    ctx.projectTimeSamples = transport.getPositionInSamples();

    // PPQ position (quarter notes since start)
    double sr = ctx.sampleRate;
    double ppq = 0.0;

    if (sr > 0.0)
        ppq = (static_cast<double> (ctx.projectTimeSamples) / sr) * (ctx.tempo / 60.0);

    ctx.projectTimeMusic = ppq;

    // Bar position: quantise PPQ to bar boundaries
    double beatsPerBar = 4.0 * ctx.timeSigNumerator / ctx.timeSigDenominator;
    ctx.barPositionMusic = std::floor (ppq / beatsPerBar) * beatsPerBar;

    // System time — not required for correctness
    ctx.systemTime = 0;

    // Cycle (loop) markers
    if (transport.isLooping())
    {
        ctx.state |= ProcessContext::kCycleActive;
        double loopSr = ctx.sampleRate;
        if (loopSr > 0.0)
        {
            ctx.cycleStartMusic = (static_cast<double> (transport.getLoopStartInSamples()) / loopSr) * (ctx.tempo / 60.0);
            ctx.cycleEndMusic   = (static_cast<double> (transport.getLoopEndInSamples()) / loopSr) * (ctx.tempo / 60.0);
        }
    }
    else
    {
        ctx.cycleStartMusic = 0.0;
        ctx.cycleEndMusic   = 0.0;
    }
}

} // namespace dc

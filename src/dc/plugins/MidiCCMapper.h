#pragma once

#include "dc/plugins/ParameterChangeQueue.h"
#include <pluginterfaces/vst/ivstmidicontrollers.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <array>

namespace dc
{

class MidiMessage;

class MidiCCMapper
{
public:
    MidiCCMapper();

    // Query IMidiMapping from controller and cache all CC->ParamID mappings.
    // Call once at plugin load time (message thread). Safe if controller is null
    // or does not implement IMidiMapping.
    void buildFromController (Steinberg::Vst::IEditController* controller);

    // Translate a MIDI CC or pitch bend message to a parameter change.
    // Called from audio thread -- must be lock-free.
    // Returns true if a mapping was found and a point was added.
    bool translateToParameterChanges (const MidiMessage& msg,
                                      int sampleOffset,
                                      ParameterChangeQueue& queue) const;

    // Check if any mappings were found
    bool hasMappings() const { return hasMappings_; }

    // For testing: manually add a CC mapping
    void addMapping (int ccNumber, Steinberg::Vst::ParamID paramId);

private:
    // 128 standard CCs + kAfterTouch(128) + kPitchBend(129) + kCountCtrlNumber(130)
    static constexpr int kNumControllers = 131;
    static constexpr Steinberg::Vst::ParamID kUnmapped = 0xFFFFFFFF;

    std::array<Steinberg::Vst::ParamID, kNumControllers> ccToParam_;
    bool hasMappings_ = false;
};

} // namespace dc

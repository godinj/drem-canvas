#include "dc/plugins/MidiCCMapper.h"
#include "dc/midi/MidiMessage.h"

namespace dc
{

MidiCCMapper::MidiCCMapper()
{
    ccToParam_.fill (kUnmapped);
}

void MidiCCMapper::buildFromController (Steinberg::Vst::IEditController* controller)
{
    if (controller == nullptr)
        return;

    Steinberg::Vst::IMidiMapping* midiMapping = nullptr;
    if (controller->queryInterface (Steinberg::Vst::IMidiMapping::iid, reinterpret_cast<void**> (&midiMapping)) != Steinberg::kResultOk)
        return;

    // Iterate CCs 0-127 plus kAfterTouch (128) and kPitchBend (129)
    for (int cc = 0; cc < kNumControllers; ++cc)
    {
        Steinberg::Vst::ParamID paramId = 0;
        auto result = midiMapping->getMidiControllerAssignment (
            0, 0,
            static_cast<Steinberg::Vst::CtrlNumber> (cc),
            paramId);

        if (result == Steinberg::kResultOk)
        {
            ccToParam_[static_cast<size_t> (cc)] = paramId;
            hasMappings_ = true;
        }
    }

    midiMapping->release();
}

void MidiCCMapper::addMapping (int ccNumber, Steinberg::Vst::ParamID paramId)
{
    if (ccNumber >= 0 && ccNumber < kNumControllers)
    {
        ccToParam_[static_cast<size_t> (ccNumber)] = paramId;
        hasMappings_ = true;
    }
}

bool MidiCCMapper::translateToParameterChanges (const MidiMessage& msg,
                                                 int sampleOffset,
                                                 ParameterChangeQueue& queue) const
{
    Steinberg::Vst::ParamID paramId = kUnmapped;
    double normalizedValue = 0.0;

    if (msg.isController())
    {
        int ccNum = msg.getControllerNumber();
        if (ccNum < 0 || ccNum >= kNumControllers)
            return false;

        paramId = ccToParam_[static_cast<size_t> (ccNum)];
        if (paramId == kUnmapped)
            return false;

        normalizedValue = static_cast<double> (msg.getControllerValue()) / 127.0;
    }
    else if (msg.isPitchWheel())
    {
        // kPitchBend = 129
        paramId = ccToParam_[129];
        if (paramId == kUnmapped)
            return false;

        normalizedValue = static_cast<double> (msg.getPitchWheelValue()) / 16383.0;
    }
    else if (msg.isChannelPressure())
    {
        // kAfterTouch = 128
        paramId = ccToParam_[128];
        if (paramId == kUnmapped)
            return false;

        // Channel pressure value is in data byte 1 (accessed via getRawData)
        normalizedValue = static_cast<double> (msg.getRawData()[1]) / 127.0;
    }
    else
    {
        return false;
    }

    // Add the parameter change to the queue
    Steinberg::int32 paramIndex = 0;
    auto* paramQueue = queue.addParameterData (paramId, paramIndex);
    if (paramQueue == nullptr)
        return false;

    Steinberg::int32 pointIndex = 0;
    paramQueue->addPoint (static_cast<Steinberg::int32> (sampleOffset), normalizedValue, pointIndex);
    return true;
}

} // namespace dc

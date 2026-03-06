#pragma once

#include <pluginterfaces/vst/ivstparameterchanges.h>
#include <array>

namespace dc
{

class ParamValueQueue : public Steinberg::Vst::IParamValueQueue
{
public:
    ParamValueQueue();

    void setParameterId (Steinberg::Vst::ParamID id);
    void clear();

    // IParamValueQueue
    Steinberg::Vst::ParamID PLUGIN_API getParameterId() override;
    Steinberg::int32 PLUGIN_API getPointCount() override;
    Steinberg::tresult PLUGIN_API getPoint (Steinberg::int32 index,
        Steinberg::int32& sampleOffset,
        Steinberg::Vst::ParamValue& value) override;
    Steinberg::tresult PLUGIN_API addPoint (Steinberg::int32 sampleOffset,
        Steinberg::Vst::ParamValue value,
        Steinberg::int32& index) override;

    // FUnknown (minimal -- no ref counting needed for stack/member objects)
    Steinberg::tresult PLUGIN_API queryInterface (const Steinberg::TUID, void**) override
    { return Steinberg::kNoInterface; }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }

private:
    static constexpr int kMaxPoints = 16;
    Steinberg::Vst::ParamID paramId_ = 0;
    int pointCount_ = 0;
    struct Point { Steinberg::int32 sampleOffset; Steinberg::Vst::ParamValue value; };
    std::array<Point, kMaxPoints> points_ {};
};

class ParameterChangeQueue : public Steinberg::Vst::IParameterChanges
{
public:
    ParameterChangeQueue();

    void clear();

    // IParameterChanges
    Steinberg::int32 PLUGIN_API getParameterCount() override;
    Steinberg::Vst::IParamValueQueue* PLUGIN_API getParameterData (Steinberg::int32 index) override;
    Steinberg::Vst::IParamValueQueue* PLUGIN_API addParameterData (
        const Steinberg::Vst::ParamID& id,
        Steinberg::int32& index) override;

    // FUnknown (minimal)
    Steinberg::tresult PLUGIN_API queryInterface (const Steinberg::TUID, void**) override
    { return Steinberg::kNoInterface; }
    Steinberg::uint32 PLUGIN_API addRef() override { return 1; }
    Steinberg::uint32 PLUGIN_API release() override { return 1; }

private:
    static constexpr int kMaxParams = 128;
    int paramCount_ = 0;
    std::array<ParamValueQueue, kMaxParams> queues_;
};

} // namespace dc

#include "dc/plugins/ParameterChangeQueue.h"

namespace dc
{

// ─── ParamValueQueue ─────────────────────────────────────────────────────────

ParamValueQueue::ParamValueQueue()
    : paramId_ (0)
    , pointCount_ (0)
{
}

void ParamValueQueue::setParameterId (Steinberg::Vst::ParamID id)
{
    paramId_ = id;
}

void ParamValueQueue::clear()
{
    pointCount_ = 0;
}

Steinberg::Vst::ParamID PLUGIN_API ParamValueQueue::getParameterId()
{
    return paramId_;
}

Steinberg::int32 PLUGIN_API ParamValueQueue::getPointCount()
{
    return static_cast<Steinberg::int32> (pointCount_);
}

Steinberg::tresult PLUGIN_API ParamValueQueue::getPoint (Steinberg::int32 index,
    Steinberg::int32& sampleOffset,
    Steinberg::Vst::ParamValue& value)
{
    if (index < 0 || index >= pointCount_)
        return Steinberg::kResultFalse;

    sampleOffset = points_[static_cast<size_t> (index)].sampleOffset;
    value = points_[static_cast<size_t> (index)].value;
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API ParamValueQueue::addPoint (Steinberg::int32 sampleOffset,
    Steinberg::Vst::ParamValue value,
    Steinberg::int32& index)
{
    if (pointCount_ >= kMaxPoints)
        return Steinberg::kResultFalse;

    auto idx = static_cast<size_t> (pointCount_);
    points_[idx].sampleOffset = sampleOffset;
    points_[idx].value = value;
    index = static_cast<Steinberg::int32> (pointCount_);
    ++pointCount_;
    return Steinberg::kResultOk;
}

// ─── ParameterChangeQueue ────────────────────────────────────────────────────

ParameterChangeQueue::ParameterChangeQueue()
    : paramCount_ (0)
{
}

void ParameterChangeQueue::clear()
{
    for (int i = 0; i < paramCount_; ++i)
        queues_[static_cast<size_t> (i)].clear();

    paramCount_ = 0;
}

Steinberg::int32 PLUGIN_API ParameterChangeQueue::getParameterCount()
{
    return static_cast<Steinberg::int32> (paramCount_);
}

Steinberg::Vst::IParamValueQueue* PLUGIN_API ParameterChangeQueue::getParameterData (Steinberg::int32 index)
{
    if (index < 0 || index >= paramCount_)
        return nullptr;

    return &queues_[static_cast<size_t> (index)];
}

Steinberg::Vst::IParamValueQueue* PLUGIN_API ParameterChangeQueue::addParameterData (
    const Steinberg::Vst::ParamID& id,
    Steinberg::int32& index)
{
    // Scan existing queues for a matching paramId
    for (int i = 0; i < paramCount_; ++i)
    {
        if (queues_[static_cast<size_t> (i)].getParameterId() == id)
        {
            index = static_cast<Steinberg::int32> (i);
            return &queues_[static_cast<size_t> (i)];
        }
    }

    // Allocate a new slot
    if (paramCount_ >= kMaxParams)
        return nullptr;

    auto idx = static_cast<size_t> (paramCount_);
    queues_[idx].clear();
    queues_[idx].setParameterId (id);
    index = static_cast<Steinberg::int32> (paramCount_);
    ++paramCount_;
    return &queues_[idx];
}

} // namespace dc

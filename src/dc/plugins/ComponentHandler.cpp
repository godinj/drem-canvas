#include "dc/plugins/ComponentHandler.h"
#include "dc/foundation/assert.h"

#include <pluginterfaces/base/funknown.h>

namespace dc {

ComponentHandler::ComponentHandler (SPSCQueue<EditEvent>& editQueue)
    : editQueue_ (editQueue)
{
}

// --- IComponentHandler ---

Steinberg::tresult PLUGIN_API ComponentHandler::beginEdit (Steinberg::Vst::ParamID /*id*/)
{
    // No-op for now — can add undo transaction grouping later.
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API ComponentHandler::performEdit (
    Steinberg::Vst::ParamID id,
    Steinberg::Vst::ParamValue valueNormalized)
{
    // Push the edit event to the SPSC queue for parameter finder snoop.
    editQueue_.push (EditEvent { id, valueNormalized });

    // Notify callback (for automation recording).
    if (paramCallback_)
        paramCallback_ (id, valueNormalized);

    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API ComponentHandler::endEdit (Steinberg::Vst::ParamID /*id*/)
{
    // No-op for now — can add undo transaction grouping later.
    return Steinberg::kResultOk;
}

Steinberg::tresult PLUGIN_API ComponentHandler::restartComponent (Steinberg::int32 flags)
{
    if (restartCallback_)
        restartCallback_ (flags);

    return Steinberg::kResultOk;
}

// --- FUnknown ---

Steinberg::tresult PLUGIN_API ComponentHandler::queryInterface (
    const Steinberg::TUID iid, void** obj)
{
    if (Steinberg::FUnknownPrivate::iidEqual (iid,
            Steinberg::Vst::IComponentHandler::iid))
    {
        addRef();
        *obj = static_cast<Steinberg::Vst::IComponentHandler*> (this);
        return Steinberg::kResultOk;
    }

    if (Steinberg::FUnknownPrivate::iidEqual (iid, Steinberg::FUnknown::iid))
    {
        addRef();
        *obj = static_cast<Steinberg::FUnknown*> (this);
        return Steinberg::kResultOk;
    }

    *obj = nullptr;
    return Steinberg::kNoInterface;
}

Steinberg::uint32 PLUGIN_API ComponentHandler::addRef()
{
    return ++refCount_;
}

Steinberg::uint32 PLUGIN_API ComponentHandler::release()
{
    auto count = --refCount_;
    // Do NOT delete this on release — the handler is owned by PluginInstance.
    // This prevents double-free when the plugin holds a reference.
    return count;
}

// --- Extensions ---

void ComponentHandler::setParameterChangeCallback (
    std::function<void (Steinberg::Vst::ParamID, double)> cb)
{
    paramCallback_ = std::move (cb);
}

void ComponentHandler::setRestartCallback (
    std::function<void (Steinberg::int32)> cb)
{
    restartCallback_ = std::move (cb);
}

} // namespace dc

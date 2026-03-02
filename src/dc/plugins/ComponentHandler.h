#pragma once
#include "dc/foundation/spsc_queue.h"
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <functional>
#include <atomic>

namespace dc {

struct EditEvent
{
    Steinberg::Vst::ParamID paramId;
    double value;
};

class ComponentHandler
    : public Steinberg::Vst::IComponentHandler
{
public:
    explicit ComponentHandler (SPSCQueue<EditEvent>& editQueue);

    // --- IComponentHandler ---
    Steinberg::tresult PLUGIN_API beginEdit (
        Steinberg::Vst::ParamID id) override;
    Steinberg::tresult PLUGIN_API performEdit (
        Steinberg::Vst::ParamID id,
        Steinberg::Vst::ParamValue valueNormalized) override;
    Steinberg::tresult PLUGIN_API endEdit (
        Steinberg::Vst::ParamID id) override;
    Steinberg::tresult PLUGIN_API restartComponent (
        Steinberg::int32 flags) override;

    // --- FUnknown ---
    Steinberg::tresult PLUGIN_API queryInterface (
        const Steinberg::TUID iid, void** obj) override;
    Steinberg::uint32 PLUGIN_API addRef() override;
    Steinberg::uint32 PLUGIN_API release() override;

    // --- Extensions ---

    /// Callback for parameter changes (for automation recording).
    /// Called from the plugin's UI thread during performEdit.
    void setParameterChangeCallback (
        std::function<void (Steinberg::Vst::ParamID, double)> cb);

    /// Callback for restartComponent (latency change, etc.)
    void setRestartCallback (
        std::function<void (Steinberg::int32)> cb);

private:
    SPSCQueue<EditEvent>& editQueue_;
    std::function<void (Steinberg::Vst::ParamID, double)> paramCallback_;
    std::function<void (Steinberg::int32)> restartCallback_;
    std::atomic<Steinberg::uint32> refCount_ {1};
};

} // namespace dc

#include "dc/plugins/PluginEditor.h"
#include "dc/plugins/PluginInstance.h"
#include "dc/foundation/assert.h"

#include <pluginterfaces/gui/iplugview.h>
#include <pluginterfaces/vst/ivstplugview.h>
#include <pluginterfaces/base/funknown.h>
#include <atomic>

namespace dc {

// ─── PlugFrame — IPlugFrame implementation ───────────────────────────────

class PluginEditor::PlugFrame : public Steinberg::IPlugFrame
{
public:
    PlugFrame() = default;

    Steinberg::tresult PLUGIN_API resizeView (
        Steinberg::IPlugView* view,
        Steinberg::ViewRect* newSize) override
    {
        if (! view || ! newSize)
            return Steinberg::kInvalidArgument;

        int w = newSize->right - newSize->left;
        int h = newSize->bottom - newSize->top;

        // Apply the resize to the view
        auto result = view->onSize (newSize);

        // Notify host of the resize
        if (resizeCallback_)
            resizeCallback_ (w, h);

        return result;
    }

    // --- FUnknown ---

    Steinberg::tresult PLUGIN_API queryInterface (
        const Steinberg::TUID iid, void** obj) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual (iid,
            Steinberg::IPlugFrame::iid))
        {
            addRef();
            *obj = static_cast<IPlugFrame*> (this);
            return Steinberg::kResultOk;
        }

        if (Steinberg::FUnknownPrivate::iidEqual (iid,
            Steinberg::FUnknown::iid))
        {
            addRef();
            *obj = static_cast<Steinberg::FUnknown*> (
                static_cast<IPlugFrame*> (this));
            return Steinberg::kResultOk;
        }

        *obj = nullptr;
        return Steinberg::kNoInterface;
    }

    // Ref-counting for COM compliance. The PlugFrame is owned by PluginEditor
    // via unique_ptr — do NOT delete via release(). The ref count is maintained
    // for protocol compliance but lifetime is managed by the owning PluginEditor.
    Steinberg::uint32 PLUGIN_API addRef() override { return ++refCount_; }
    Steinberg::uint32 PLUGIN_API release() override { return --refCount_; }

    void setResizeCallback (std::function<void (int, int)> cb)
    {
        resizeCallback_ = std::move (cb);
    }

private:
    std::function<void (int, int)> resizeCallback_;
    std::atomic<Steinberg::uint32> refCount_ {1};
};

// ─── PluginEditor static factory ─────────────────────────────────────────

std::unique_ptr<PluginEditor> PluginEditor::create (PluginInstance& instance)
{
    auto* controller = instance.getController();

    if (controller == nullptr)
    {
        dc_log ("PluginEditor::create: controller is null");
        return nullptr;
    }

    auto* view = controller->createView ("editor");

    if (view == nullptr)
    {
        dc_log ("PluginEditor::create: plugin has no editor view");
        return nullptr;
    }

    auto editor = std::unique_ptr<PluginEditor> (new PluginEditor (view, instance));

    // Create the PlugFrame and set it on the view
    editor->frame_ = std::make_unique<PlugFrame>();
    view->setFrame (editor->frame_.get());

    // Query IParameterFinder from the view (standard location per VST3 spec)
    Steinberg::Vst::IParameterFinder* viewFinder = nullptr;
    view->queryInterface (Steinberg::Vst::IParameterFinder::iid,
                          reinterpret_cast<void**> (&viewFinder));
    if (viewFinder != nullptr)
    {
        editor->viewFinder_ = viewFinder;
        instance.setViewParameterFinder (viewFinder);
    }

    return editor;
}

// ─── Constructor / Destructor ────────────────────────────────────────────

PluginEditor::PluginEditor (Steinberg::IPlugView* view, PluginInstance& instance)
    : view_ (view)
    , instance_ (instance)
{
}

PluginEditor::~PluginEditor()
{
    if (attached_)
        detach();

    // Unregister the view-based parameter finder before releasing the view
    if (viewFinder_ != nullptr)
    {
        instance_.setViewParameterFinder (nullptr);
        viewFinder_->release();
        viewFinder_ = nullptr;
    }

    if (view_ != nullptr)
    {
        view_->setFrame (nullptr);
        view_->release();
        view_ = nullptr;
    }
}

// ─── Preferred size ──────────────────────────────────────────────────────

std::pair<int, int> PluginEditor::getPreferredSize() const
{
    if (view_ == nullptr)
        return { 0, 0 };

    Steinberg::ViewRect rect {};

    if (view_->getSize (&rect) == Steinberg::kResultOk)
        return { rect.right - rect.left, rect.bottom - rect.top };

    return { 0, 0 };
}

// ─── Window attachment ───────────────────────────────────────────────────

void PluginEditor::attachToWindow (void* nativeHandle)
{
    if (view_ == nullptr || attached_)
        return;

    Steinberg::FIDString platformType = nullptr;

#ifdef __APPLE__
    platformType = Steinberg::kPlatformTypeNSView;
#elif defined(__linux__)
    platformType = Steinberg::kPlatformTypeX11EmbedWindowID;
#else
    dc_log ("PluginEditor::attachToWindow: unsupported platform");
    return;
#endif

    auto result = view_->attached (nativeHandle, platformType);

    if (result == Steinberg::kResultOk)
    {
        attached_ = true;
    }
    else
    {
        dc_log ("PluginEditor::attachToWindow: view->attached() failed");
    }
}

// ─── Detach ──────────────────────────────────────────────────────────────

void PluginEditor::detach()
{
    if (! attached_ || view_ == nullptr)
        return;

    view_->removed();
    attached_ = false;
}

// ─── Resize ──────────────────────────────────────────────────────────────

void PluginEditor::setSize (int width, int height)
{
    if (view_ == nullptr)
        return;

    Steinberg::ViewRect rect { 0, 0, static_cast<Steinberg::int32> (width),
                                      static_cast<Steinberg::int32> (height) };
    view_->onSize (&rect);
}

// ─── Query state ─────────────────────────────────────────────────────────

bool PluginEditor::isAttached() const
{
    return attached_;
}

Steinberg::IPlugView* PluginEditor::getPlugView() const
{
    return view_;
}

PluginInstance& PluginEditor::getInstance() const
{
    return instance_;
}

// ─── IParameterFinder delegation ─────────────────────────────────────────

int PluginEditor::findParameterAtPoint (int x, int y) const
{
    return instance_.findParameterAtPoint (x, y);
}

// ─── Resize callback ────────────────────────────────────────────────────

void PluginEditor::setResizeCallback (std::function<void (int, int)> cb)
{
    if (frame_)
        frame_->setResizeCallback (std::move (cb));
}

} // namespace dc

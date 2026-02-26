#pragma once

#include "graphics/core/Widget.h"
#include "graphics/core/Event.h"
#include "graphics/widgets/ButtonWidget.h"
#include "graphics/widgets/LabelWidget.h"
#include "engine/TransportController.h"
#include "model/TempoMap.h"

namespace dc
{
namespace ui
{

class TransportBarWidget : public gfx::Widget
{
public:
    TransportBarWidget (TransportController& transport, TempoMap& tempoMap);

    void paint (gfx::Canvas& canvas) override;
    void resized() override;
    void animationTick (double timestampMs) override;
    void mouseWheel (const gfx::WheelEvent& e) override;

    // Callbacks for session/utility actions (wired by AppController)
    std::function<void()> onSaveSession;
    std::function<void()> onLoadSession;
    std::function<void()> onImportAudio;
    std::function<void()> onAudioSettings;
    std::function<void()> onToggleBrowser;

private:
    void updateTempoDisplay();

    TransportController& transportController;
    TempoMap& tempoMap;

    gfx::ButtonWidget playButton;
    gfx::ButtonWidget stopButton;
    gfx::LabelWidget timeDisplay;
    gfx::LabelWidget tempoDisplay;

    gfx::ButtonWidget saveButton;
    gfx::ButtonWidget loadButton;
    gfx::ButtonWidget importButton;
    gfx::ButtonWidget audioSettingsButton;
    gfx::ButtonWidget pluginsButton;
};

} // namespace ui
} // namespace dc

#pragma once

#include "graphics/core/Widget.h"
#include "graphics/core/EventDispatch.h"
#include "graphics/rendering/Renderer.h"
#include "graphics/theme/Theme.h"
#include "engine/AudioEngine.h"
#include "engine/TransportController.h"
#include "engine/MixBusProcessor.h"
#include "engine/TrackProcessor.h"
#include "engine/StepSequencerProcessor.h"
#include "engine/MidiClipProcessor.h"
#include "engine/MeterTapProcessor.h"
#include "engine/MidiEngine.h"
#include "engine/SimpleSynthProcessor.h"
#include "model/Project.h"
#include "model/Arrangement.h"
#include "model/TempoMap.h"
#include "model/GridSystem.h"
#include "model/StepSequencer.h"
#include "vim/VimEngine.h"
#include "vim/VimContext.h"
#include "plugins/PluginManager.h"
#include "plugins/PluginHost.h"
#include "plugins/PluginWindowManager.h"
#include "ui/transport/TransportBarWidget.h"
#include "ui/vim/VimStatusBarWidget.h"
#include "ui/arrangement/ArrangementWidget.h"
#include "ui/mixer/MixerWidget.h"
#include "ui/sequencer/StepSequencerWidget.h"
#include "ui/midieditor/PianoRollWidget.h"
#include "ui/browser/BrowserWidget.h"
#include "ui/palette/CommandPaletteWidget.h"
#include "vim/ActionRegistry.h"
#include "ui/keyboard/VirtualKeyboardWidget.h"
#include "ui/pluginview/PluginViewWidget.h"
#include "model/RecentProjects.h"
#include "dc/foundation/message_queue.h"
#include <filesystem>
#include <vector>

namespace dc
{
namespace ui
{

class AppController : public gfx::Widget,
                      private VimEngine::Listener
{
public:
    AppController();
    ~AppController() override;

    void paint (gfx::Canvas& canvas) override;
    void paintOverChildren (gfx::Canvas& canvas) override;
    void resized() override;
    bool keyDown (const gfx::KeyEvent& e) override;
    bool keyUp (const gfx::KeyEvent& e) override;

    // Initialize the audio engine and all UI
    void initialise();

    // Called by the platform render loop to push meter levels and process messages
    void tick();

    // Access for wiring
    gfx::Renderer* getRenderer() { return renderer; }
    void setRenderer (gfx::Renderer* r);

    void setNativeWindowHandle (void* handle);

    // Project access (used by E2E validation in Main.cpp)
    Project& getProject() { return project; }
    const Project& getProject() const { return project; }

    // Session loading (public for E2E --load flag)
    void loadSessionFromDirectory (const std::filesystem::path& dir);

    // Plugin view access (used by E2E --scan-plugin flag)
    PluginViewWidget* getPluginViewWidget() { return pluginViewWidget.get(); }

    // Open plugin editor (used by E2E --scan-plugin flag)
    void openPluginEditor (int trackIndex, int pluginIndex);

    // Plugin manager access (used by E2E --browser-scan flag)
    PluginManager& getPluginManager() { return pluginManager; }

    // Browser toggle (used by E2E --browser-scan flag)
    void toggleBrowser();

    // Plugin chain info (used by E2E --capture-plugin-state flag)
    struct PluginNodeInfo
    {
        NodeId node = 0;
        dc::PluginInstance* plugin = nullptr;
    };

    /// Returns plugin chain info for the specified track (e2e test support).
    const std::vector<PluginNodeInfo>& getTrackPluginChain (int trackIndex) const;

private:
    // PropertyTree::Listener
    void propertyChanged (PropertyTree&, PropertyId) override;
    void childAdded (PropertyTree&, PropertyTree&) override;
    void childRemoved (PropertyTree&, PropertyTree&, int) override;

    // VimEngine::Listener
    void vimModeChanged (VimEngine::Mode newMode) override;
    void vimContextChanged() override;

    // Audio graph management
    void rebuildAudioGraph();
    void syncTrackProcessorsFromModel();
    void syncSequencerFromModel();
    void syncMidiClipFromModel (int trackIndex);

    void connectTrackPluginChain (int trackIndex);
    void disconnectTrackPluginChain (int trackIndex);
    void captureAllPluginStates();
    void insertPluginOnTrack (int trackIndex, const dc::PluginDescription& desc);

    // Session management
    void saveSession();
    void loadSession();
    void openFile();
    void addTrackFromFile (const std::filesystem::path& file);
    void addMidiTrack (const std::string& name);
    void showAudioSettings();

    // Panel visibility
    void updatePanelVisibility();

    // Command palette
    void registerAllActions();
    void showCommandPalette();
    void dismissCommandPalette();
    void refreshRecentProjectActions();

    // ─── Engine ──────────────────────────────────────────
    AudioEngine audioEngine;
    TransportController transportController;
    NodeId mixBusNode = 0;
    MixBusProcessor* mixBusProcessor = nullptr;   // non-owning; graph owns
    std::vector<TrackProcessor*> trackProcessors;
    std::vector<MidiClipProcessor*> midiClipProcessors;
    std::vector<NodeId> trackNodes;
    std::vector<std::vector<PluginNodeInfo>> trackPluginChains;
    std::vector<MeterTapProcessor*> meterTapProcessors;
    std::vector<NodeId> meterTapNodes;
    std::vector<NodeId> fallbackSynthNodes;
    StepSequencerProcessor* sequencerProcessor = nullptr;
    NodeId sequencerNode = 0;
    dc::MessageQueue messageQueue;
    MidiEngine midiEngine { messageQueue };

    // ─── Plugin infrastructure ───────────────────────────
    PluginManager pluginManager { messageQueue };
    PluginHost pluginHost { pluginManager };
    PluginWindowManager pluginWindowManager;

    // ─── Model ───────────────────────────────────────────
    Project project;
    Arrangement arrangement { project };
    TempoMap tempoMap;
    GridSystem gridSystem { tempoMap };
    VimContext vimContext;
    std::unique_ptr<VimEngine> vimEngine;
    ActionRegistry actionRegistry;
    RecentProjects recentProjects;

    std::filesystem::path currentSessionDirectory;

    // ─── UI widgets ──────────────────────────────────────
    std::unique_ptr<TransportBarWidget> transportBar;
    std::unique_ptr<VimStatusBarWidget> vimStatusBar;
    std::unique_ptr<ArrangementWidget> arrangementWidget;
    std::unique_ptr<MixerWidget> mixerWidget;
    std::unique_ptr<StepSequencerWidget> sequencerWidget;
    std::unique_ptr<PianoRollWidget> pianoRollWidget;
    std::unique_ptr<BrowserWidget> browserWidget;
    std::unique_ptr<CommandPaletteWidget> commandPalette;
    std::unique_ptr<VirtualKeyboardWidget> keyboardWidget;
    std::unique_ptr<PluginViewWidget> pluginViewWidget;

    bool browserVisible = false;

    // Resizer bar position: fraction of center area occupied by top (arrangement)
    float splitRatio = 0.65f;

    void* nativeWindowHandle = nullptr;

    gfx::Renderer* renderer = nullptr;
};

} // namespace ui
} // namespace dc

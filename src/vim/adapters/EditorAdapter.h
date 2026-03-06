#pragma once
#include "vim/ContextAdapter.h"
#include "dc/foundation/listener_list.h"
#include <functional>
#include <vector>

namespace dc
{

class Project;
class Arrangement;
class Track;
class TransportController;
class GridSystem;
class VimEngine;

class EditorAdapter : public ContextAdapter
{
public:
    EditorAdapter (Project& project, Arrangement& arrangement,
                   TransportController& transport, GridSystem& gridSystem,
                   VimContext& context);

    VimContext::Panel getPanel() const override { return VimContext::Editor; }

    // Motion resolution
    MotionRange resolveMotion (char32_t motionKey, int count) const override;
    MotionRange resolveLinewiseMotion (int count) const override;

    // Motion execution
    void executeMotion (char32_t motionKey, int count) override;

    // Operator execution
    void executeOperator (VimGrammar::Operator op,
                          const MotionRange& range, char reg) override;

    std::string getSupportedMotions() const override { return "hjkl0$GgwbeW"; }
    std::string getSupportedOperators() const override { return "dyc"; }

    // Register editor actions with the action registry
    void registerActions (ActionRegistry& registry) override;

    // Visual mode
    void enterVisualMode() override;
    void enterVisualLineMode() override;
    void exitVisualMode() override;
    void updateVisualSelection() override;
    bool handleVisualKey (const dc::KeyPress& key) override;
    bool handleVisualLineKey (const dc::KeyPress& key) override;

    // --- Public action methods (moved from VimEngine) ---
    // Navigation
    void moveSelectionUp();
    void moveSelectionDown();
    void moveSelectionLeft();
    void moveSelectionRight();
    void jumpToFirstTrack();
    void jumpToLastTrack();

    // Transport
    void jumpToSessionStart();
    void jumpToSessionEnd();
    void togglePlayStop();

    // Clip operations
    void deleteSelectedRegions (char reg = '\0');
    void yankSelectedRegions (char reg = '\0');
    void pasteAfterPlayhead (char reg = '\0');
    void pasteBeforePlayhead (char reg = '\0');
    void splitRegionAtPlayhead();
    void duplicateSelectedClip();

    // Track state
    void toggleMute();
    void toggleSolo();
    void toggleRecordArm();

    // Grid
    void adjustGridDivision (int delta);

    // Listener proxy -- adapter needs to notify VimEngine listeners
    using ContextChangedCallback = std::function<void()>;
    using ModeChangedCallback = std::function<void (int newMode)>;
    void setContextChangedCallback (ContextChangedCallback cb) { onContextChanged = cb; }
    void setModeChangedCallback (ModeChangedCallback cb) { onModeChanged = cb; }

    // Callbacks that reach up to AppController (moved from VimEngine)
    std::function<void (const PropertyTree&)> onOpenPianoRoll;

    // Cycle support
    using ToggleCycleCallback = std::function<void()>;
    using SetCycleCallback = std::function<void()>;
    void setToggleCycleCallback (ToggleCycleCallback cb) { onToggleCycle = cb; }
    void setCycleToGridVisualCallback (SetCycleCallback cb) { onSetCycleToGridVisual = cb; }

    // Open focused item (Enter key on editor clips)
    void openFocusedItem();

    // Grid cursor clip index update (public for undo/redo refresh)
    void updateClipIndexFromGridCursor();

private:
    // Clip collection
    struct ClipEntry { PropertyTree data; int trackOffset; int64_t timeOffset; };
    std::vector<ClipEntry> collectClipsForRange (const MotionRange& range) const;

    // Operator helpers
    void executeDelete (const MotionRange& range, char reg);
    void executeYank (const MotionRange& range, char reg);
    void executeChange (const MotionRange& range, char reg);

    // Visual mode helpers
    MotionRange getVisualRange() const;
    void executeVisualOperator (VimGrammar::Operator op, char reg);
    void executeGridVisualDelete (char reg);
    void executeGridVisualYank (bool isYank, char reg);
    void executeVisualMute();
    void executeVisualSolo();

    // Clip gap carving
    static void carveGap (Track& track, int64_t gapStart, int64_t gapEnd, UndoManager& um);

    Project& project;
    Arrangement& arrangement;
    TransportController& transport;
    GridSystem& gridSystem;
    VimContext& context;

    // Visual mode anchor
    int visualAnchorTrack = 0;
    int visualAnchorClip  = 0;
    int64_t visualAnchorGridPos = 0;

    ContextChangedCallback onContextChanged;
    ModeChangedCallback onModeChanged;
    ToggleCycleCallback onToggleCycle;
    SetCycleCallback onSetCycleToGridVisual;
};

} // namespace dc

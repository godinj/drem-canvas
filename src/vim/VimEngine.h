#pragma once
#include "dc/foundation/keycode.h"
#include "VimContext.h"
#include "model/Project.h"
#include "model/Arrangement.h"
#include "model/Track.h"
#include "model/StepSequencer.h"
#include "engine/TransportController.h"
#include "graphics/core/Event.h"
#include "vim/VirtualKeyboardState.h"
#include "dc/midi/MidiMessage.h"
#include "model/GridSystem.h"
#include "dc/foundation/listener_list.h"
#include <string>

namespace dc
{

class VimEngine
{
public:
    enum Mode { Normal, Insert, Command, Keyboard, PluginMenu, Visual, VisualLine };
    enum Operator { OpNone, OpDelete, OpYank, OpChange };

    struct MotionRange
    {
        int startTrack = 0;
        int startClip  = 0;
        int endTrack   = 0;
        int endClip    = 0;
        bool linewise  = false;
        bool valid     = false;
    };

    class Listener
    {
    public:
        virtual ~Listener() = default;
        virtual void vimModeChanged (Mode newMode) = 0;
        virtual void vimContextChanged() = 0;
    };

    VimEngine (Project& project, TransportController& transport,
               Arrangement& arrangement, VimContext& context,
               GridSystem& gridSystem);

    // Primary key dispatch entry point (converts gfx::KeyEvent to dc::KeyPress)
    bool handleKeyEvent (const gfx::KeyEvent& event);

    // Key-up event path (needed for Keyboard mode note-off)
    bool handleKeyUp (const gfx::KeyEvent& event);

    Mode getMode() const { return mode; }
    bool hasPendingKey() const { return pendingKey != 0; }

    // Operator-pending queries (used by status bar)
    bool isOperatorPending() const { return pendingOperator != OpNone; }
    bool hasPendingState() const;
    std::string getPendingDisplay() const;

    // Command mode
    const std::string& getCommandBuffer() const { return commandBuffer; }

    // Plugin command callback (wired by MainComponent)
    std::function<void (const std::string&)> onPluginCommand;

    // Command palette callback
    std::function<void()> onCommandPalette;

    // MIDI track creation callback
    std::function<void (const std::string&)> onCreateMidiTrack;

    // Piano roll open callback
    std::function<void (const PropertyTree&)> onOpenPianoRoll;

    // Browser toggle callback (gp keybinding)
    std::function<void()> onToggleBrowser;

    // Plugin view callbacks
    std::function<void (int trackIndex, int pluginIndex)> onOpenPluginView;
    std::function<void()> onClosePluginView;
    std::function<void()> onPluginViewRescan;
    std::function<void()> onPluginViewToggleDragAxis;
    std::function<void()> onPluginViewToggleDragCenter;
    std::function<void()> onPluginViewEndDrag;
    std::function<void (int paramIndex, float delta)> onPluginParamAdjust;
    std::function<void (int paramIndex, float newValue)> onPluginParamChanged;
    std::function<int()> onQuerySpatialHintCount;
    std::function<int (int spatialIndex)> onResolveSpatialHint;
    std::function<int()> onQueryPluginParamCount;

    // Hint label generation (totalCount determines uniform label length)
    static std::string generateHintLabel (int index, int totalCount);
    static int resolveHintLabel (const std::string& label, int totalCount);

    // Mixer plugin callbacks (wired by MainComponent)
    std::function<void (int trackIndex, int pluginIndex)> onMixerPluginOpen;
    std::function<void (int trackIndex)> onMixerPluginAdd;
    std::function<void (int trackIndex, int pluginIndex)> onMixerPluginRemove;
    std::function<void (int trackIndex, int pluginIndex)> onMixerPluginBypass;
    std::function<void (int trackIndex, int fromIndex, int toIndex)> onMixerPluginReorder;

    // Plugin menu callbacks (wired by MainComponent)
    std::function<void (int)> onPluginMenuMove;     // delta: +1 down, -1 up
    std::function<void (int)> onPluginMenuScroll;    // direction: +1 half-page down, -1 half-page up
    std::function<void()> onPluginMenuConfirm;
    std::function<void()> onPluginMenuCancel;

    // Plugin search callbacks (wired by AppController / MainComponent)
    std::function<void (const std::string&)> onPluginMenuFilter;
    std::function<void()> onPluginMenuClearFilter;

    // Plugin search accessors (used by status bars)
    bool isPluginSearchActive() const { return pluginSearchActive; }
    const std::string& getPluginSearchBuffer() const { return pluginSearchBuffer; }

    // Piano roll action callbacks (wired by AppController)
    std::function<void (int tool)> onSetPianoRollTool;     // 0=Select, 1=Draw, 2=Erase
    std::function<void (char reg)> onPianoRollDeleteSelected;
    std::function<void (char reg)> onPianoRollCopy;
    std::function<void (char reg)> onPianoRollPaste;
    std::function<void()> onPianoRollDuplicate;
    std::function<void (int)> onPianoRollTranspose;        // semitones
    std::function<void()> onPianoRollSelectAll;
    std::function<void()> onPianoRollQuantize;
    std::function<void()> onPianoRollHumanize;
    std::function<void (bool)> onPianoRollVelocityLane;    // toggle
    std::function<void (float)> onPianoRollZoom;           // factor
    std::function<void()> onPianoRollZoomToFit;
    std::function<void (int)> onPianoRollGridDiv;          // delta to grid division
    std::function<void (int, int)> onPianoRollMoveCursor;  // dBeatCol, dNoteRow
    std::function<void()> onPianoRollAddNote;
    std::function<void (int, int)> onPianoRollJumpCursor;  // absolute beatCol, noteRow (-1 = no change)

    // Live MIDI output callback (wired by AppController to selected MIDI track)
    std::function<void (const dc::MidiMessage&)> onLiveMidiNote;

    // Keyboard state (public for widget access)
    VirtualKeyboardState& getKeyboardState() { return keyboardState; }
    const VirtualKeyboardState& getKeyboardState() const { return keyboardState; }

    // Keyboard mode transitions
    void enterKeyboardMode();
    void exitKeyboardMode();

    void addListener (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

    // ─── Public action methods (for ActionRegistry) ──────────

    // Navigation
    void moveSelectionUp();
    void moveSelectionDown();
    void moveSelectionLeft();
    void moveSelectionRight();

    // Track jumps
    void jumpToFirstTrack();
    void jumpToLastTrack();

    // Transport
    void jumpToSessionStart();
    void jumpToSessionEnd();
    void togglePlayStop();

    // Clip operations
    void deleteSelectedRegions();
    void yankSelectedRegions();
    void pasteAfterPlayhead();
    void pasteBeforePlayhead();
    void splitRegionAtPlayhead();
    void duplicateSelectedClip();

    // Track state
    void toggleMute();
    void toggleSolo();
    void toggleRecordArm();

    // Mode switching
    void enterInsertMode();
    void enterNormalMode();
    void enterPluginMenuMode();
    void enterVisualMode();
    void enterVisualLineMode();
    void exitVisualMode();

    // Panel
    void cycleFocusPanel();

    // Piano roll actions
    void prMoveLeft();
    void prMoveRight();
    void prMoveUp();
    void prMoveDown();
    void prSelectNote();
    void prDeleteNote();
    void prAddNote();
    void prJumpStart();
    void prJumpEnd();

    // Sequencer actions
    void seqMoveLeft();
    void seqMoveRight();
    void seqMoveUp();
    void seqMoveDown();
    void seqJumpFirstStep();
    void seqJumpLastStep();
    void seqJumpFirstRow();
    void seqJumpLastRow();
    void seqToggleStep();
    void seqAdjustVelocity (int delta);
    void seqCycleVelocity();
    void seqToggleRowMute();
    void seqToggleRowSolo();

private:
    bool dispatch (const dc::KeyPress& key);

    bool handleNormalKey (const dc::KeyPress& key);
    bool handleInsertKey (const dc::KeyPress& key);
    bool handleCommandKey (const dc::KeyPress& key);
    bool handleKeyboardKey (const dc::KeyPress& key);
    bool handlePluginMenuKey (const dc::KeyPress& key);
    bool handlePluginSearchKey (const dc::KeyPress& key);
    bool handleVisualKey (const dc::KeyPress& key);
    bool handleVisualLineKey (const dc::KeyPress& key);
    void executeCommand();

    bool handleSequencerNormalKey (const dc::KeyPress& key);
    bool handlePianoRollNormalKey (const dc::KeyPress& key);
    bool handleMixerNormalKey (const dc::KeyPress& key);
    bool handlePluginViewNormalKey (const dc::KeyPress& key);

    void openPluginView (int trackIndex, int pluginIndex);
    void closePluginView();

    int getMixerPluginCount() const;

    void openFocusedItem();
    void closePianoRoll();

    // Pending key helpers
    void clearPending();

    // Count helpers
    bool isDigitForCount (char32_t c) const;
    void accumulateDigit (char32_t c);
    int  getEffectiveCount() const;
    void resetCounts();

    // Operator state
    void startOperator (Operator op);
    void cancelOperator();
    Operator charToOperator (char32_t c) const;

    // Motion
    bool isMotionKey (char32_t c) const;
    MotionRange resolveMotion (char32_t key, int count) const;
    MotionRange resolveLinewiseMotion (int count) const;

    // Operator execution
    void executeOperator (Operator op, const MotionRange& range);
    void executeDelete (const MotionRange& range);
    void executeYank (const MotionRange& range);
    void executeChange (const MotionRange& range);
    void executeMotion (char32_t key, int count);

    // Clip collection helpers
    std::vector<Clipboard::ClipEntry> collectClipsForRange (const MotionRange& range) const;

    // Visual mode helpers
    MotionRange getVisualRange() const;
    void updateVisualSelection();
    void executeVisualOperator (Operator op);
    void executeGridVisualDelete();
    void executeGridVisualYank (bool isYank);
    void executeVisualMute();
    void executeVisualSolo();

    void updateClipIndexFromGridCursor();

    Project& project;
    TransportController& transport;
    Arrangement& arrangement;
    VimContext& context;
    GridSystem& gridSystem;

    Mode mode = Normal;
    std::string commandBuffer;
    char32_t pendingKey = 0;
    int64_t pendingTimestamp = 0;
    static constexpr int64_t pendingTimeoutMs = 1000;

    // Operator-pending state
    Operator pendingOperator = OpNone;
    int countAccumulator = 0;   // count typed before operator (e.g. the 3 in 3d2j)
    int operatorCount = 0;      // count typed after operator  (e.g. the 2 in 3d2j)

    // Register prefix state ("x prefix)
    char pendingRegister = '\0';
    bool awaitingRegisterChar = false;
    char consumeRegister();     // returns pending register and resets

    VirtualKeyboardState keyboardState;

    // Plugin search sub-state
    bool pluginSearchActive = false;
    std::string pluginSearchBuffer;

    // Visual mode anchor
    int visualAnchorTrack = 0;
    int visualAnchorClip  = 0;
    int64_t visualAnchorGridPos = 0;

    dc::ListenerList<Listener> listeners;

    VimEngine (const VimEngine&) = delete;
    VimEngine& operator= (const VimEngine&) = delete;
};

} // namespace dc

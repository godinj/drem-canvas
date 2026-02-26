#pragma once
#include <JuceHeader.h>
#include "VimContext.h"
#include "model/Project.h"
#include "model/Arrangement.h"
#include "model/Track.h"
#include "model/StepSequencer.h"
#include "engine/TransportController.h"
#include "graphics/core/Event.h"

namespace dc
{

class VimEngine : public juce::KeyListener
{
public:
    enum Mode { Normal, Insert, Command };
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
               Arrangement& arrangement, VimContext& context);

    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;

    // New event path for graphics engine (converts to KeyPress internally)
    bool handleKeyEvent (const gfx::KeyEvent& event);

    Mode getMode() const { return mode; }
    bool hasPendingKey() const { return pendingKey != 0; }

    // Operator-pending queries (used by status bar)
    bool isOperatorPending() const { return pendingOperator != OpNone; }
    bool hasPendingState() const;
    juce::String getPendingDisplay() const;

    // Command mode
    const juce::String& getCommandBuffer() const { return commandBuffer; }

    // Plugin command callback (wired by MainComponent)
    std::function<void (const juce::String&)> onPluginCommand;

    // Command palette callback
    std::function<void()> onCommandPalette;

    // MIDI track creation callback
    std::function<void (const juce::String&)> onCreateMidiTrack;

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

    // Track state
    void toggleMute();
    void toggleSolo();
    void toggleRecordArm();

    // Mode switching
    void enterInsertMode();
    void enterNormalMode();

    // Panel
    void cycleFocusPanel();

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
    bool handleNormalKey (const juce::KeyPress& key);
    bool handleInsertKey (const juce::KeyPress& key);
    bool handleCommandKey (const juce::KeyPress& key);
    void executeCommand();

    bool handleSequencerNormalKey (const juce::KeyPress& key);

    // Stubs
    void openFocusedItem();

    // Pending key helpers
    void clearPending();

    // Count helpers
    bool isDigitForCount (juce_wchar c) const;
    void accumulateDigit (juce_wchar c);
    int  getEffectiveCount() const;
    void resetCounts();

    // Operator state
    void startOperator (Operator op);
    void cancelOperator();
    Operator charToOperator (juce_wchar c) const;

    // Motion
    bool isMotionKey (juce_wchar c) const;
    MotionRange resolveMotion (juce_wchar key, int count) const;
    MotionRange resolveLinewiseMotion (int count) const;

    // Operator execution
    void executeOperator (Operator op, const MotionRange& range);
    void executeDelete (const MotionRange& range);
    void executeYank (const MotionRange& range);
    void executeChange (const MotionRange& range);
    void executeMotion (juce_wchar key, int count);

    Project& project;
    TransportController& transport;
    Arrangement& arrangement;
    VimContext& context;

    Mode mode = Normal;
    juce::String commandBuffer;
    juce_wchar pendingKey = 0;
    juce::int64 pendingTimestamp = 0;
    static constexpr juce::int64 pendingTimeoutMs = 1000;

    // Operator-pending state
    Operator pendingOperator = OpNone;
    int countAccumulator = 0;   // count typed before operator (e.g. the 3 in 3d2j)
    int operatorCount = 0;      // count typed after operator  (e.g. the 2 in 3d2j)

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VimEngine)
};

} // namespace dc

#pragma once
#include <JuceHeader.h>
#include "VimContext.h"
#include "model/Project.h"
#include "model/Arrangement.h"
#include "model/Track.h"
#include "engine/TransportController.h"

namespace dc
{

class VimEngine : public juce::KeyListener
{
public:
    enum Mode { Normal, Insert };

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

    Mode getMode() const { return mode; }
    bool hasPendingKey() const { return pendingKey != 0; }

    void addListener (Listener* l) { listeners.add (l); }
    void removeListener (Listener* l) { listeners.remove (l); }

private:
    bool handleNormalKey (const juce::KeyPress& key);
    bool handleInsertKey (const juce::KeyPress& key);

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

    // Stubs
    void openFocusedItem();

    // Pending key helpers
    void clearPending();

    Project& project;
    TransportController& transport;
    Arrangement& arrangement;
    VimContext& context;

    Mode mode = Normal;
    juce_wchar pendingKey = 0;
    juce::int64 pendingTimestamp = 0;
    static constexpr juce::int64 pendingTimeoutMs = 1000;

    juce::ListenerList<Listener> listeners;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (VimEngine)
};

} // namespace dc

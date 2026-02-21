#pragma once
#include <JuceHeader.h>

namespace dc
{

class UndoSystem
{
public:
    explicit UndoSystem (juce::UndoManager& undoManager);

    // Transaction grouping
    void beginTransaction (const juce::String& name = {});
    void endTransaction();

    // Coalescing for continuous edits (e.g. fader drags)
    // Groups rapid edits within a time window into a single undo step
    void beginCoalescedTransaction (const juce::String& name, int coalescingWindowMs = 500);

    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    juce::String getUndoDescription() const;
    juce::String getRedoDescription() const;

    juce::UndoManager& getUndoManager() { return undoManager; }

private:
    juce::UndoManager& undoManager;
    juce::String currentCoalescingName;
    juce::int64 lastCoalescingTime = 0;
    int coalescingWindowMs = 500;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (UndoSystem)
};

class ScopedTransaction
{
public:
    explicit ScopedTransaction (UndoSystem& us, const juce::String& name)
    {
        us.beginTransaction (name);
    }

    ~ScopedTransaction() = default;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ScopedTransaction)
};

} // namespace dc

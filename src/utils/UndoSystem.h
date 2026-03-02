#pragma once
#include <JuceHeader.h>
#include <string>

namespace dc
{

class UndoSystem
{
public:
    explicit UndoSystem (juce::UndoManager& undoManager);

    // Transaction grouping
    void beginTransaction (const std::string& name = {});
    void endTransaction();

    // Coalescing for continuous edits (e.g. fader drags)
    // Groups rapid edits within a time window into a single undo step
    void beginCoalescedTransaction (const std::string& name, int coalescingWindowMs = 500);

    void undo();
    void redo();
    bool canUndo() const;
    bool canRedo() const;

    std::string getUndoDescription() const;
    std::string getRedoDescription() const;

    juce::UndoManager& getUndoManager() { return undoManager; }

private:
    juce::UndoManager& undoManager;
    std::string currentCoalescingName;
    int64_t lastCoalescingTime = 0;
    int coalescingWindowMs = 500;

    UndoSystem (const UndoSystem&) = delete;
    UndoSystem& operator= (const UndoSystem&) = delete;
};

class ScopedTransaction
{
public:
    explicit ScopedTransaction (UndoSystem& us, const std::string& name)
    {
        us.beginTransaction (name);
    }

    ~ScopedTransaction() = default;

private:
    ScopedTransaction (const ScopedTransaction&) = delete;
    ScopedTransaction& operator= (const ScopedTransaction&) = delete;
};

} // namespace dc

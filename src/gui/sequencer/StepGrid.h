#pragma once
#include <JuceHeader.h>
#include "StepButton.h"
#include "model/Project.h"
#include "model/StepSequencer.h"

namespace dc
{

class StepGrid : public juce::Component,
                 private juce::ValueTree::Listener
{
public:
    StepGrid (Project& project);
    ~StepGrid() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    // Update from model
    void rebuild();

    // Cursor
    void setCursorPosition (int row, int step);
    int  getCursorRow() const  { return cursorRow; }
    int  getCursorStep() const { return cursorStep; }

    // Playback highlight
    void setPlaybackStep (int step);

    int getNumRows() const;
    int getNumSteps() const;

    static constexpr int rowLabelWidth = 120;
    static constexpr int stepSize      = 32;
    static constexpr int rowHeight     = 32;

private:
    void valueTreePropertyChanged (juce::ValueTree&, const juce::Identifier&) override;
    void valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&) override;
    void valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int) override;

    Project& project;
    juce::OwnedArray<StepButton> buttons; // numRows * numSteps

    int numRows  = 0;
    int numSteps = 0;
    int cursorRow  = 0;
    int cursorStep = 0;
    int playbackStep = -1;

    StepButton* getButton (int row, int step);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (StepGrid)
};

} // namespace dc

#include "StepGrid.h"
#include "utils/UndoSystem.h"

namespace dc
{

StepGrid::StepGrid (Project& p)
    : project (p)
{
    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
    if (seqState.isValid())
        seqState.addListener (this);

    rebuild();
}

StepGrid::~StepGrid()
{
    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
    if (seqState.isValid())
        seqState.removeListener (this);
}

void StepGrid::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a1a2a));

    // Draw row labels
    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);

    g.setFont (13.0f);

    for (int r = 0; r < numRows; ++r)
    {
        auto row = seq.getRow (r);
        auto rowName = StepSequencer::getRowName (row);
        bool muted = StepSequencer::isRowMuted (row);
        bool soloed = StepSequencer::isRowSoloed (row);

        auto labelBounds = juce::Rectangle<int> (4, r * rowHeight, rowLabelWidth - 8, rowHeight);

        if (muted)
            g.setColour (juce::Colour (0xff666666));
        else if (soloed)
            g.setColour (juce::Colour (0xffffff00));
        else
            g.setColour (juce::Colour (0xffcccccc));

        g.drawText (rowName, labelBounds, juce::Justification::centredLeft, true);
    }

    // Draw beat-group separators
    if (numSteps > 0)
    {
        g.setColour (juce::Colour (0xff444466));
        for (int s = 4; s < numSteps; s += 4)
        {
            int x = rowLabelWidth + s * stepSize;
            g.drawVerticalLine (x, 0.0f, static_cast<float> (getHeight()));
        }
    }
}

void StepGrid::resized()
{
    for (int r = 0; r < numRows; ++r)
    {
        for (int s = 0; s < numSteps; ++s)
        {
            auto* btn = getButton (r, s);
            if (btn != nullptr)
                btn->setBounds (rowLabelWidth + s * stepSize, r * rowHeight, stepSize, rowHeight);
        }
    }
}

void StepGrid::rebuild()
{
    buttons.clear();

    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
    if (! seqState.isValid())
    {
        numRows = 0;
        numSteps = 0;
        return;
    }

    StepSequencer seq (seqState);
    auto pattern = seq.getActivePattern();
    if (! pattern.isValid())
    {
        numRows = 0;
        numSteps = 0;
        return;
    }

    numRows  = seq.getNumRows();
    numSteps = static_cast<int> (pattern.getProperty (IDs::numSteps, 16));

    for (int r = 0; r < numRows; ++r)
    {
        auto rowState = seq.getRow (r);

        for (int s = 0; s < numSteps; ++s)
        {
            auto stepState = StepSequencer::getStep (rowState, s);
            auto* btn = buttons.add (new StepButton());

            btn->setActive (StepSequencer::isStepActive (stepState));
            btn->setVelocity (StepSequencer::getStepVelocity (stepState));
            btn->setSelected (r == cursorRow && s == cursorStep);
            btn->setPlayhead (s == playbackStep);

            // Click toggles the step
            btn->onClick = [this, r, s]
            {
                auto seqSt = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
                if (! seqSt.isValid()) return;

                StepSequencer seqModel (seqSt);
                auto row = seqModel.getRow (r);
                auto step = StepSequencer::getStep (row, s);
                if (! step.isValid()) return;

                ScopedTransaction txn (project.getUndoSystem(), "Toggle Step");
                bool isActive = StepSequencer::isStepActive (step);
                step.setProperty (IDs::active, ! isActive, &project.getUndoManager());
            };

            addAndMakeVisible (btn);
        }
    }

    // Set preferred size for parent layout
    setSize (rowLabelWidth + numSteps * stepSize, numRows * rowHeight);
    resized();
}

void StepGrid::setCursorPosition (int row, int step)
{
    // Deselect old
    if (auto* old = getButton (cursorRow, cursorStep))
        old->setSelected (false);

    cursorRow  = juce::jlimit (0, std::max (numRows - 1, 0), row);
    cursorStep = juce::jlimit (0, std::max (numSteps - 1, 0), step);

    // Select new
    if (auto* btn = getButton (cursorRow, cursorStep))
        btn->setSelected (true);
}

void StepGrid::setPlaybackStep (int step)
{
    if (step == playbackStep)
        return;

    // Clear old highlight
    for (int r = 0; r < numRows; ++r)
    {
        if (auto* old = getButton (r, playbackStep))
            old->setPlayhead (false);
    }

    playbackStep = step;

    // Set new highlight
    for (int r = 0; r < numRows; ++r)
    {
        if (auto* btn = getButton (r, playbackStep))
            btn->setPlayhead (true);
    }
}

int StepGrid::getNumRows() const { return numRows; }
int StepGrid::getNumSteps() const { return numSteps; }

StepButton* StepGrid::getButton (int row, int step)
{
    if (row < 0 || row >= numRows || step < 0 || step >= numSteps)
        return nullptr;

    int idx = row * numSteps + step;
    return buttons[idx];
}

void StepGrid::valueTreePropertyChanged (juce::ValueTree& tree, const juce::Identifier&)
{
    if (tree.hasType (IDs::STEP))
    {
        // Find which step changed and update its button
        auto rowState = tree.getParent();
        if (! rowState.isValid()) return;

        auto patternState = rowState.getParent();
        if (! patternState.isValid()) return;

        int r = patternState.indexOf (rowState);
        int s = static_cast<int> (tree.getProperty (IDs::index, -1));

        if (auto* btn = getButton (r, s))
        {
            btn->setActive (StepSequencer::isStepActive (tree));
            btn->setVelocity (StepSequencer::getStepVelocity (tree));
        }
    }
    else if (tree.hasType (IDs::STEP_ROW))
    {
        repaint(); // row label may have changed
    }
}

void StepGrid::valueTreeChildAdded (juce::ValueTree&, juce::ValueTree&)
{
    rebuild();
}

void StepGrid::valueTreeChildRemoved (juce::ValueTree&, juce::ValueTree&, int)
{
    rebuild();
}

} // namespace dc

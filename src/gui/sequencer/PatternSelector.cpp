#include "PatternSelector.h"

namespace dc
{

PatternSelector::PatternSelector (Project& p)
    : project (p)
{
    rebuild();
}

void PatternSelector::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1e1e2e));

    g.setColour (juce::Colour (0xffcccccc));
    g.setFont (13.0f);
    g.drawText ("Pattern:", 4, 0, 60, preferredHeight, juce::Justification::centredLeft);
}

void PatternSelector::resized()
{
    int x = 68;
    for (auto* btn : patternButtons)
    {
        btn->setBounds (x, 4, buttonWidth, preferredHeight - 8);
        x += buttonWidth + 4;
    }
}

void PatternSelector::rebuild()
{
    patternButtons.clear();

    auto seqState = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
    if (! seqState.isValid()) return;

    StepSequencer seq (seqState);
    int activeBank = seq.getActivePatternBank();
    int activeSlot = seq.getActivePatternSlot();

    for (int i = 0; i < seq.getNumPatterns(); ++i)
    {
        auto pattern = seq.getPattern (i);
        auto name = pattern.getProperty (IDs::name, "?").toString();
        int bank = static_cast<int> (pattern.getProperty (IDs::bank, 0));
        int slot = static_cast<int> (pattern.getProperty (IDs::slot, 0));

        auto* btn = patternButtons.add (new juce::TextButton (name));

        bool isActive = (bank == activeBank && slot == activeSlot);
        btn->setToggleState (isActive, juce::dontSendNotification);
        btn->setClickingTogglesState (false);

        btn->onClick = [this, bank, slot]
        {
            auto seqSt = project.getState().getChildWithName (IDs::STEP_SEQUENCER);
            if (! seqSt.isValid()) return;

            StepSequencer seqModel (seqSt);
            seqModel.setActivePattern (bank, slot, &project.getUndoManager());
            rebuild();
        };

        addAndMakeVisible (btn);
    }

    resized();
}

} // namespace dc

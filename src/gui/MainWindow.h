#pragma once
#include <JuceHeader.h>

namespace dc
{

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow (const juce::String& name);
    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

} // namespace dc

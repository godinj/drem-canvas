#pragma once
#include <JuceHeader.h>
#include <string>

namespace dc
{

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow (const std::string& name);
    void closeButtonPressed() override;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
};

} // namespace dc

#include "MainWindow.h"
#include "MainComponent.h"

namespace dc
{

MainWindow::MainWindow (const juce::String& name)
    : DocumentWindow (name,
                      juce::Colours::darkgrey,
                      DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setContentOwned (new MainComponent(), true);
    setResizable (true, true);
    centreWithSize (getWidth(), getHeight());
    setVisible (true);
}

void MainWindow::closeButtonPressed()
{
    juce::JUCEApplication::getInstance()->systemRequestedQuit();
}

} // namespace dc

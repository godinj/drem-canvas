#include "MainWindow.h"
#include "MainComponent.h"
#include "gui/common/ColourBridge.h"
#include "dc/foundation/types.h"

using dc::bridge::toJuce;

namespace dc
{

MainWindow::MainWindow (const std::string& name)
    : DocumentWindow (name,
                      toJuce (dc::Colours::darkgrey),
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

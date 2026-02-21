#include <JuceHeader.h>
#include "gui/MainWindow.h"

class DremCanvasApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override           { return false; }

    void initialise (const juce::String& /*commandLine*/) override
    {
        mainWindow = std::make_unique<dc::MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        mainWindow.reset();
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    std::unique_ptr<dc::MainWindow> mainWindow;
};

START_JUCE_APPLICATION (DremCanvasApplication)

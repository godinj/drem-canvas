#include "TransportBarWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"

namespace dc
{
namespace ui
{

TransportBarWidget::TransportBarWidget (TransportController& transport)
    : transportController (transport),
      playButton ("Play"),
      stopButton ("Stop"),
      timeDisplay ("00:00.000", gfx::LabelWidget::Centre),
      saveButton ("Save"),
      loadButton ("Load"),
      importButton ("Import"),
      audioSettingsButton ("Audio"),
      pluginsButton ("Plugins")
{
    timeDisplay.setUseMonoFont (true);
    timeDisplay.setFontSize (16.0f);

    addChild (&playButton);
    addChild (&stopButton);
    addChild (&timeDisplay);
    addChild (&saveButton);
    addChild (&loadButton);
    addChild (&importButton);
    addChild (&audioSettingsButton);
    addChild (&pluginsButton);

    playButton.onClick = [this]()
    {
        transportController.togglePlayStop();
    };

    stopButton.onClick = [this]()
    {
        transportController.stop();
        transportController.setPositionInSamples (0);
    };

    saveButton.onClick = [this]()
    {
        if (onSaveSession) onSaveSession();
    };

    loadButton.onClick = [this]()
    {
        if (onLoadSession) onLoadSession();
    };

    importButton.onClick = [this]()
    {
        if (onImportAudio) onImportAudio();
    };

    audioSettingsButton.onClick = [this]()
    {
        if (onAudioSettings) onAudioSettings();
    };

    pluginsButton.onClick = [this]()
    {
        if (onToggleBrowser) onToggleBrowser();
    };

    setAnimating (true);
}

void TransportBarWidget::paint (gfx::Canvas& canvas)
{
    auto& theme = gfx::Theme::getDefault();
    canvas.fillRect (gfx::Rect (0, 0, getWidth(), getHeight()),
                     gfx::Color::fromARGB (0xff2d2d3d));
}

void TransportBarWidget::resized()
{
    float buttonWidth = 70.0f;
    float margin = 4.0f;
    float w = getWidth();
    float h = getHeight();
    float bh = h - 2.0f * margin;
    float bw = buttonWidth - 2.0f * margin;

    // Left side: Play | Stop | Time
    playButton.setBounds (margin, margin, bw, bh);
    stopButton.setBounds (buttonWidth + margin, margin, bw, bh);

    // Right side: utility buttons
    float rightX = w;
    rightX -= buttonWidth;
    pluginsButton.setBounds (rightX + margin, margin, bw, bh);
    rightX -= buttonWidth;
    audioSettingsButton.setBounds (rightX + margin, margin, bw, bh);
    rightX -= buttonWidth;
    importButton.setBounds (rightX + margin, margin, bw, bh);
    rightX -= buttonWidth;
    loadButton.setBounds (rightX + margin, margin, bw, bh);
    rightX -= buttonWidth;
    saveButton.setBounds (rightX + margin, margin, bw, bh);

    // Time display fills the middle
    float timeX = buttonWidth * 2.0f;
    float timeW = rightX - timeX;
    if (timeW < 0) timeW = 0;
    timeDisplay.setBounds (timeX, 0, timeW, h);
}

void TransportBarWidget::animationTick (double /*timestampMs*/)
{
    auto timeStr = transportController.getTimeString().toStdString();
    timeDisplay.setText (timeStr);

    if (transportController.isPlaying())
        playButton.setText ("Pause");
    else
        playButton.setText ("Play");
}

} // namespace ui
} // namespace dc

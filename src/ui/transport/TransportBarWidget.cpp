#include "TransportBarWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include <cmath>

namespace dc
{
namespace ui
{

TransportBarWidget::TransportBarWidget (TransportController& transport, TempoMap& tempo)
    : transportController (transport),
      tempoMap (tempo),
      playButton ("Play"),
      stopButton ("Stop"),
      timeDisplay ("1.1.000", gfx::LabelWidget::Centre),
      tempoDisplay ("120 BPM", gfx::LabelWidget::Centre),
      saveButton ("Save"),
      loadButton ("Load"),
      importButton ("Import"),
      audioSettingsButton ("Audio"),
      pluginsButton ("Plugins")
{
    timeDisplay.setUseMonoFont (true);
    timeDisplay.setFontSize (16.0f);

    tempoDisplay.setUseMonoFont (true);
    tempoDisplay.setFontSize (14.0f);

    addChild (&playButton);
    addChild (&stopButton);
    addChild (&timeDisplay);
    addChild (&tempoDisplay);
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

    updateTempoDisplay();
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
    float tempoWidth = 90.0f;
    float margin = 4.0f;
    float w = getWidth();
    float h = getHeight();
    float bh = h - 2.0f * margin;
    float bw = buttonWidth - 2.0f * margin;

    // Left side: Play | Stop | Tempo | Time
    playButton.setBounds (margin, margin, bw, bh);
    stopButton.setBounds (buttonWidth + margin, margin, bw, bh);
    tempoDisplay.setBounds (buttonWidth * 2.0f, 0, tempoWidth, h);

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

    // Time display fills the middle (after tempo)
    float timeX = buttonWidth * 2.0f + tempoWidth;
    float timeW = rightX - timeX;
    if (timeW < 0) timeW = 0;
    timeDisplay.setBounds (timeX, 0, timeW, h);
}

void TransportBarWidget::animationTick (double /*timestampMs*/)
{
    auto pos = tempoMap.samplesToBarBeat (transportController.getPositionInSamples(),
                                          transportController.getSampleRate());
    auto timeStr = tempoMap.formatBarBeat (pos).toStdString();
    timeDisplay.setText (timeStr);

    if (transportController.isPlaying())
        playButton.setText ("Pause");
    else
        playButton.setText ("Play");
}

void TransportBarWidget::mouseWheel (const gfx::WheelEvent& e)
{
    // Check if wheel is over the tempo display area
    auto tempoBounds = tempoDisplay.getBounds();
    if (e.x >= tempoBounds.x && e.x < tempoBounds.x + tempoBounds.width
        && e.y >= tempoBounds.y && e.y < tempoBounds.y + tempoBounds.height)
    {
        float delta = e.deltaY > 0 ? 1.0f : -1.0f;
        double newTempo = tempoMap.getTempo() + delta;
        newTempo = std::max (20.0, std::min (300.0, newTempo));
        tempoMap.setTempo (newTempo);
        updateTempoDisplay();
    }
}

void TransportBarWidget::updateTempoDisplay()
{
    int bpm = static_cast<int> (std::round (tempoMap.getTempo()));
    tempoDisplay.setText (std::to_string (bpm) + " BPM");
}

} // namespace ui
} // namespace dc

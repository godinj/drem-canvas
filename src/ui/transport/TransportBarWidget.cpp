#include "TransportBarWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include <cmath>

namespace dc
{
namespace ui
{

static std::string getBranchBadge()
{
#ifdef DC_GIT_BRANCH
    std::string branch = DC_GIT_BRANCH;
    if (! branch.empty() && branch != "master" && branch != "main")
        return branch;
#endif
    return "";
}

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
      pluginsButton ("Plugins"),
      branchLabel (getBranchBadge(), gfx::LabelWidget::Centre)
{
    timeDisplay.setUseMonoFont (true);
    timeDisplay.setFontSize (16.0f);

    tempoDisplay.setUseMonoFont (true);
    tempoDisplay.setFontSize (14.0f);
    tempoDisplay.setInterceptsMouse (false);

    addChild (&playButton);
    addChild (&stopButton);
    addChild (&timeDisplay);
    addChild (&tempoDisplay);
    addChild (&saveButton);
    addChild (&loadButton);
    addChild (&importButton);
    addChild (&audioSettingsButton);
    addChild (&pluginsButton);
    addChild (&cpuMeter);

    auto badge = getBranchBadge();
    if (! badge.empty())
    {
        branchLabel.setFontSize (11.0f);
        branchLabel.setTextColor (gfx::Color::fromARGB (0xff80e0a0));
        addChild (&branchLabel);
    }

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
    float cpuMeterWidth = 100.0f;
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
    rightX -= cpuMeterWidth;
    cpuMeter.setBounds (rightX + margin, margin, cpuMeterWidth - 2.0f * margin, bh);

    // Branch badge (if on a feature branch) and time display fill the middle
    float timeX = buttonWidth * 2.0f + tempoWidth;
    if (branchLabel.getParent() != nullptr)
    {
        float branchW = 200.0f;
        branchLabel.setBounds (timeX, 0, branchW, h);
        timeX += branchW;
    }
    float timeW = rightX - timeX;
    if (timeW < 0) timeW = 0;
    timeDisplay.setBounds (timeX, 0, timeW, h);
}

void TransportBarWidget::animationTick (double /*timestampMs*/)
{
    auto pos = tempoMap.samplesToBarBeat (transportController.getPositionInSamples(),
                                          transportController.getSampleRate());
    auto timeStr = tempoMap.formatBarBeat (pos);
    timeDisplay.setText (timeStr);

    if (! editingTempo)
        updateTempoDisplay();

    if (transportController.isPlaying())
        playButton.setText ("Pause");
    else
        playButton.setText ("Play");
}

void TransportBarWidget::mouseDown (const gfx::MouseEvent& e)
{
    auto tempoBounds = tempoDisplay.getBounds();
    if (e.x >= tempoBounds.x && e.x < tempoBounds.x + tempoBounds.width
        && e.y >= tempoBounds.y && e.y < tempoBounds.y + tempoBounds.height)
    {
        editingTempo = true;
        tempoEditBuffer.clear();
        tempoDisplay.setText ("_ BPM");
        setFocusable (true);
        grabFocus();
    }
    else if (editingTempo)
    {
        commitTempoEdit();
    }
}

bool TransportBarWidget::keyDown (const gfx::KeyEvent& e)
{
    if (! editingTempo)
        return false;

    char ch = static_cast<char> (e.character);

    if (ch >= '0' && ch <= '9')
    {
        if (tempoEditBuffer.size() < 3)
        {
            tempoEditBuffer += ch;
            tempoDisplay.setText (tempoEditBuffer + "_ BPM");
        }
        return true;
    }

    // Backspace (ASCII 8/127 or Mac virtual key 0x33)
    if (e.keyCode == 8 || e.keyCode == 127 || e.keyCode == 0x33)
    {
        if (! tempoEditBuffer.empty())
            tempoEditBuffer.pop_back();
        tempoDisplay.setText (tempoEditBuffer + "_ BPM");
        return true;
    }

    // Enter — commit (ASCII 13 or Mac virtual key 0x24)
    if (e.keyCode == 13 || e.keyCode == 0x24 || ch == '\r' || ch == '\n')
    {
        commitTempoEdit();
        return true;
    }

    // Escape — cancel (ASCII 27 or Mac virtual key 0x35)
    if (e.keyCode == 27 || e.keyCode == 0x35)
    {
        cancelTempoEdit();
        return true;
    }

    return true; // consume all keys while editing
}

bool TransportBarWidget::mouseWheel (const gfx::WheelEvent& e)
{
    // Check if wheel is over the tempo display area
    auto tempoBounds = tempoDisplay.getBounds();
    if (e.x >= tempoBounds.x && e.x < tempoBounds.x + tempoBounds.width
        && e.y >= tempoBounds.y && e.y < tempoBounds.y + tempoBounds.height)
    {
        float delta = e.deltaY > 0 ? 1.0f : -1.0f;
        double newTempo = tempoMap.getTempo() + delta;
        newTempo = std::max (20.0, std::min (300.0, newTempo));
        if (onTempoChanged)
            onTempoChanged (newTempo);
        return true;
    }
    return false;
}

void TransportBarWidget::commitTempoEdit()
{
    if (! tempoEditBuffer.empty())
    {
        double newTempo = std::stod (tempoEditBuffer);
        newTempo = std::max (20.0, std::min (300.0, newTempo));
        if (onTempoChanged)
            onTempoChanged (newTempo);
    }
    editingTempo = false;
    tempoEditBuffer.clear();
    releaseFocus();
    setFocusable (false);
    updateTempoDisplay();
}

void TransportBarWidget::cancelTempoEdit()
{
    editingTempo = false;
    tempoEditBuffer.clear();
    releaseFocus();
    setFocusable (false);
    updateTempoDisplay();
}

void TransportBarWidget::updateTempoDisplay()
{
    int bpm = static_cast<int> (std::round (tempoMap.getTempo()));
    tempoDisplay.setText (std::to_string (bpm) + " BPM");
}

} // namespace ui
} // namespace dc

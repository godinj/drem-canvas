#include "VirtualKeyboardWidget.h"
#include "graphics/rendering/Canvas.h"
#include "graphics/theme/Theme.h"
#include "graphics/theme/FontManager.h"

namespace dc
{
namespace ui
{

// Key layout definition
struct PianoKeyDef
{
    char qwertyLabel;
    const char* noteName;
    int semitone;     // semitone offset from base octave
    bool isBlack;
};

static const PianoKeyDef keyDefs[] = {
    { 'A', "C",  0,  false },
    { 'W', "C#", 1,  true  },
    { 'S', "D",  2,  false },
    { 'E', "D#", 3,  true  },
    { 'D', "E",  4,  false },
    { 'F', "F",  5,  false },
    { 'T', "F#", 6,  true  },
    { 'G', "G",  7,  false },
    { 'Y', "G#", 8,  true  },
    { 'H', "A",  9,  false },
    { 'U', "A#", 10, true  },
    { 'J', "B",  11, false },
    { 'K', "C",  12, false },
    { 'O', "C#", 13, true  },
    { 'L', "D",  14, false },
    { 'P', "D#", 15, true  },
    { ';', "E",  16, false },
};

static constexpr int numKeys = sizeof (keyDefs) / sizeof (keyDefs[0]);
static constexpr int numWhiteKeys = 10; // A S D F G H J K L ;

VirtualKeyboardWidget::VirtualKeyboardWidget (VirtualKeyboardState& state)
    : kbState (state)
{
    kbState.addListener (this);
    setAnimating (true);
}

VirtualKeyboardWidget::~VirtualKeyboardWidget()
{
    kbState.removeListener (this);
}

void VirtualKeyboardWidget::paint (gfx::Canvas& canvas)
{
    using namespace gfx;
    auto& fm = FontManager::getInstance();
    auto& font = fm.getSmallFont();
    float w = getWidth();
    float h = getHeight();

    // Background
    canvas.fillRect (Rect (0, 0, w, h), Color::fromARGB (0xff1e1e2e));

    // Info bar at bottom (16px)
    float infoH = 16.0f;
    float keysH = h - infoH;
    float keysY = 0.0f;

    // Draw info bar
    canvas.fillRect (Rect (0, keysH, w, infoH), Color::fromARGB (0xff181825));

    auto infoText = "Oct: " + std::to_string (kbState.baseOctave)
                  + "  Vel: " + std::to_string (kbState.velocity)
                  + "  Ch: " + std::to_string (kbState.midiChannel)
                  + "        [Z]<< >>[X]  [C]- +[V]";

    canvas.drawText (infoText, 8.0f, keysH + infoH * 0.5f + 4.0f, font,
                     Color::fromARGB (0xffa6adc8));

    // Piano keys layout
    float keyboardW = std::min (w - 20.0f, 680.0f);
    float startX = 10.0f;

    float whiteKeyW = keyboardW / static_cast<float> (numWhiteKeys);
    float whiteKeyH = keysH - 4.0f;
    float blackKeyW = whiteKeyW * 0.6f;
    float blackKeyH = whiteKeyH * 0.6f;

    // Map white key index for positioning
    int whiteIdx = 0;

    // First pass: draw white keys
    for (int i = 0; i < numKeys; ++i)
    {
        if (keyDefs[i].isBlack) continue;

        float kx = startX + static_cast<float> (whiteIdx) * whiteKeyW;
        float ky = keysY + 2.0f;

        int midiNote = kbState.baseOctave * 12 + keyDefs[i].semitone;
        bool pressed = kbState.heldNotes.count (midiNote) > 0;

        Color keyColor = pressed ? Color::fromARGB (0xff89b4fa) : Color::fromARGB (0xfff0f0f0);
        canvas.fillRoundedRect (Rect (kx + 1.0f, ky, whiteKeyW - 2.0f, whiteKeyH), 3.0f, keyColor);
        canvas.strokeRect (Rect (kx + 1.0f, ky, whiteKeyW - 2.0f, whiteKeyH),
                           Color::fromARGB (0xff45475a), 1.0f);

        // QWERTY label at bottom of key
        std::string label (1, keyDefs[i].qwertyLabel);
        Color labelColor = pressed ? Color::fromARGB (0xff1e1e2e) : Color::fromARGB (0xff45475a);
        canvas.drawTextCentred (label,
                                Rect (kx + 1.0f, ky + whiteKeyH - 18.0f, whiteKeyW - 2.0f, 16.0f),
                                font, labelColor);

        // Note name above QWERTY label
        std::string noteName = std::string (keyDefs[i].noteName)
                             + std::to_string (kbState.baseOctave + keyDefs[i].semitone / 12);
        canvas.drawTextCentred (noteName,
                                Rect (kx + 1.0f, ky + whiteKeyH - 34.0f, whiteKeyW - 2.0f, 16.0f),
                                font, labelColor);

        ++whiteIdx;
    }

    // Second pass: draw black keys on top
    whiteIdx = 0;
    int prevWhiteIdx = 0;
    for (int i = 0; i < numKeys; ++i)
    {
        if (keyDefs[i].isBlack)
        {
            // Position black key between the previous white key and the next
            float kx = startX + (static_cast<float> (prevWhiteIdx) + 1.0f) * whiteKeyW
                      - blackKeyW * 0.5f;
            float ky = keysY + 2.0f;

            int midiNote = kbState.baseOctave * 12 + keyDefs[i].semitone;
            bool pressed = kbState.heldNotes.count (midiNote) > 0;

            Color keyColor = pressed ? Color::fromARGB (0xff74c7ec) : Color::fromARGB (0xff313244);
            canvas.fillRoundedRect (Rect (kx, ky, blackKeyW, blackKeyH), 3.0f, keyColor);

            // QWERTY label
            std::string label (1, keyDefs[i].qwertyLabel);
            Color labelColor = pressed ? Color::fromARGB (0xff1e1e2e) : Color::fromARGB (0xffa6adc8);
            canvas.drawTextCentred (label,
                                    Rect (kx, ky + blackKeyH - 16.0f, blackKeyW, 14.0f),
                                    font, labelColor);
        }
        else
        {
            prevWhiteIdx = whiteIdx;
            ++whiteIdx;
        }
    }
}

void VirtualKeyboardWidget::animationTick (double /*timestampMs*/)
{
    repaint();
}

void VirtualKeyboardWidget::keyboardStateChanged()
{
    repaint();
}

} // namespace ui
} // namespace dc

#include <catch2/catch_test_macros.hpp>
#include "vim/VimContext.h"

TEST_CASE ("VimContext default state", "[integration][vim_context]")
{
    dc::VimContext ctx;

    CHECK (ctx.getPanel() == dc::VimContext::Editor);
    CHECK (ctx.getPanelName() == "Editor");
    CHECK (ctx.getMixerFocus() == dc::VimContext::FocusNone);
    CHECK (ctx.getGridCursorPosition() == 0);
    CHECK (ctx.getSelectedClipIndex() == 0);
    CHECK_FALSE (ctx.isPluginViewEnlarged());
    CHECK_FALSE (ctx.isNumberEntryActive());
    CHECK (ctx.getHintMode() == dc::VimContext::HintNone);
}

TEST_CASE ("VimContext panel cycling", "[integration][vim_context]")
{
    dc::VimContext ctx;

    CHECK (ctx.getPanel() == dc::VimContext::Editor);

    ctx.cyclePanel();
    CHECK (ctx.getPanel() == dc::VimContext::Mixer);
    CHECK (ctx.getPanelName() == "Mixer");
    CHECK (ctx.getMixerFocus() == dc::VimContext::FocusVolume);

    ctx.cyclePanel();
    CHECK (ctx.getPanel() == dc::VimContext::Sequencer);
    CHECK (ctx.getPanelName() == "Sequencer");
    CHECK (ctx.getMixerFocus() == dc::VimContext::FocusNone);

    ctx.cyclePanel();
    CHECK (ctx.getPanel() == dc::VimContext::Editor);
}

TEST_CASE ("VimContext setPanel to Mixer sets FocusVolume", "[integration][vim_context]")
{
    dc::VimContext ctx;

    ctx.setPanel (dc::VimContext::Mixer);
    CHECK (ctx.getMixerFocus() == dc::VimContext::FocusVolume);

    ctx.setPanel (dc::VimContext::Editor);
    CHECK (ctx.getMixerFocus() == dc::VimContext::FocusNone);
}

TEST_CASE ("VimContext cyclePanel from PianoRoll exits to Editor", "[integration][vim_context]")
{
    dc::VimContext ctx;
    ctx.setPanel (dc::VimContext::PianoRoll);
    CHECK (ctx.getPanel() == dc::VimContext::PianoRoll);

    ctx.cyclePanel();
    CHECK (ctx.getPanel() == dc::VimContext::Editor);
}

TEST_CASE ("VimContext cyclePanel from PluginView exits to Mixer", "[integration][vim_context]")
{
    dc::VimContext ctx;
    ctx.setPanel (dc::VimContext::PluginView);
    CHECK (ctx.getPanel() == dc::VimContext::PluginView);

    ctx.cyclePanel();
    CHECK (ctx.getPanel() == dc::VimContext::Mixer);
    CHECK (ctx.getMixerFocus() == dc::VimContext::FocusVolume);
}

TEST_CASE ("VimContext mixer focus", "[integration][vim_context]")
{
    dc::VimContext ctx;
    ctx.setPanel (dc::VimContext::Mixer);

    ctx.setMixerFocus (dc::VimContext::FocusVolume);
    CHECK (ctx.getMixerFocusName() == "Volume");

    ctx.setMixerFocus (dc::VimContext::FocusPan);
    CHECK (ctx.getMixerFocusName() == "Pan");

    ctx.setMixerFocus (dc::VimContext::FocusPlugins);
    CHECK (ctx.getMixerFocusName() == "Plugins");
    CHECK (ctx.getSelectedPluginSlot() == 0); // reset on FocusPlugins
}

TEST_CASE ("VimContext visual selection", "[integration][vim_context]")
{
    dc::VimContext ctx;

    SECTION ("no active selection")
    {
        CHECK_FALSE (ctx.isTrackInVisualSelection (0));
        CHECK_FALSE (ctx.isClipInVisualSelection (0, 0));
    }

    SECTION ("linewise selection selects all clips on tracks")
    {
        dc::VimContext::VisualSelection sel;
        sel.active = true;
        sel.linewise = true;
        sel.startTrack = 1;
        sel.endTrack = 3;

        ctx.setVisualSelection (sel);

        CHECK_FALSE (ctx.isTrackInVisualSelection (0));
        CHECK (ctx.isTrackInVisualSelection (1));
        CHECK (ctx.isTrackInVisualSelection (2));
        CHECK (ctx.isTrackInVisualSelection (3));
        CHECK_FALSE (ctx.isTrackInVisualSelection (4));

        // Linewise: all clips on selected tracks are selected
        CHECK (ctx.isClipInVisualSelection (2, 0));
        CHECK (ctx.isClipInVisualSelection (2, 5));
    }

    SECTION ("single-track clip range selection")
    {
        dc::VimContext::VisualSelection sel;
        sel.active = true;
        sel.linewise = false;
        sel.startTrack = 2;
        sel.startClip = 1;
        sel.endTrack = 2;
        sel.endClip = 3;

        ctx.setVisualSelection (sel);

        CHECK (ctx.isClipInVisualSelection (2, 1));
        CHECK (ctx.isClipInVisualSelection (2, 2));
        CHECK (ctx.isClipInVisualSelection (2, 3));
        CHECK_FALSE (ctx.isClipInVisualSelection (2, 0));
        CHECK_FALSE (ctx.isClipInVisualSelection (2, 4));
    }

    SECTION ("clearVisualSelection deactivates")
    {
        dc::VimContext::VisualSelection sel;
        sel.active = true;
        sel.startTrack = 0;
        sel.endTrack = 0;
        ctx.setVisualSelection (sel);

        CHECK (ctx.isTrackInVisualSelection (0));

        ctx.clearVisualSelection();
        CHECK_FALSE (ctx.isTrackInVisualSelection (0));
    }
}

TEST_CASE ("VimContext grid cursor position", "[integration][vim_context]")
{
    dc::VimContext ctx;

    CHECK (ctx.getGridCursorPosition() == 0);

    ctx.setGridCursorPosition (44100);
    CHECK (ctx.getGridCursorPosition() == 44100);

    ctx.setGridCursorPosition (0);
    CHECK (ctx.getGridCursorPosition() == 0);
}

TEST_CASE ("VimContext plugin view target", "[integration][vim_context]")
{
    dc::VimContext ctx;

    CHECK (ctx.getPluginViewTrackIndex() == -1);
    CHECK (ctx.getPluginViewPluginIndex() == -1);

    ctx.setPluginViewTarget (2, 1);
    CHECK (ctx.getPluginViewTrackIndex() == 2);
    CHECK (ctx.getPluginViewPluginIndex() == 1);
    CHECK (ctx.getSelectedParamIndex() == 0);

    ctx.clearPluginViewTarget();
    CHECK (ctx.getPluginViewTrackIndex() == -1);
    CHECK (ctx.getPluginViewPluginIndex() == -1);
    CHECK_FALSE (ctx.isPluginViewEnlarged());
}

TEST_CASE ("VimContext hint mode", "[integration][vim_context]")
{
    dc::VimContext ctx;

    CHECK (ctx.getHintMode() == dc::VimContext::HintNone);

    ctx.setHintMode (dc::VimContext::HintActive);
    CHECK (ctx.getHintMode() == dc::VimContext::HintActive);

    ctx.setHintBuffer ("ab");
    CHECK (ctx.getHintBuffer() == "ab");

    ctx.clearHintBuffer();
    CHECK (ctx.getHintBuffer().empty());
}

TEST_CASE ("VimContext number entry", "[integration][vim_context]")
{
    dc::VimContext ctx;

    CHECK_FALSE (ctx.isNumberEntryActive());

    ctx.setNumberEntryActive (true);
    ctx.setNumberBuffer ("42");

    CHECK (ctx.isNumberEntryActive());
    CHECK (ctx.getNumberBuffer() == "42");

    ctx.clearNumberEntry();
    CHECK_FALSE (ctx.isNumberEntryActive());
    CHECK (ctx.getNumberBuffer().empty());
}

TEST_CASE ("VimContext sequencer cursor", "[integration][vim_context]")
{
    dc::VimContext ctx;

    CHECK (ctx.getSeqRow() == 0);
    CHECK (ctx.getSeqStep() == 0);

    ctx.setSeqRow (3);
    ctx.setSeqStep (7);

    CHECK (ctx.getSeqRow() == 3);
    CHECK (ctx.getSeqStep() == 7);
}

TEST_CASE ("VimContext grid visual selection", "[integration][vim_context]")
{
    dc::VimContext ctx;

    dc::VimContext::GridVisualSelection sel;
    sel.active = true;
    sel.startTrack = 1;
    sel.endTrack = 3;
    sel.startPos = 0;
    sel.endPos = 44100;

    ctx.setGridVisualSelection (sel);
    auto& stored = ctx.getGridVisualSelection();
    CHECK (stored.active);
    CHECK (stored.startTrack == 1);
    CHECK (stored.endTrack == 3);
    CHECK (stored.startPos == 0);
    CHECK (stored.endPos == 44100);

    ctx.clearGridVisualSelection();
    CHECK_FALSE (ctx.getGridVisualSelection().active);
}

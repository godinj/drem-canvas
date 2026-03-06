#include <catch2/catch_test_macros.hpp>
#include "vim/KeymapRegistry.h"
#include "vim/KeySequence.h"
#include <fstream>
#include <cstdio>

// Helper to create a temporary YAML file and return its path
static std::string writeTempYAML (const std::string& content)
{
    auto path = std::string (std::tmpnam (nullptr)) + ".yaml";
    std::ofstream out (path);
    out << content;
    out.close();
    return path;
}

static void removeTempFile (const std::string& path)
{
    std::remove (path.c_str());
}

TEST_CASE("Load YAML and resolve simple global binding", "[vim][keymap_registry]")
{
    auto path = writeTempYAML (R"(
normal:
  global:
    "Space": transport.play_stop
    "i": mode.insert
)");

    dc::KeymapRegistry reg;
    reg.loadFromYAML (path);
    removeTempFile (path);

    auto seq = dc::KeySequence::parse ("Space");
    auto result = reg.resolve (dc::VimModes::Normal, dc::VimContext::Editor, seq);
    REQUIRE(result == "transport.play_stop");

    auto seqI = dc::KeySequence::parse ("i");
    auto resultI = reg.resolve (dc::VimModes::Normal, dc::VimContext::Mixer, seqI);
    REQUIRE(resultI == "mode.insert");
}

TEST_CASE("Panel-specific binding overrides global", "[vim][keymap_registry]")
{
    auto path = writeTempYAML (R"(
normal:
  global:
    "Space": transport.play_stop
  sequencer:
    "Space": seq.toggle_step
)");

    dc::KeymapRegistry reg;
    reg.loadFromYAML (path);
    removeTempFile (path);

    auto seq = dc::KeySequence::parse ("Space");

    // From editor panel, should get the global binding
    auto fromEditor = reg.resolve (dc::VimModes::Normal, dc::VimContext::Editor, seq);
    REQUIRE(fromEditor == "transport.play_stop");

    // From sequencer panel, should get the panel-specific binding
    auto fromSeq = reg.resolve (dc::VimModes::Normal, dc::VimContext::Sequencer, seq);
    REQUIRE(fromSeq == "seq.toggle_step");
}

TEST_CASE("Null value unbinds a key in load", "[vim][keymap_registry]")
{
    auto path = writeTempYAML (R"(
normal:
  global:
    "Space": ~
    "i": mode.insert
)");

    dc::KeymapRegistry reg;
    reg.loadFromYAML (path);
    removeTempFile (path);

    auto seqSpace = dc::KeySequence::parse ("Space");
    auto result = reg.resolve (dc::VimModes::Normal, dc::VimContext::Editor, seqSpace);
    REQUIRE(result.empty());

    auto seqI = dc::KeySequence::parse ("i");
    auto resultI = reg.resolve (dc::VimModes::Normal, dc::VimContext::Editor, seqI);
    REQUIRE(resultI == "mode.insert");
}

TEST_CASE("Overlay user keymap overrides default", "[vim][keymap_registry]")
{
    auto defaultPath = writeTempYAML (R"(
normal:
  global:
    "Space": transport.play_stop
    "i": mode.insert
)");

    auto overlayPath = writeTempYAML (R"(
normal:
  global:
    "Space": custom.action
)");

    dc::KeymapRegistry reg;
    reg.loadFromYAML (defaultPath);
    reg.overlayFromYAML (overlayPath);
    removeTempFile (defaultPath);
    removeTempFile (overlayPath);

    auto seqSpace = dc::KeySequence::parse ("Space");
    auto result = reg.resolve (dc::VimModes::Normal, dc::VimContext::Editor, seqSpace);
    REQUIRE(result == "custom.action");

    // i should still be the default
    auto seqI = dc::KeySequence::parse ("i");
    auto resultI = reg.resolve (dc::VimModes::Normal, dc::VimContext::Editor, seqI);
    REQUIRE(resultI == "mode.insert");
}

TEST_CASE("Overlay with null removes binding", "[vim][keymap_registry]")
{
    auto defaultPath = writeTempYAML (R"(
normal:
  global:
    "Space": transport.play_stop
)");

    auto overlayPath = writeTempYAML (R"(
normal:
  global:
    "Space": ~
)");

    dc::KeymapRegistry reg;
    reg.loadFromYAML (defaultPath);
    reg.overlayFromYAML (overlayPath);
    removeTempFile (defaultPath);
    removeTempFile (overlayPath);

    auto seq = dc::KeySequence::parse ("Space");
    auto result = reg.resolve (dc::VimModes::Normal, dc::VimContext::Editor, seq);
    REQUIRE(result.empty());
}

TEST_CASE("feedKey multi-step gg returns pending then action", "[vim][keymap_registry]")
{
    auto path = writeTempYAML (R"(
normal:
  editor:
    "gg": motion.file_start
    "gp": view.toggle_browser
    "j": motion.down
)");

    dc::KeymapRegistry reg;
    reg.loadFromYAML (path);
    removeTempFile (path);

    // Feed 'g' -- should be pending (matches gg and gp)
    auto r1 = reg.feedKey (dc::VimModes::Normal, dc::VimContext::Editor,
                           'g', false, false, false, false);
    REQUIRE(r1 == "pending");

    // Feed second 'g' -- should complete
    auto r2 = reg.feedKey (dc::VimModes::Normal, dc::VimContext::Editor,
                           'g', false, false, false, false);
    REQUIRE(r2 == "motion.file_start");
}

TEST_CASE("feedKey no match returns empty", "[vim][keymap_registry]")
{
    auto path = writeTempYAML (R"(
normal:
  editor:
    "j": motion.down
)");

    dc::KeymapRegistry reg;
    reg.loadFromYAML (path);
    removeTempFile (path);

    auto result = reg.feedKey (dc::VimModes::Normal, dc::VimContext::Editor,
                               'x', false, false, false, false);
    REQUIRE(result.empty());
}

TEST_CASE("feedKey single key returns action immediately", "[vim][keymap_registry]")
{
    auto path = writeTempYAML (R"(
normal:
  editor:
    "j": motion.down
)");

    dc::KeymapRegistry reg;
    reg.loadFromYAML (path);
    removeTempFile (path);

    auto result = reg.feedKey (dc::VimModes::Normal, dc::VimContext::Editor,
                               'j', false, false, false, false);
    REQUIRE(result == "motion.down");
}

TEST_CASE("feedKey resets after failed match", "[vim][keymap_registry]")
{
    auto path = writeTempYAML (R"(
normal:
  editor:
    "gg": motion.file_start
    "j": motion.down
)");

    dc::KeymapRegistry reg;
    reg.loadFromYAML (path);
    removeTempFile (path);

    // Feed 'g' -> pending
    auto r1 = reg.feedKey (dc::VimModes::Normal, dc::VimContext::Editor,
                           'g', false, false, false, false);
    REQUIRE(r1 == "pending");

    // Feed 'x' -> no match (gx not bound), clears buffer
    auto r2 = reg.feedKey (dc::VimModes::Normal, dc::VimContext::Editor,
                           'x', false, false, false, false);
    REQUIRE(r2.empty());

    // Now 'j' should work fresh
    auto r3 = reg.feedKey (dc::VimModes::Normal, dc::VimContext::Editor,
                           'j', false, false, false, false);
    REQUIRE(r3 == "motion.down");
}

TEST_CASE("getKeybindingForAction reverse lookup", "[vim][keymap_registry]")
{
    auto path = writeTempYAML (R"(
normal:
  global:
    "Space": transport.play_stop
  editor:
    "gg": motion.file_start
)");

    dc::KeymapRegistry reg;
    reg.loadFromYAML (path);
    removeTempFile (path);

    REQUIRE(reg.getKeybindingForAction ("transport.play_stop",
                                         dc::VimModes::Normal,
                                         dc::VimContext::Editor) == "Space");

    REQUIRE(reg.getKeybindingForAction ("motion.file_start",
                                         dc::VimModes::Normal,
                                         dc::VimContext::Editor) == "gg");

    // Non-existent action returns empty
    REQUIRE(reg.getKeybindingForAction ("nonexistent",
                                         dc::VimModes::Normal,
                                         dc::VimContext::Editor).empty());
}

TEST_CASE("Visual mode bindings are resolved separately", "[vim][keymap_registry]")
{
    auto path = writeTempYAML (R"(
normal:
  global:
    "d": operator.delete
visual:
  global:
    "d": visual.delete
)");

    dc::KeymapRegistry reg;
    reg.loadFromYAML (path);
    removeTempFile (path);

    auto seq = dc::KeySequence::parse ("d");
    REQUIRE(reg.resolve (dc::VimModes::Normal, dc::VimContext::Editor, seq) == "operator.delete");
    REQUIRE(reg.resolve (dc::VimModes::Visual, dc::VimContext::Editor, seq) == "visual.delete");
}

TEST_CASE("Clear removes all bindings", "[vim][keymap_registry]")
{
    auto path = writeTempYAML (R"(
normal:
  global:
    "Space": transport.play_stop
)");

    dc::KeymapRegistry reg;
    reg.loadFromYAML (path);
    removeTempFile (path);

    REQUIRE(reg.getAllBindings().size() > 0);
    reg.clear();
    REQUIRE(reg.getAllBindings().empty());
}

TEST_CASE("resetFeed clears pending state", "[vim][keymap_registry]")
{
    auto path = writeTempYAML (R"(
normal:
  editor:
    "gg": motion.file_start
)");

    dc::KeymapRegistry reg;
    reg.loadFromYAML (path);
    removeTempFile (path);

    // Feed 'g' -> pending
    auto r1 = reg.feedKey (dc::VimModes::Normal, dc::VimContext::Editor,
                           'g', false, false, false, false);
    REQUIRE(r1 == "pending");

    // Reset and feed 'g' again -> pending (not 'gg')
    reg.resetFeed();
    auto r2 = reg.feedKey (dc::VimModes::Normal, dc::VimContext::Editor,
                           'g', false, false, false, false);
    REQUIRE(r2 == "pending");
}

TEST_CASE("Panel-specific feedKey prioritizes panel binding", "[vim][keymap_registry]")
{
    auto path = writeTempYAML (R"(
normal:
  global:
    "j": nav.down
  mixer:
    "j": mixer.focus_down
)");

    dc::KeymapRegistry reg;
    reg.loadFromYAML (path);
    removeTempFile (path);

    // From mixer panel, should get mixer-specific binding
    auto fromMixer = reg.feedKey (dc::VimModes::Normal, dc::VimContext::Mixer,
                                  'j', false, false, false, false);
    REQUIRE(fromMixer == "mixer.focus_down");

    // From editor panel, should get global binding
    auto fromEditor = reg.feedKey (dc::VimModes::Normal, dc::VimContext::Editor,
                                   'j', false, false, false, false);
    REQUIRE(fromEditor == "nav.down");
}

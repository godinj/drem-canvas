#include <catch2/catch_test_macros.hpp>
#include "vim/ActionRegistry.h"

// Helper: create a test action
static dc::ActionInfo makeAction (const std::string& id, const std::string& name,
                                  const std::string& category,
                                  std::vector<dc::VimContext::Panel> panels = {})
{
    dc::ActionInfo info;
    info.id = id;
    info.name = name;
    info.category = category;
    info.availablePanels = panels;
    return info;
}

TEST_CASE ("ActionRegistry registerAction and search", "[integration][action_registry]")
{
    dc::ActionRegistry registry;

    registry.registerAction (makeAction ("transport.play", "Play / Stop", "Transport"));
    registry.registerAction (makeAction ("transport.record", "Toggle Record", "Transport"));
    registry.registerAction (makeAction ("edit.undo", "Undo", "Edit"));
    registry.registerAction (makeAction ("edit.redo", "Redo", "Edit"));

    SECTION ("empty query returns all actions")
    {
        auto results = registry.search ("", dc::VimContext::Editor);
        CHECK (results.size() == 4);
    }

    SECTION ("search by exact prefix")
    {
        auto results = registry.search ("Play", dc::VimContext::Editor);
        REQUIRE (results.size() >= 1);
        CHECK (results[0].action->id == "transport.play");
        CHECK (results[0].score == 100); // exact prefix match
    }

    SECTION ("search by substring")
    {
        auto results = registry.search ("Record", dc::VimContext::Editor);
        REQUIRE (results.size() >= 1);
        CHECK (results[0].action->id == "transport.record");
    }

    SECTION ("search by initials")
    {
        // "PS" matches "Play / Stop" initials (P, S)
        auto results = registry.search ("ps", dc::VimContext::Editor);
        REQUIRE (results.size() >= 1);
        CHECK (results[0].action->id == "transport.play");
        CHECK (results[0].score == 80); // initials match
    }

    SECTION ("fuzzy character scatter")
    {
        // "uo" matches "Undo" (u...n...d...o -> "uo" scattered)
        auto results = registry.search ("uo", dc::VimContext::Editor);
        REQUIRE (results.size() >= 1);
        bool foundUndo = false;
        for (auto& r : results)
        {
            if (r.action->id == "edit.undo")
                foundUndo = true;
        }
        CHECK (foundUndo);
    }

    SECTION ("no match returns empty")
    {
        auto results = registry.search ("zzzzxxx", dc::VimContext::Editor);
        CHECK (results.empty());
    }
}

TEST_CASE ("ActionRegistry search filters by panel", "[integration][action_registry]")
{
    dc::ActionRegistry registry;

    registry.registerAction (makeAction ("global.action", "Global Action", "Global"));
    registry.registerAction (makeAction ("editor.zoom", "Zoom In", "Editor",
                                         { dc::VimContext::Editor }));
    registry.registerAction (makeAction ("mixer.mute", "Mute Track", "Mixer",
                                         { dc::VimContext::Mixer }));

    SECTION ("global actions available in all panels")
    {
        auto editorResults = registry.search ("", dc::VimContext::Editor);
        auto mixerResults = registry.search ("", dc::VimContext::Mixer);

        bool globalInEditor = false, globalInMixer = false;
        for (auto& r : editorResults)
            if (r.action->id == "global.action") globalInEditor = true;
        for (auto& r : mixerResults)
            if (r.action->id == "global.action") globalInMixer = true;

        CHECK (globalInEditor);
        CHECK (globalInMixer);
    }

    SECTION ("panel-specific actions only in their panel")
    {
        auto editorResults = registry.search ("", dc::VimContext::Editor);
        auto mixerResults = registry.search ("", dc::VimContext::Mixer);

        bool zoomInEditor = false, zoomInMixer = false;
        for (auto& r : editorResults)
            if (r.action->id == "editor.zoom") zoomInEditor = true;
        for (auto& r : mixerResults)
            if (r.action->id == "editor.zoom") zoomInMixer = true;

        CHECK (zoomInEditor);
        CHECK_FALSE (zoomInMixer);
    }
}

TEST_CASE ("ActionRegistry executeAction by ID", "[integration][action_registry]")
{
    dc::ActionRegistry registry;

    bool executed = false;
    dc::ActionInfo info = makeAction ("test.action", "Test", "Test");
    info.execute = [&executed] () { executed = true; };
    registry.registerAction (info);

    SECTION ("valid ID executes")
    {
        CHECK (registry.executeAction ("test.action"));
        CHECK (executed);
    }

    SECTION ("invalid ID returns false")
    {
        CHECK_FALSE (registry.executeAction ("nonexistent"));
        CHECK_FALSE (executed);
    }
}

TEST_CASE ("ActionRegistry removeActionsWithPrefix", "[integration][action_registry]")
{
    dc::ActionRegistry registry;

    registry.registerAction (makeAction ("plugin.1", "Plugin 1", "Plugins"));
    registry.registerAction (makeAction ("plugin.2", "Plugin 2", "Plugins"));
    registry.registerAction (makeAction ("edit.undo", "Undo", "Edit"));

    CHECK (registry.getAllActions().size() == 3);

    registry.removeActionsWithPrefix ("plugin.");
    CHECK (registry.getAllActions().size() == 1);
    CHECK (registry.getAllActions()[0].id == "edit.undo");
}

TEST_CASE ("ActionRegistry fuzzyScore known pairs", "[integration][action_registry]")
{
    dc::ActionRegistry registry;

    // We can't call fuzzyScore directly (it's private), but we can verify
    // the scoring through search results

    registry.registerAction (makeAction ("a", "Play / Stop", "Transport"));
    registry.registerAction (makeAction ("b", "Plugin Manager", "Plugins"));
    registry.registerAction (makeAction ("c", "Paste", "Edit"));

    SECTION ("exact prefix gets highest score")
    {
        auto results = registry.search ("Play", dc::VimContext::Editor);
        REQUIRE (results.size() >= 1);
        CHECK (results[0].score == 100);
        CHECK (results[0].action->name == "Play / Stop");
    }

    SECTION ("results are sorted by score descending")
    {
        auto results = registry.search ("P", dc::VimContext::Editor);
        // All three match "P" as prefix: Play, Plugin, Paste
        REQUIRE (results.size() >= 2);
        for (size_t i = 1; i < results.size(); ++i)
        {
            CHECK (results[i - 1].score >= results[i].score);
        }
    }

    SECTION ("contiguous substring match")
    {
        auto results = registry.search ("aste", dc::VimContext::Editor);
        REQUIRE (results.size() >= 1);
        CHECK (results[0].action->name == "Paste");
        CHECK (results[0].score == 60); // substring match
    }
}

TEST_CASE ("ActionRegistry empty registry returns empty results", "[integration][action_registry]")
{
    dc::ActionRegistry registry;

    auto results = registry.search ("anything", dc::VimContext::Editor);
    CHECK (results.empty());

    auto all = registry.search ("", dc::VimContext::Editor);
    CHECK (all.empty());
}

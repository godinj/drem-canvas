#pragma once

#include "VimContext.h"
#include <functional>
#include <string>
#include <vector>

namespace dc
{

struct ActionInfo
{
    std::string id;           // e.g. "transport.play_stop"
    std::string name;         // e.g. "Play / Stop"
    std::string category;     // e.g. "Transport"
    std::string keybinding;   // e.g. "Space"
    std::function<void()> execute;
    std::vector<VimContext::Panel> availablePanels; // empty = global
};

struct ScoredAction
{
    const ActionInfo* action = nullptr;
    int score = 0;
};

class ActionRegistry
{
public:
    void registerAction (ActionInfo info);

    std::vector<ScoredAction> search (const std::string& query,
                                      VimContext::Panel currentPanel) const;

    bool executeAction (const std::string& id) const;

    const std::vector<ActionInfo>& getAllActions() const { return actions; }

private:
    static int fuzzyScore (const std::string& query, const std::string& text);
    static bool matchesPanel (const ActionInfo& info, VimContext::Panel panel);

    std::vector<ActionInfo> actions;
};

} // namespace dc

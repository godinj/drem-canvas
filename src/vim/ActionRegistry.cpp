#include "ActionRegistry.h"
#include <algorithm>
#include <cctype>

namespace dc
{

void ActionRegistry::registerAction (ActionInfo info)
{
    actions.push_back (std::move (info));
}

std::vector<ScoredAction> ActionRegistry::search (const std::string& query,
                                                   VimContext::Panel currentPanel) const
{
    std::vector<ScoredAction> results;

    if (query.empty())
    {
        // Return all actions that match the current panel
        for (auto& action : actions)
        {
            if (matchesPanel (action, currentPanel))
                results.push_back ({ &action, 0 });
        }
        return results;
    }

    for (auto& action : actions)
    {
        if (! matchesPanel (action, currentPanel))
            continue;

        int nameScore = fuzzyScore (query, action.name);
        int catScore = fuzzyScore (query, action.category);
        int bestScore = std::max (nameScore, catScore / 2);

        if (bestScore > 0)
            results.push_back ({ &action, bestScore });
    }

    std::sort (results.begin(), results.end(),
               [] (const ScoredAction& a, const ScoredAction& b)
               {
                   if (a.score != b.score)
                       return a.score > b.score;
                   return a.action->name < b.action->name;
               });

    return results;
}

bool ActionRegistry::executeAction (const std::string& id) const
{
    for (auto& action : actions)
    {
        if (action.id == id && action.execute)
        {
            action.execute();
            return true;
        }
    }
    return false;
}

int ActionRegistry::fuzzyScore (const std::string& query, const std::string& text)
{
    if (query.empty() || text.empty())
        return 0;

    // Build lowercase versions
    std::string lowerQuery, lowerText;
    lowerQuery.reserve (query.size());
    lowerText.reserve (text.size());
    for (char c : query) lowerQuery += static_cast<char> (std::tolower (static_cast<unsigned char> (c)));
    for (char c : text)  lowerText  += static_cast<char> (std::tolower (static_cast<unsigned char> (c)));

    // 1. Exact prefix match → score 100
    if (lowerText.substr (0, lowerQuery.size()) == lowerQuery)
        return 100;

    // 2. Word-boundary initials match → score 80
    {
        std::string initials;
        bool prevWasBoundary = true;
        for (char c : lowerText)
        {
            if (c == ' ' || c == '/' || c == '_' || c == '-')
            {
                prevWasBoundary = true;
                continue;
            }
            if (prevWasBoundary)
            {
                initials += c;
                prevWasBoundary = false;
            }
            else
            {
                prevWasBoundary = false;
            }
        }

        if (! initials.empty() && initials.substr (0, lowerQuery.size()) == lowerQuery)
            return 80;
    }

    // 3. Contiguous substring match → score 60
    if (lowerText.find (lowerQuery) != std::string::npos)
        return 60;

    // 4. Ordered character scatter → score 20
    {
        size_t qi = 0;
        for (size_t ti = 0; ti < lowerText.size() && qi < lowerQuery.size(); ++ti)
        {
            if (lowerText[ti] == lowerQuery[qi])
                ++qi;
        }
        if (qi == lowerQuery.size())
            return 20;
    }

    return 0;
}

bool ActionRegistry::matchesPanel (const ActionInfo& info, VimContext::Panel panel)
{
    if (info.availablePanels.empty())
        return true;

    for (auto p : info.availablePanels)
    {
        if (p == panel)
            return true;
    }
    return false;
}

} // namespace dc

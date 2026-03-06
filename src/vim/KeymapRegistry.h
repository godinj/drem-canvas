#pragma once
#include "KeySequence.h"
#include "VimContext.h"
#include <string>
#include <vector>
#include <unordered_map>

namespace dc
{

// Forward declaration -- VimEngine::Mode lives in VimEngine.h
// Use an int alias to avoid circular dependency
using VimMode = int;

// Mode constants matching VimEngine::Mode
namespace VimModes
{
    constexpr int Normal     = 0;
    constexpr int Insert     = 1;
    constexpr int Command    = 2;
    constexpr int Keyboard   = 3;
    constexpr int PluginMenu = 4;
    constexpr int Visual     = 5;
    constexpr int VisualLine = 6;
}

class KeymapRegistry
{
public:
    struct Binding
    {
        KeySequence keys;
        std::string actionId;
        VimMode mode;
        VimContext::Panel panel;
        bool isGlobal;           // true = applies to all panels in this mode
    };

    // Load default keymap (embedded or from file)
    void loadFromYAML (const std::string& path);

    // Overlay user keymap on top of defaults (user bindings override)
    void overlayFromYAML (const std::string& path);

    // Resolve: given mode + panel + key sequence, return action ID (empty if no match)
    std::string resolve (VimMode mode, VimContext::Panel panel,
                         const KeySequence& seq) const;

    // Multi-step resolution: feed one key at a time, returns:
    //   - action ID if fully matched
    //   - "pending" if partial match (more keys needed)
    //   - empty string if no match
    std::string feedKey (VimMode mode, VimContext::Panel panel,
                         char32_t keyChar, bool shift, bool ctrl, bool alt, bool cmd);
    void resetFeed();

    // Reverse lookup: get keybinding display string for an action
    std::string getKeybindingForAction (const std::string& actionId,
                                        VimMode mode, VimContext::Panel panel) const;

    // Get all bindings (for debug/UI display)
    const std::vector<Binding>& getAllBindings() const { return bindings; }

    // Clear all bindings
    void clear();

private:
    std::vector<Binding> bindings;
    std::vector<KeySequence::Step> feedBuffer;

    void parseYAMLSection (const std::string& sectionName,
                           VimMode mode,
                           const void* yamlNode);

    static VimContext::Panel panelFromString (const std::string& name);
    static VimMode modeFromString (const std::string& name);
};

} // namespace dc

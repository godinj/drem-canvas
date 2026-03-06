#include "KeymapRegistry.h"
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <fstream>

namespace dc
{

// ── Panel / mode string mapping ──────────────────────────────────────────────

VimContext::Panel KeymapRegistry::panelFromString (const std::string& name)
{
    if (name == "editor")     return VimContext::Editor;
    if (name == "mixer")      return VimContext::Mixer;
    if (name == "sequencer")  return VimContext::Sequencer;
    if (name == "pianoroll")  return VimContext::PianoRoll;
    if (name == "pluginview") return VimContext::PluginView;
    return VimContext::Editor; // fallback
}

VimMode KeymapRegistry::modeFromString (const std::string& name)
{
    if (name == "normal")      return VimModes::Normal;
    if (name == "insert")      return VimModes::Insert;
    if (name == "command")     return VimModes::Command;
    if (name == "keyboard")    return VimModes::Keyboard;
    if (name == "pluginmenu")  return VimModes::PluginMenu;
    if (name == "visual")      return VimModes::Visual;
    if (name == "visual_line") return VimModes::VisualLine;

    // Also support section names used in YAML like "motions" and "operators"
    // which map to Normal mode
    if (name == "motions")     return VimModes::Normal;
    if (name == "operators")   return VimModes::Normal;

    return VimModes::Normal;
}

// ── YAML section parsing ─────────────────────────────────────────────────────

void KeymapRegistry::parseYAMLSection (const std::string& sectionName,
                                        VimMode mode,
                                        const void* yamlNodePtr)
{
    auto& node = *static_cast<const YAML::Node*> (yamlNodePtr);

    if (! node.IsMap())
        return;

    for (auto it = node.begin(); it != node.end(); ++it)
    {
        auto subName = it->first.as<std::string>();
        auto subNode = it->second;

        if (! subNode.IsMap())
            continue;

        bool isGlobal = (subName == "global");
        VimContext::Panel panel = isGlobal ? VimContext::Editor : panelFromString (subName);

        for (auto kvIt = subNode.begin(); kvIt != subNode.end(); ++kvIt)
        {
            auto keyStr = kvIt->first.as<std::string>();

            // Handle null values as unbind markers
            if (kvIt->second.IsNull())
                continue;

            auto actionId = kvIt->second.as<std::string>();

            Binding binding;
            binding.keys     = KeySequence::parse (keyStr);
            binding.actionId = actionId;
            binding.mode     = mode;
            binding.panel    = panel;
            binding.isGlobal = isGlobal;

            bindings.push_back (binding);
        }
    }
}

// ── Load from YAML ───────────────────────────────────────────────────────────

void KeymapRegistry::loadFromYAML (const std::string& path)
{
    bindings.clear();

    YAML::Node root = YAML::LoadFile (path);

    // Parse known mode sections
    static const std::vector<std::string> modeSections = {
        "normal", "insert", "command", "keyboard",
        "pluginmenu", "visual", "visual_line",
        "motions", "operators"
    };

    for (auto& section : modeSections)
    {
        if (root[section])
        {
            VimMode mode = modeFromString (section);
            const YAML::Node& sectionNode = root[section];
            parseYAMLSection (section, mode, &sectionNode);
        }
    }
}

// ── Overlay from YAML ────────────────────────────────────────────────────────

void KeymapRegistry::overlayFromYAML (const std::string& path)
{
    YAML::Node root = YAML::LoadFile (path);

    static const std::vector<std::string> modeSections = {
        "normal", "insert", "command", "keyboard",
        "pluginmenu", "visual", "visual_line",
        "motions", "operators"
    };

    for (auto& section : modeSections)
    {
        if (! root[section])
            continue;

        VimMode mode = modeFromString (section);
        auto sectionNode = root[section];

        if (! sectionNode.IsMap())
            continue;

        for (auto it = sectionNode.begin(); it != sectionNode.end(); ++it)
        {
            auto subName = it->first.as<std::string>();
            auto subNode = it->second;

            if (! subNode.IsMap())
                continue;

            bool isGlobal = (subName == "global");
            VimContext::Panel panel = isGlobal ? VimContext::Editor : panelFromString (subName);

            for (auto kvIt = subNode.begin(); kvIt != subNode.end(); ++kvIt)
            {
                auto keyStr = kvIt->first.as<std::string>();
                auto seq = KeySequence::parse (keyStr);

                // Remove existing binding with same mode + panel/global + keys
                bindings.erase (
                    std::remove_if (bindings.begin(), bindings.end(),
                        [&] (const Binding& b)
                        {
                            return b.mode == mode
                                && b.isGlobal == isGlobal
                                && (isGlobal || b.panel == panel)
                                && b.keys == seq;
                        }),
                    bindings.end());

                // If value is null, just unbind (don't add)
                if (kvIt->second.IsNull())
                    continue;

                auto actionId = kvIt->second.as<std::string>();

                Binding binding;
                binding.keys     = seq;
                binding.actionId = actionId;
                binding.mode     = mode;
                binding.panel    = panel;
                binding.isGlobal = isGlobal;

                bindings.push_back (binding);
            }
        }
    }
}

// ── Resolve ──────────────────────────────────────────────────────────────────

std::string KeymapRegistry::resolve (VimMode mode, VimContext::Panel panel,
                                      const KeySequence& seq) const
{
    // Panel-specific bindings take priority over global
    std::string globalMatch;

    for (auto& b : bindings)
    {
        if (b.mode != mode)
            continue;

        if (b.keys == seq)
        {
            if (! b.isGlobal && b.panel == panel)
                return b.actionId; // panel-specific wins immediately

            if (b.isGlobal && globalMatch.empty())
                globalMatch = b.actionId;
        }
    }

    return globalMatch;
}

// ── Feed key (multi-step resolution) ─────────────────────────────────────────

std::string KeymapRegistry::feedKey (VimMode mode, VimContext::Panel panel,
                                      char32_t keyChar, bool shift, bool ctrl,
                                      bool alt, bool cmd)
{
    KeySequence::Step step;
    step.character = keyChar;
    step.shift     = shift;
    step.control   = ctrl;
    step.alt       = alt;
    step.command   = cmd;

    feedBuffer.push_back (step);

    bool anyPartialMatch = false;
    std::string fullMatchAction;
    bool fullMatchIsPanelSpecific = false;

    for (auto& b : bindings)
    {
        if (b.mode != mode)
            continue;

        if (! b.isGlobal && b.panel != panel)
            continue;

        auto& bSteps = b.keys.steps;

        // Check if feedBuffer is a prefix of (or matches) this binding's steps
        if (feedBuffer.size() > bSteps.size())
            continue;

        bool prefixMatches = true;
        for (size_t i = 0; i < feedBuffer.size(); ++i)
        {
            auto& fed = feedBuffer[i];
            auto& expected = bSteps[i];

            // Build a keyChar for matching from the fed step
            bool stepMatches = expected.matches (
                fed.character, fed.shift, fed.control, fed.alt, fed.command);

            if (! stepMatches)
            {
                prefixMatches = false;
                break;
            }
        }

        if (! prefixMatches)
            continue;

        if (feedBuffer.size() == bSteps.size())
        {
            // Full match
            bool isPanelSpecific = (! b.isGlobal && b.panel == panel);

            if (isPanelSpecific)
            {
                fullMatchAction = b.actionId;
                fullMatchIsPanelSpecific = true;
            }
            else if (! fullMatchIsPanelSpecific && b.isGlobal)
            {
                fullMatchAction = b.actionId;
            }
        }
        else
        {
            // Partial match (more keys needed)
            anyPartialMatch = true;
        }
    }

    // If there's a full match AND no partial matches that could extend it,
    // return the action immediately
    if (! fullMatchAction.empty() && ! anyPartialMatch)
    {
        feedBuffer.clear();
        return fullMatchAction;
    }

    // If there are partial matches, we need more keys
    if (anyPartialMatch)
        return "pending";

    // If we have a full match but also had partial matches... return the full match
    if (! fullMatchAction.empty())
    {
        feedBuffer.clear();
        return fullMatchAction;
    }

    // No match at all
    feedBuffer.clear();
    return "";
}

void KeymapRegistry::resetFeed()
{
    feedBuffer.clear();
}

// ── Reverse lookup ───────────────────────────────────────────────────────────

std::string KeymapRegistry::getKeybindingForAction (const std::string& actionId,
                                                     VimMode mode,
                                                     VimContext::Panel panel) const
{
    // Prefer panel-specific binding
    for (auto& b : bindings)
    {
        if (b.actionId == actionId && b.mode == mode
            && ! b.isGlobal && b.panel == panel)
            return b.keys.toString();
    }

    // Fall back to global
    for (auto& b : bindings)
    {
        if (b.actionId == actionId && b.mode == mode && b.isGlobal)
            return b.keys.toString();
    }

    return "";
}

// ── Clear ────────────────────────────────────────────────────────────────────

void KeymapRegistry::clear()
{
    bindings.clear();
    feedBuffer.clear();
}

} // namespace dc

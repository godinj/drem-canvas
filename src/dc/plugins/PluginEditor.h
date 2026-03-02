#pragma once

namespace dc {

/// Stub for PluginEditor — will be replaced by Agent 04 (IPlugView lifecycle).
/// Exists so that std::unique_ptr<PluginEditor> is a complete type.
class PluginEditor
{
public:
    virtual ~PluginEditor() = default;
};

} // namespace dc

# Agent: UID Mapping & State Format Fix

You are working on the `master` branch of Drem Canvas, a C++17 DAW with VST3 plugin hosting.
Your task is fixing two related issues: `descriptionFromPropertyTree` setting `desc.uid`
to the file path instead of the actual VST3 class UID, and the legacy JUCE-format plugin
state failing to restore.

## Context

Read these before starting:
- `docs/plugin-loading-fixes/00-prd.md` (Issues 2, 3: state format and UID mapping)
- `src/plugins/PluginHost.cpp` (line 47: `descriptionFromPropertyTree` — the bug source)
- `src/dc/plugins/PluginDescription.h` (`hexStringToUid`, `uidToHexString`, `toMap`, `fromMap`)
- `src/dc/plugins/PluginInstance.cpp` (line 348: `create()` — class enumeration fallback; line 879: `setState`)
- `src/dc/plugins/VST3Module.h` (`getFactory()`, `getPath()`)
- `src/model/serialization/YAMLSerializer.cpp` (line 238: `parsePluginChain` — how plugins are stored in YAML)
- `src/ui/AppController.cpp` (line 1438: `rebuildAudioGraph` — how plugins are loaded from project)

Examine an actual project file to understand the stored format:
- `~/2PhaseP/track-0.yaml` or similar — look at the `plugins:` section for `unique_id`, `file_or_identifier`, `state` fields

## Dependencies

This agent depends on Agent 02 (PluginDescription tests). If those tests don't exist yet,
create stub tests alongside your fixes.

## Background

### UID Problem

`PluginHost::descriptionFromPropertyTree()` currently does:
```cpp
desc.uid  = pluginNode.getProperty (IDs::pluginFileOrIdentifier).getStringOr ("");
desc.path = pluginNode.getProperty (IDs::pluginFileOrIdentifier).getStringOr ("");
```

This sets both `uid` and `path` to the file path (e.g. `/usr/lib/vst3/Phase Plant.vst3`).
The `uid` should be the VST3 class UID hex string, but old projects store `unique_id`
as a JUCE-format integer (e.g. `-3838345`), which is also not a valid hex UID.

Current workaround: `PluginInstance::create()` has a class enumeration fallback that
finds the first `kVstAudioEffectClass` when `hexStringToUid()` fails. This works for
single-class plugins but is incorrect for multi-class bundles.

### Fix approach for UID

1. Change `descriptionFromPropertyTree` to properly map `pluginFileOrIdentifier` → `desc.path`
   (already done) and `pluginUniqueId` → `desc.uid` (integer or hex).
2. For legacy integer UIDs, keep the class enumeration fallback but log a warning.
3. When a plugin is successfully loaded via fallback, update the stored UID to the
   correct hex format so future loads use the proper UID.

### State Problem

Old projects store plugin state in JUCE's opaque binary format. The new `setState`
expects `[4 bytes componentSize][componentData][controllerData]`. JUCE wraps the state
differently — it prepends a header with the plugin UID and format version.

### Fix approach for state

1. Detect JUCE-format state by checking for JUCE's magic header bytes.
2. If detected, strip the JUCE wrapper and extract the raw VST3 component/controller state.
3. If the data doesn't match either format, log a warning and skip (don't crash).

## Deliverables

### Modified files

#### 1. `src/plugins/PluginHost.cpp`

Fix `descriptionFromPropertyTree`:

```cpp
dc::PluginDescription PluginHost::descriptionFromPropertyTree (const PropertyTree& pluginNode)
{
    dc::PluginDescription desc;
    desc.name         = pluginNode.getProperty (IDs::pluginName).getStringOr ("");
    desc.manufacturer = pluginNode.getProperty (IDs::pluginManufacturer).getStringOr ("");
    desc.path         = pluginNode.getProperty (IDs::pluginFileOrIdentifier).getStringOr ("");

    // Try to get UID from pluginUniqueId (may be integer from old JUCE format)
    auto uidVariant = pluginNode.getProperty (IDs::pluginUniqueId);
    auto uidStr = uidVariant.getStringOr ("");

    // If it looks like a valid 32-char hex UID, use it directly
    char testUid[16] = {};
    if (dc::PluginDescription::hexStringToUid (uidStr, testUid))
    {
        desc.uid = uidStr;
    }
    else
    {
        // Legacy integer UID or missing — leave uid empty,
        // PluginInstance::create() will use class enumeration fallback
        desc.uid = "";
    }

    return desc;
}
```

#### 2. `src/dc/plugins/PluginInstance.cpp`

Improve `setState` to detect and handle JUCE-format state:

In `setState()`, before the existing format parsing, add detection for JUCE's format.
JUCE's VST3 state format starts with a specific structure — you'll need to examine
actual JUCE-format state data from an old project to determine the exact header.

At minimum, add a bounds check and graceful failure:

```cpp
void PluginInstance::setState (const std::vector<uint8_t>& data)
{
    if (component_ == nullptr || data.size() < 4)
        return;

    uint32_t componentSize = 0;
    std::memcpy (&componentSize, data.data(), 4);

    // Sanity check: if componentSize is unreasonably large or doesn't fit,
    // this might be JUCE-format data — try raw pass-through
    if (4 + componentSize > data.size())
    {
        dc_log ("PluginInstance::setState: format mismatch (size=%zu, declared=%u) — "
                "attempting raw pass-through", data.size(), componentSize);

        // Try passing the entire blob as component state (some formats work this way)
        auto* stream = new MemoryStream (data);
        auto result = component_->setState (stream);
        stream->release();

        if (result == Steinberg::kResultOk)
        {
            dc_log ("PluginInstance::setState: raw pass-through succeeded");
            return;
        }

        dc_log ("PluginInstance::setState: raw pass-through also failed — "
                "state will not be restored (likely legacy JUCE format)");
        return;
    }

    // ... existing code continues unchanged ...
}
```

### New files

#### 3. `tests/regression/issue_001_uid_from_file_path.cpp`

Regression test verifying that `hexStringToUid` correctly rejects file paths
and that class enumeration fallback is triggered for invalid UIDs. Tag: `[regression]`.

```cpp
TEST_CASE ("hexStringToUid rejects file paths", "[regression][plugin_description]")
{
    char uid[16] = {};

    // File paths are not valid hex UIDs
    REQUIRE_FALSE (dc::PluginDescription::hexStringToUid (
        "/usr/lib/vst3/Phase Plant.vst3", uid));

    // JUCE integer UIDs are not valid hex UIDs
    REQUIRE_FALSE (dc::PluginDescription::hexStringToUid ("-3838345", uid));

    // Empty string is not valid
    REQUIRE_FALSE (dc::PluginDescription::hexStringToUid ("", uid));
}
```

#### 4. `tests/regression/issue_002_juce_state_format.cpp`

Regression test verifying that `setState` doesn't crash on JUCE-format data.
Tag: `[regression]`.

```cpp
TEST_CASE ("setState handles legacy JUCE-format data gracefully", "[regression]")
{
    // JUCE-format state data starts with a componentSize that doesn't match
    // the dc:: format expectation. Verify no crash.
    std::vector<uint8_t> juceFormatData = {
        0xFF, 0xFF, 0x00, 0x00,  // componentSize = 65535 (larger than data)
        0x01, 0x02, 0x03, 0x04   // some payload
    };

    // This would need a real PluginInstance to test — if we can't create one
    // in unit tests, document this as a manual test case.
    // The key assertion: PluginInstance::setState(juceFormatData) does NOT crash.
}
```

### Modified files

#### 5. `tests/CMakeLists.txt`

Add the two regression test files to the appropriate test target.

## Scope Limitation

Do not modify VST3Module, ProbeCache, or VST3Host. Focus on PluginHost.cpp
(UID mapping) and PluginInstance.cpp (state format). The class enumeration fallback
in `PluginInstance::create()` is already working — don't remove it.

## Conventions

- Namespace: `dc`
- JUCE coding style: spaces around operators, braces on new line for classes/functions
- Regression tests go in `tests/regression/` with `[regression]` tag
- Add all new `.cpp` files to `CMakeLists.txt` `target_sources`
- Build verification: `cmake --build --preset test`
- Run verification: `scripts/verify.sh`

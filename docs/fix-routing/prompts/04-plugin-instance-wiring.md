# Agent: Plugin Instance Routing Wiring

You are working on the `feature/fix-routing` branch of Drem Canvas, a C++17 DAW with Skia rendering and VST3 plugin hosting.
Your task is to wire the new `ProcessContextBuilder`, `ParameterChangeQueue`, and `MidiCCMapper` into `PluginInstance::process()` and connect the transport/tempo sync through `AppController`.

## Context

Read these before starting:
- `CLAUDE.md` (build commands, conventions, testing)
- `src/dc/plugins/PluginInstance.h` (class declaration: members, factory `create()`, `process()`, `prepare()`)
- `src/dc/plugins/PluginInstance.cpp` (full file — focus on `create()` ~line 376, `process()` ~line 622, `setupProcessing()` ~line 1117)
- `src/ui/AppController.cpp` (plugin instantiation ~line 1453-1499: where `pluginHost.createPluginSync()` is called; tempo sync ~line 2098-2111; initial load ~line 728)
- `src/ui/AppController.h` (members: `transportController`, `trackPluginChains`)
- `src/engine/TransportController.h` (after Agent 01: has `getTempo()`, `setTempo()`, `getTimeSigNumerator()`, etc.)

## Dependencies

This agent depends on:
- **Agent 01** (`01-process-context`): Provides `ProcessContextBuilder.h/.cpp` and `TransportController` with tempo/timeSig fields
- **Agent 02** (`02-parameter-changes`): Provides `ParameterChangeQueue.h/.cpp` and `MidiCCMapper.h/.cpp`

If those files don't exist yet, create stub headers with the interfaces described in their prompts and implement against them. The key APIs you need:

```cpp
// From Agent 01
namespace dc {
struct ProcessContextBuilder {
    static void populate (Steinberg::Vst::ProcessContext& ctx,
                          const TransportController& transport, int numSamples);
};
}

// From Agent 02
namespace dc {
class ParameterChangeQueue : public Steinberg::Vst::IParameterChanges { ... };
class MidiCCMapper {
    void buildFromController (Steinberg::Vst::IEditController* controller);
    bool translateToParameterChanges (const MidiMessage& msg, int sampleOffset,
                                      ParameterChangeQueue& queue) const;
};
}
```

## Deliverables

### Modified files

#### 1. `src/dc/plugins/PluginInstance.h`

Add new includes at the top:
```cpp
#include "dc/plugins/ParameterChangeQueue.h"
#include <memory>
```

Forward declare in namespace dc:
```cpp
class TransportController;
class MidiCCMapper;
```

Add new public method:
```cpp
void setTransportController (TransportController* transport);
```

Add new private members (after existing `bypassed_` member, ~line 122):
```cpp
TransportController* transport_ = nullptr;
Steinberg::Vst::ProcessContext processContext_ {};
ParameterChangeQueue inputParamChanges_;
std::unique_ptr<MidiCCMapper> midiCCMapper_;
```

#### 2. `src/dc/plugins/PluginInstance.cpp`

Add includes at the top:
```cpp
#include "dc/plugins/ProcessContextBuilder.h"
#include "dc/plugins/MidiCCMapper.h"
#include "engine/TransportController.h"
```

**Add setter implementation** (anywhere after existing methods):
```cpp
void PluginInstance::setTransportController (TransportController* transport)
{
    transport_ = transport;
}
```

**Modify `create()` method** (~line 557, after `buildParameterList()`):

Add MidiCCMapper initialization:
```cpp
instance->midiCCMapper_ = std::make_unique<MidiCCMapper>();
instance->midiCCMapper_->buildFromController (instance->controller_);
```

**Modify `process()` method** — three changes:

*Change 1*: Replace `processData_.processContext = nullptr;` (line 656) with:
```cpp
if (transport_ != nullptr)
{
    ProcessContextBuilder::populate (processContext_, *transport_, numSamples);
    processData_.processContext = &processContext_;
}
else
{
    processData_.processContext = nullptr;
}
```

*Change 2*: Replace `processData_.inputParameterChanges = nullptr;` (line 654) with:
```cpp
inputParamChanges_.clear();
processData_.inputParameterChanges = &inputParamChanges_;
```

*Change 3*: In the MIDI event loop, after the aftertouch `else if` block (after line 711) and before the closing comment (line 713), add:
```cpp
else if (msg.isController() && midiCCMapper_)
{
    midiCCMapper_->translateToParameterChanges (msg, event.sampleOffset, inputParamChanges_);
}
else if (msg.isPitchWheel() && midiCCMapper_)
{
    midiCCMapper_->translateToParameterChanges (msg, event.sampleOffset, inputParamChanges_);
}
else if (msg.isChannelPressure() && midiCCMapper_)
{
    midiCCMapper_->translateToParameterChanges (msg, event.sampleOffset, inputParamChanges_);
}
```

Remove or update the old comment on line 713-714:
```
// Note: CC / pitch bend / program change are handled via
// IParameterChanges in a full implementation. For now, skip them.
```
Replace with:
```
// Program change is not natively supported in VST3 — handled via IUnitInfo if needed.
```

#### 3. `src/ui/AppController.cpp`

**Wire transport to plugin instances during session load** (~line 1469, after `PluginHost::restorePluginState()` and before wrapping in `PluginProcessorNode`):

```cpp
instance->setTransportController (&transportController);
```

The existing code at this location is:
```cpp
if (instance != nullptr)
{
    std::string base64State = pluginState.getProperty (IDs::pluginState).getStringOr ("");
    if (! base64State.empty())
        PluginHost::restorePluginState (*instance, base64State);

    auto* pluginPtr = instance.get();
    auto wrapper = std::make_unique<PluginProcessorNode> (std::move (instance));
```

Insert `instance->setTransportController (&transportController);` after the state restore and before `auto* pluginPtr`.

**Sync tempo to TransportController on initial load** (~line 728, where `tempoMap.setTempo(project.getTempo())` is):

Add after that line:
```cpp
transportController.setTempo (project.getTempo());
```

**Sync tempo on property change** (~line 2100-2111, in the `onPropertyChanged` handler for `IDs::tempo`):

The existing code syncs tempo to sequencer and MIDI clip processors. Add transport sync alongside:
```cpp
transportController.setTempo (project.getTempo());
```

Add it right after `sequencerProcessor->setTempo(project.getTempo())` (line 2103).

**Also check**: Is there a time signature change handler? Search for `IDs::time_signature` or similar. If found, add:
```cpp
transportController.setTimeSig (project.getTimeSigNumerator(), project.getTimeSigDenominator());
```

If no time sig change handler exists, sync it during initial load alongside the tempo sync.

#### 4. `CMakeLists.txt` (root)

Verify that all new `.cpp` files from Agents 01 and 02 are present in `target_sources`. If any are missing, add them:
```
src/dc/plugins/ProcessContextBuilder.cpp
src/dc/plugins/ParameterChangeQueue.cpp
src/dc/plugins/MidiCCMapper.cpp
```

These should be in the main `DremCanvas` target's dc_plugins source group (~line 194-201).

## Audio Thread Safety

The changes to `PluginInstance::process()` run on the audio thread. Verify:
- `ProcessContextBuilder::populate()` only reads atomics from TransportController
- `inputParamChanges_.clear()` resets counters (no deallocation)
- `midiCCMapper_->translateToParameterChanges()` uses fixed arrays only
- No `new`, `delete`, `malloc`, `std::mutex`, `std::cout`, or `pthread_create`

## Conventions

- Namespace: `dc`
- Coding style: spaces around operators, braces on new line for classes/functions, `camelCase` methods, `PascalCase` classes
- Header includes use project-relative paths (e.g., `"dc/plugins/ProcessContextBuilder.h"`)
- Add all new `.cpp` files to `target_sources` in `CMakeLists.txt`
- Build verification: `cmake --build --preset release`
- Test verification: `cmake --preset test && cmake --build --preset test && ctest --test-dir build-debug --output-on-failure -j$(nproc)`
- Run `scripts/verify.sh` after all changes

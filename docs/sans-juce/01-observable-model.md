# 01 — Observable Model: PropertyTree

> Replaces `juce::ValueTree`, `juce::Identifier`, and `juce::var` with a custom
> observable hierarchical data model.

**Phase**: 1 (Model + Undo)
**Dependencies**: Phase 0 (Foundation Types)
**Related**: [07-undo-system.md](07-undo-system.md), [08-migration-guide.md](08-migration-guide.md)

---

## Overview

`juce::ValueTree` is the single source of truth for all model state in Drem Canvas.
Every Track, Clip, Plugin, and project setting is stored as a ValueTree node with
Identifier-keyed properties. GUI components observe changes via `ValueTree::Listener`.

The replacement must preserve:
1. **Hierarchical structure** — trees of typed nodes with named children
2. **Observable mutations** — listeners notified on property/child changes
3. **Undo integration** — all mutations optionally record undo actions
4. **Thread safety model** — mutations on message thread only, read from any thread
5. **YAML serialization** — round-trip through existing session format

## Current ValueTree Usage

### Hierarchy (from model/)

```
PROJECT
├── TRACKS
│   └── TRACK*
│       ├── AUDIO_CLIP* | MIDI_CLIP*
│       └── PLUGIN_CHAIN
│           └── PLUGIN*
├── MASTER_BUS
│   └── PLUGIN_CHAIN
│       └── PLUGIN*
└── STEP_SEQUENCER
    └── STEP_PATTERN*
        └── STEP_ROW*
            └── STEP*
```

### Property Types Used

| juce::var type | Example properties | Frequency |
|---------------|-------------------|-----------|
| `int` | startPosition, length, trimStart, noteNumber, velocity | Very high |
| `int64` | (via cast from int) | Medium |
| `double` | volume, pan, tempo, sampleRate | High |
| `bool` | mute, solo, armed, pluginEnabled | High |
| `String` | name, sourceFile, pluginName, pluginState (base64) | High |

### Listener Callbacks Used

| Callback | Used by |
|----------|---------|
| `valueTreePropertyChanged` | MainComponent, ChannelStrip, TrackLane, MixerPanel, PluginSlotList, StepGrid, all Widgets |
| `valueTreeChildAdded` | MainComponent, MixerPanel, PluginSlotList, ArrangementView |
| `valueTreeChildRemoved` | MainComponent, MixerPanel, PluginSlotList, ArrangementView |
| `valueTreeChildOrderChanged` | MixerPanel (track reorder) |
| `valueTreeParentChanged` | Rarely used |

---

## Design: `dc::PropertyTree`

### dc::PropertyId

Replaces `juce::Identifier`. String-interned for O(1) comparison.

```cpp
namespace dc {

class PropertyId
{
public:
    /// Construct from string literal (interned on first use)
    explicit PropertyId(const char* name);
    explicit PropertyId(std::string_view name);

    /// O(1) pointer comparison
    bool operator==(const PropertyId& other) const { return ptr_ == other.ptr_; }
    bool operator!=(const PropertyId& other) const { return ptr_ != other.ptr_; }
    bool operator<(const PropertyId& other) const { return ptr_ < other.ptr_; }

    /// Access the interned string
    const std::string& toString() const { return *ptr_; }

    /// Hash support (for unordered containers)
    struct Hash { size_t operator()(const PropertyId& id) const; };

private:
    const std::string* ptr_;  // points into global intern table

    static const std::string* intern(std::string_view name);
};

} // namespace dc
```

**Intern table**: `static std::unordered_set<std::string>` protected by
`std::shared_mutex` (write-locked on first use of each ID, then read-only).
All IDs are created at static init time in practice (`IDs::PROJECT`, etc.).

### dc::Variant

Replaces `juce::var`. Strongly-typed union with explicit type tags.

```cpp
namespace dc {

class Variant
{
public:
    enum class Type { Void, Int, Double, Bool, String, Binary };

    Variant();                              // Void
    Variant(int64_t v);
    Variant(int v);                         // promotes to int64_t
    Variant(double v);
    Variant(bool v);
    Variant(std::string v);
    Variant(std::string_view v);
    Variant(const char* v);
    Variant(std::vector<uint8_t> blob);     // Binary

    Type type() const;
    bool isVoid() const;

    // Accessors (throw dc::TypeMismatch if wrong type)
    int64_t     toInt() const;
    double      toDouble() const;
    bool        toBool() const;
    const std::string& toString() const;
    const std::vector<uint8_t>& toBinary() const;

    // Conversion accessors (best-effort, like juce::var)
    int64_t     getIntOr(int64_t fallback) const;
    double      getDoubleOr(double fallback) const;
    bool        getBoolOr(bool fallback) const;
    std::string getStringOr(std::string_view fallback) const;

    bool operator==(const Variant& other) const;
    bool operator!=(const Variant& other) const;

private:
    Type type_;
    std::variant<std::monostate, int64_t, double, bool,
                 std::string, std::vector<uint8_t>> value_;
};

} // namespace dc
```

### dc::PropertyTree

Replaces `juce::ValueTree`. Reference-counted shared tree node.

```cpp
namespace dc {

class PropertyTree
{
public:
    /// Create a new tree node with the given type
    explicit PropertyTree(PropertyId type);

    /// Invalid/empty tree (like default ValueTree)
    PropertyTree();
    bool isValid() const;

    // --- Type ---
    PropertyId getType() const;

    // --- Properties ---
    Variant getProperty(PropertyId name) const;
    Variant getProperty(PropertyId name, const Variant& fallback) const;
    bool hasProperty(PropertyId name) const;
    int getNumProperties() const;
    PropertyId getPropertyName(int index) const;

    /// Set property. If undoManager is non-null, records an UndoAction.
    void setProperty(PropertyId name, Variant value,
                     UndoManager* undoManager = nullptr);

    /// Remove property. Returns previous value.
    Variant removeProperty(PropertyId name,
                           UndoManager* undoManager = nullptr);

    // --- Children ---
    int getNumChildren() const;
    PropertyTree getChild(int index) const;
    PropertyTree getChildWithType(PropertyId type) const;
    PropertyTree getChildWithProperty(PropertyId prop,
                                      const Variant& value) const;
    int indexOf(const PropertyTree& child) const;

    void addChild(PropertyTree child, int index,
                  UndoManager* undoManager = nullptr);
    void removeChild(int index, UndoManager* undoManager = nullptr);
    void removeChild(PropertyTree child,
                     UndoManager* undoManager = nullptr);
    void removeAllChildren(UndoManager* undoManager = nullptr);
    void moveChild(int currentIndex, int newIndex,
                   UndoManager* undoManager = nullptr);

    // --- Parent ---
    PropertyTree getParent() const;

    // --- Listeners ---
    void addListener(Listener* listener);
    void removeListener(Listener* listener);

    // --- Comparison ---
    bool operator==(const PropertyTree& other) const;  // identity
    bool operator!=(const PropertyTree& other) const;

    // --- Deep copy ---
    PropertyTree createDeepCopy() const;

    // --- Iteration ---
    class Iterator;
    Iterator begin() const;
    Iterator end() const;

    // --- Listener interface ---
    class Listener
    {
    public:
        virtual ~Listener() = default;

        virtual void propertyChanged(PropertyTree& tree,
                                     PropertyId property) {}
        virtual void childAdded(PropertyTree& parent,
                                PropertyTree& child) {}
        virtual void childRemoved(PropertyTree& parent,
                                  PropertyTree& child,
                                  int index) {}
        virtual void childOrderChanged(PropertyTree& parent,
                                       int oldIndex, int newIndex) {}
        virtual void parentChanged(PropertyTree& tree) {}
    };

private:
    struct Data;
    std::shared_ptr<Data> data_;
};

} // namespace dc
```

### Internal Data Structure

```cpp
struct PropertyTree::Data
{
    PropertyId type;
    std::vector<std::pair<PropertyId, Variant>> properties;  // ordered
    std::vector<PropertyTree> children;
    PropertyTree::Data* parent = nullptr;  // raw, non-owning
    dc::ListenerList<Listener> listeners;

    // Listeners are called on the tree that changed AND on all ancestors
    // (bubble-up), matching ValueTree behavior.
};
```

**Reference counting**: `std::shared_ptr<Data>` means multiple `PropertyTree`
handles can point to the same node. Copy semantics are shallow (like ValueTree).
Use `createDeepCopy()` for structural copies.

---

## Listener Dispatch

Listeners are notified synchronously on the calling thread (always message thread
for mutations). Notifications bubble up: when a property changes on a child,
listeners on that child AND all ancestors are called.

This matches `juce::ValueTree::Listener` behavior, where a listener on the root
`PROJECT` tree receives all changes from any descendant.

```
setProperty("volume", 0.8) on TRACK node
  → TRACK listeners: propertyChanged(track, "volume")
  → TRACKS listeners: propertyChanged(track, "volume")
  → PROJECT listeners: propertyChanged(track, "volume")
```

## Thread Safety Model

- **Mutations**: Message thread only. No locking needed for the tree itself.
- **Reads**: Safe from any thread IF the caller accepts stale data. Audio thread
  reads atomic snapshots (see below).
- **Audio thread bridge**: Use `dc::SPSCQueue` to send snapshots of relevant
  properties to the audio thread, exactly as done today with `std::atomic`.

The audio thread never reads PropertyTree directly. Transport position, volume,
pan, mute/solo flags are mirrored to `std::atomic` members in engine processors,
synced via listener callbacks on the message thread.

## YAML Bridge

The existing `YAMLSerializer` / `SessionWriter` / `SessionReader` currently
operate on `juce::ValueTree`. The migration updates them to use `PropertyTree`:

| Current | New |
|---------|-----|
| `tree.getProperty(IDs::name).toString()` | `tree.getProperty(IDs::name).toString()` |
| `tree[IDs::volume]` | `tree.getProperty(IDs::volume)` |
| `tree.setProperty(IDs::x, val, nullptr)` | `tree.setProperty(IDs::x, Variant(val))` |
| `ValueTree(IDs::TRACK)` | `PropertyTree(IDs::TRACK)` |
| `tree.getChildWithName(IDs::TRACKS)` | `tree.getChildWithType(IDs::TRACKS)` |
| `tree.appendChild(child, &undoManager)` | `tree.addChild(child, -1, &undoManager)` |

The API is intentionally similar to minimize migration effort. Most call sites
require only a type rename and minor accessor adjustments.

## IDs Namespace

The existing `IDs` namespace with `juce::Identifier` constants maps directly:

```cpp
// Before (model/Project.h or similar)
namespace IDs
{
    const juce::Identifier PROJECT    ("PROJECT");
    const juce::Identifier TRACKS     ("TRACKS");
    const juce::Identifier TRACK      ("TRACK");
    const juce::Identifier name       ("name");
    const juce::Identifier volume     ("volume");
    // ...
}

// After
namespace IDs
{
    const dc::PropertyId PROJECT    ("PROJECT");
    const dc::PropertyId TRACKS     ("TRACKS");
    const dc::PropertyId TRACK      ("TRACK");
    const dc::PropertyId name       ("name");
    const dc::PropertyId volume     ("volume");
    // ...
}
```

## Integration with Undo

Every mutation method accepts an optional `dc::UndoManager*`. When non-null,
the mutation creates a `dc::UndoAction` and adds it to the manager before
applying the change. See [07-undo-system.md](07-undo-system.md) for the
undo action types:

- `PropertyChangeAction` — records old/new value for `setProperty` / `removeProperty`
- `ChildAddAction` — records child and index for `addChild`
- `ChildRemoveAction` — records child, index, and deep copy for `removeChild`
- `ChildMoveAction` — records old/new index for `moveChild`

## Migration Checklist

### Files to migrate (model/)

| File | Key changes |
|------|------------|
| `Project.h/.cpp` | `ValueTree` → `PropertyTree`, `Identifier` → `PropertyId` |
| `Track.h` | Static helper methods, property access |
| `Arrangement.h` | Track list management |
| `AudioClip.h` | Clip property access, factory methods |
| `MidiClip.h/.cpp` | MidiMessageSequence encoding (move to dc::MidiSequence) |
| `StepSequencer.h` | Pattern/row/step tree structure |
| `MixerState.h` | Master bus properties |
| `TempoMap.h` | Tempo/time-sig properties |
| `Clipboard.h` | Deep copy of subtrees |
| `GridSystem.h` | Grid properties |
| `RecentProjects.h` | Recent file list |
| `serialization/*.cpp` | YAML read/write |

### Files to migrate (gui/ and ui/)

All `ValueTree::Listener` subclasses switch to `PropertyTree::Listener`.
The callback signatures are identical in shape, so changes are mechanical.

| Pattern | Count | Change |
|---------|-------|--------|
| `valueTreePropertyChanged` | ~25 | → `propertyChanged` |
| `valueTreeChildAdded` | ~10 | → `childAdded` |
| `valueTreeChildRemoved` | ~10 | → `childRemoved` |
| `valueTreeChildOrderChanged` | ~3 | → `childOrderChanged` |
| `valueTreeParentChanged` | ~2 | → `parentChanged` |

## Testing Strategy

1. **Unit tests**: PropertyTree CRUD, listener dispatch, deep copy, identity semantics
2. **Round-trip test**: Serialize PropertyTree → YAML → PropertyTree, assert equality
3. **Undo test**: Mutate → undo → assert original state restored
4. **Performance benchmark**: Create 1000-node tree, mutate 10k properties, measure
   time vs juce::ValueTree baseline

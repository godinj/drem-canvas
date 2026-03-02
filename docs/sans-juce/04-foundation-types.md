# 04 — Foundation Types: Type Mapping Reference

> Maps every `juce::` foundation type to its `std::` or `dc::` replacement.
> Covers `String`, `File`, `Array`, `Colour`, macros, threading primitives,
> and utility functions.

**Phase**: 0 (Foundation Sweep)
**Dependencies**: None (this is the first phase)
**Related**: [00-prd.md](00-prd.md), [08-migration-guide.md](08-migration-guide.md)

---

## Overview

JUCE provides a complete standard library alternative. Most JUCE types have
direct `std::` equivalents. A few require thin `dc::` wrappers.

Phase 0 replaces all JUCE foundation types across the codebase. This is a
prerequisite for every other phase.

---

## Complete Type Mapping Table

### Strings & Text

| JUCE type | Replacement | Notes |
|-----------|-------------|-------|
| `juce::String` | `std::string` | UTF-8 everywhere. Use `std::string_view` for non-owning. |
| `juce::StringArray` | `std::vector<std::string>` | |
| `juce::CharPointer_UTF8` | `const char*` / `std::string_view` | |
| `juce::String::toStdString()` | identity | Already `std::string` |
| `juce::String::toUTF8()` | `.c_str()` or `.data()` | |
| `juce::String::formatted(...)` | `std::format(...)` (C++20) or `dc::format(...)` (snprintf wrapper) | |
| `juce::String::toInt()` | `std::stoi()` / `std::from_chars()` | |
| `juce::String::toFloat()` | `std::stof()` / `std::from_chars()` | |
| `juce::String::isEmpty()` | `.empty()` | |
| `juce::String::contains()` | `.find() != npos` or `dc::contains()` | |
| `juce::String::replace()` | `dc::replace()` helper | |
| `juce::String::trim()` | `dc::trim()` helper | |
| `juce::newLine` | `"\n"` | |

### File System

| JUCE type | Replacement | Notes |
|-----------|-------------|-------|
| `juce::File` | `std::filesystem::path` | |
| `juce::File::getFullPathName()` | `.string()` | |
| `juce::File::getFileName()` | `.filename().string()` | |
| `juce::File::getParentDirectory()` | `.parent_path()` | |
| `juce::File::exists()` | `std::filesystem::exists(p)` | |
| `juce::File::isDirectory()` | `std::filesystem::is_directory(p)` | |
| `juce::File::getChildFile(name)` | `p / name` | |
| `juce::File::createDirectory()` | `std::filesystem::create_directories(p)` | |
| `juce::File::deleteFile()` | `std::filesystem::remove(p)` | |
| `juce::File::moveFileTo(dest)` | `std::filesystem::rename(src, dest)` | |
| `juce::File::copyFileTo(dest)` | `std::filesystem::copy(src, dest)` | |
| `juce::File::loadFileAsString()` | `dc::readFileToString(p)` | |
| `juce::File::replaceWithText(s)` | `dc::writeStringToFile(p, s)` | |
| `juce::File::getFileExtension()` | `.extension().string()` | |
| `juce::File::withFileExtension(e)` | `p.replace_extension(e)` | |
| `juce::File::getSpecialLocation(...)` | Platform-specific (see below) | |
| `juce::FileInputStream` | `std::ifstream` | |
| `juce::FileOutputStream` | `std::ofstream` | |

**Special locations**:
```cpp
namespace dc {
    std::filesystem::path getUserHomeDirectory();      // getenv("HOME")
    std::filesystem::path getUserAppDataDirectory();   // ~/.config/DremCanvas (Linux), ~/Library/Application Support/DremCanvas (macOS)
    std::filesystem::path getTempDirectory();          // std::filesystem::temp_directory_path()
}
```

### Containers

| JUCE type | Replacement | Notes |
|-----------|-------------|-------|
| `juce::Array<T>` | `std::vector<T>` | |
| `juce::OwnedArray<T>` | `std::vector<std::unique_ptr<T>>` | |
| `juce::ReferenceCountedArray<T>` | `std::vector<std::shared_ptr<T>>` | |
| `juce::HashMap<K,V>` | `std::unordered_map<K,V>` | |
| `juce::SortedSet<T>` | `std::set<T>` | |
| `juce::LinkedListPointer<T>` | `std::list<T>` or `std::forward_list<T>` | |
| `juce::NamedValueSet` | `std::unordered_map<std::string, dc::Variant>` | Or use PropertyTree |
| `juce::StringPairArray` | `std::vector<std::pair<std::string, std::string>>` | |

### Colours

| JUCE type | Replacement | Notes |
|-----------|-------------|-------|
| `juce::Colour` | `dc::Colour` or `uint32_t` (ARGB) | |
| `juce::Colour(0xAARRGGBB)` | `dc::Colour(0xAARRGGBB)` | |
| `juce::Colours::white` | `dc::Colours::white` | |
| `juce::Colour::fromFloatRGBA(r,g,b,a)` | `dc::Colour::fromFloat(r,g,b,a)` | |
| `colour.getFloatRed()` etc. | Same API | |

```cpp
namespace dc {

struct Colour
{
    uint32_t argb;  // 0xAARRGGBB

    constexpr Colour() : argb(0xff000000) {}
    constexpr explicit Colour(uint32_t value) : argb(value) {}

    static Colour fromFloat(float r, float g, float b, float a = 1.0f);
    static Colour fromRGB(uint8_t r, uint8_t g, uint8_t b);
    static Colour fromHSV(float h, float s, float v, float a = 1.0f);

    uint8_t getAlpha() const;
    uint8_t getRed() const;
    uint8_t getGreen() const;
    uint8_t getBlue() const;
    float getFloatAlpha() const;
    float getFloatRed() const;
    float getFloatGreen() const;
    float getFloatBlue() const;

    Colour withAlpha(float a) const;
    Colour brighter(float amount = 0.4f) const;
    Colour darker(float amount = 0.4f) const;

    /// Convert to SkColor (for Skia rendering)
    uint32_t toSkColor() const;

    /// Convert to/from hex string (e.g., "ff252535")
    std::string toHexString() const;
    static Colour fromHexString(std::string_view hex);

    bool operator==(const Colour& other) const { return argb == other.argb; }
    bool operator!=(const Colour& other) const { return argb != other.argb; }
};

namespace Colours
{
    constexpr Colour black{0xff000000};
    constexpr Colour white{0xffffffff};
    constexpr Colour red{0xffff0000};
    constexpr Colour green{0xff00ff00};
    constexpr Colour blue{0xff0000ff};
    constexpr Colour transparentBlack{0x00000000};
    // ... add as needed
}

} // namespace dc
```

**Note**: The graphics engine already uses Skia colors internally. `dc::Colour`
is primarily for the model layer (track colors) and theme system.

### Time & Timing

| JUCE type | Replacement | Notes |
|-----------|-------------|-------|
| `juce::Time::currentTimeMillis()` | `dc::currentTimeMillis()` | See below |
| `juce::Time::getMillisecondCounterHiRes()` | `dc::hiResTimeMs()` | |
| `juce::Time::getHighResolutionTicks()` | `std::chrono::high_resolution_clock` | |
| `juce::RelativeTime` | `std::chrono::duration` | |
| `juce::Timer` | `dc::Timer` (already exists in graphics engine) | |

```cpp
namespace dc {

inline int64_t currentTimeMillis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

inline double hiResTimeMs()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(
        high_resolution_clock::now().time_since_epoch()).count() / 1000.0;
}

} // namespace dc
```

### Threading

| JUCE type | Replacement | Notes |
|-----------|-------------|-------|
| `juce::Thread` | `std::thread` | |
| `juce::CriticalSection` | `std::mutex` | |
| `juce::ScopedLock` | `std::lock_guard<std::mutex>` | |
| `juce::ReadWriteLock` | `std::shared_mutex` | |
| `juce::ScopedReadLock` | `std::shared_lock<std::shared_mutex>` | |
| `juce::ScopedWriteLock` | `std::unique_lock<std::shared_mutex>` | |
| `juce::WaitableEvent` | `std::condition_variable` + `std::mutex` | |
| `juce::TimeSliceThread` | `std::thread` + work queue | Or `dc::WorkerThread` |
| `juce::ThreadPool` | `dc::ThreadPool` (simple implementation) | |
| `std::atomic<T>` | `std::atomic<T>` (unchanged) | |
| `juce::SpinLock` | `dc::SpinLock` (atomic flag) | |

### Memory

| JUCE type | Replacement | Notes |
|-----------|-------------|-------|
| `juce::MemoryBlock` | `std::vector<uint8_t>` | |
| `juce::MemoryOutputStream` | `std::vector<uint8_t>` + write helpers | |
| `juce::MemoryInputStream` | `const uint8_t*` + read helpers | |
| `juce::ReferenceCountedObject` | Use `std::shared_ptr` | |
| `juce::ScopedPointer` | `std::unique_ptr` (already deprecated in JUCE) | |
| `juce::HeapBlock` | `std::vector<T>` or `std::unique_ptr<T[]>` | |

### Math & Random

| JUCE type | Replacement | Notes |
|-----------|-------------|-------|
| `juce::Random::getSystemRandom()` | `dc::randomFloat()` / `dc::randomInt()` | Thread-local `mt19937` |
| `juce::jmin(a, b)` | `std::min(a, b)` | |
| `juce::jmax(a, b)` | `std::max(a, b)` | |
| `juce::jlimit(lo, hi, val)` | `std::clamp(val, lo, hi)` | |
| `juce::roundToInt(x)` | `static_cast<int>(std::round(x))` or `dc::roundToInt(x)` | |
| `juce::MathConstants<T>::pi` | `std::numbers::pi_v<T>` (C++20) or `dc::pi<T>` | |
| `juce::MathConstants<T>::twoPi` | `2 * std::numbers::pi_v<T>` | |
| `juce::approximatelyEqual(a, b)` | `dc::approxEqual(a, b, epsilon)` | |

```cpp
namespace dc {

inline int roundToInt(float x) { return static_cast<int>(std::round(x)); }
inline int roundToInt(double x) { return static_cast<int>(std::round(x)); }

template<typename T>
constexpr T pi = T(3.14159265358979323846);

inline float randomFloat()
{
    thread_local std::mt19937 gen{std::random_device{}()};
    thread_local std::uniform_real_distribution<float> dist(0.0f, 1.0f);
    return dist(gen);
}

inline int randomInt(int min, int max)
{
    thread_local std::mt19937 gen{std::random_device{}()};
    std::uniform_int_distribution<int> dist(min, max);
    return dist(gen);
}

} // namespace dc
```

### Base64

| JUCE type | Replacement | Notes |
|-----------|-------------|-------|
| `juce::Base64::convertToBase64(data, size)` | `dc::base64Encode(data)` | |
| `juce::Base64::convertFromBase64(output, str)` | `dc::base64Decode(str)` | |
| `memoryBlock.toBase64Encoding()` | `dc::base64Encode(vec)` | |
| `memoryBlock.fromBase64Encoding(str)` | `dc::base64Decode(str)` | |

```cpp
namespace dc {
    std::string base64Encode(const std::vector<uint8_t>& data);
    std::string base64Encode(const uint8_t* data, size_t size);
    std::vector<uint8_t> base64Decode(std::string_view encoded);
}
```

### XML (Legacy)

| JUCE type | Replacement | Notes |
|-----------|-------------|-------|
| `juce::XmlElement` | Not needed (YAML-only) | Used only for KnownPluginList persistence |
| `juce::parseXML(str)` | Not needed | |
| `juce::ValueTree::toXmlString()` | Not needed (use PropertyTree YAML) | |
| `juce::ValueTree::fromXml(xml)` | Not needed | |

XML is only used for `KnownPluginList` persistence (JUCE plugin database).
The replacement uses YAML or JSON directly. No XML library needed.

---

## `dc::MessageQueue`

Replaces `juce::MessageManager::callAsync()`. Provides a thread-safe way to
post callbacks to the message (main) thread.

```cpp
namespace dc {

class MessageQueue
{
public:
    /// Post a callback to be executed on the message thread
    void post(std::function<void()> fn);

    /// Process all pending callbacks (called from message thread event loop)
    void processAll();

    /// Number of pending callbacks
    size_t pending() const;

private:
    std::mutex mutex_;
    std::vector<std::function<void()>> queue_;
    std::vector<std::function<void()>> processing_;  // swap buffer
};

} // namespace dc
```

**Integration**: The platform event loop (Cocoa run loop on macOS, GLFW poll
on Linux) calls `messageQueue.processAll()` on each iteration.

```cpp
// In Main.cpp event loop:
while (running)
{
    glfwPollEvents();          // or Cocoa event dispatch
    messageQueue.processAll(); // process async callbacks
    renderer.renderFrame();
}
```

---

## `dc::ListenerList<T>`

Simple listener container replacing `juce::ListenerList`.

```cpp
namespace dc {

template<typename ListenerType>
class ListenerList
{
public:
    void add(ListenerType* listener);
    void remove(ListenerType* listener);

    /// Call a method on all listeners
    template<typename Callback>
    void call(Callback&& callback)
    {
        // Iterate a copy to allow listeners to remove themselves
        auto copy = listeners_;
        for (auto* l : copy)
            if (l) callback(*l);
    }

    int size() const;
    bool isEmpty() const;

private:
    std::vector<ListenerType*> listeners_;
};

} // namespace dc
```

---

## Macro Replacements

| JUCE macro | Replacement | Notes |
|------------|-------------|-------|
| `jassert(expr)` | `dc_assert(expr)` | `assert(expr)` in debug, no-op in release |
| `jassertfalse` | `dc_assert(false)` | |
| `DBG(msg)` | `dc_log(msg)` | `fprintf(stderr, ...)` or structured logging |
| `JUCE_DECLARE_NON_COPYABLE(ClassName)` | Delete copy ctor/assign directly | |
| `JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ClassName)` | Delete copy ctor/assign | Leak detector not needed |
| `START_JUCE_APPLICATION(AppClass)` | Custom `main()` function | |
| `JUCE_APPLICATION_NAME_STRING` | `DC_APP_NAME` (from config.h) | |
| `JUCE_APPLICATION_VERSION_STRING` | `DC_APP_VERSION` (from config.h) | |

```cpp
// dc/foundation/assert.h
#ifdef NDEBUG
    #define dc_assert(expr) ((void)0)
#else
    #define dc_assert(expr) assert(expr)
#endif

// dc/foundation/log.h
#define dc_log(msg) fprintf(stderr, "[DC] %s\n", (msg))
// Or: structured logging to file with timestamp
```

---

## CMake-Generated `config.h`

Replace JUCE compile definitions with a CMake-generated header:

```cmake
# CMakeLists.txt
configure_file(
    "${CMAKE_SOURCE_DIR}/src/config.h.in"
    "${CMAKE_BINARY_DIR}/generated/config.h"
)
target_include_directories(DremCanvas PRIVATE "${CMAKE_BINARY_DIR}/generated")
```

```cpp
// src/config.h.in
#pragma once

#define DC_APP_NAME "@CMAKE_PROJECT_NAME@"
#define DC_APP_VERSION "@CMAKE_PROJECT_VERSION@"

// Platform detection
#if defined(__APPLE__)
    #define DC_MAC 1
    #define DC_LINUX 0
#elif defined(__linux__)
    #define DC_MAC 0
    #define DC_LINUX 1
#endif
```

---

## `dc::WorkerThread`

Simple worker thread for background tasks. Replaces `juce::TimeSliceThread`.

```cpp
namespace dc {

class WorkerThread
{
public:
    explicit WorkerThread(const std::string& name);
    ~WorkerThread();

    /// Submit a task to the worker thread
    void submit(std::function<void()> task);

    /// Stop the thread (waits for current task to finish)
    void stop();

    bool isRunning() const;

private:
    std::thread thread_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::queue<std::function<void()>> tasks_;
    std::atomic<bool> running_{true};
    std::string name_;

    void run();
};

} // namespace dc
```

---

## Application Entry Point

Replace `START_JUCE_APPLICATION(DremCanvasApplication)`:

```cpp
// Before (Main.cpp)
class DremCanvasApplication : public juce::JUCEApplication { ... };
START_JUCE_APPLICATION(DremCanvasApplication)

// After (Main.cpp)
int main(int argc, char* argv[])
{
    dc::Application app;
    return app.run(argc, argv);
}
```

`dc::Application` handles:
- Platform initialization (PortAudio, RtMidi)
- Window creation (NativeWindow on macOS, GlfwWindow on Linux)
- Event loop (Cocoa run loop / GLFW poll loop)
- MessageQueue integration
- Shutdown and cleanup

---

## File Helpers

Small utility functions replacing scattered JUCE file operations:

```cpp
namespace dc {

/// Read entire file to string
std::string readFileToString(const std::filesystem::path& path);

/// Write string to file (atomic: write to .tmp, then rename)
bool writeStringToFile(const std::filesystem::path& path,
                       std::string_view content);

/// Check if path is absolute
bool isAbsolutePath(const std::filesystem::path& path);

/// Resolve relative path against a base directory
std::filesystem::path resolvePath(const std::filesystem::path& base,
                                   const std::filesystem::path& relative);

} // namespace dc
```

---

## Migration Strategy

### Step 1: Create `dc/foundation/` header library

Create headers:
- `dc/foundation/types.h` — Colour, roundToInt, pi, random
- `dc/foundation/string_utils.h` — trim, replace, contains, format
- `dc/foundation/file_utils.h` — readFileToString, writeStringToFile, special locations
- `dc/foundation/assert.h` — dc_assert, dc_log
- `dc/foundation/time.h` — currentTimeMillis, hiResTimeMs
- `dc/foundation/base64.h` — encode/decode
- `dc/foundation/message_queue.h` — MessageQueue
- `dc/foundation/listener_list.h` — ListenerList<T>
- `dc/foundation/spsc_queue.h` — SPSCQueue<T>
- `dc/foundation/worker_thread.h` — WorkerThread

### Step 2: Mechanical type replacement

Global find-and-replace across all files:
1. `juce::String` → `std::string`
2. `juce::File` → `std::filesystem::path`
3. `juce::Array<T>` → `std::vector<T>`
4. `juce::OwnedArray<T>` → `std::vector<std::unique_ptr<T>>`
5. `juce::CriticalSection` → `std::mutex`
6. `juce::ScopedLock` → `std::lock_guard<std::mutex>`
7. `juce::MemoryBlock` → `std::vector<uint8_t>`
8. `juce::Colour` → `dc::Colour`
9. `jassert` → `dc_assert`
10. `DBG` → `dc_log`

### Step 3: Fix compilation errors

Each replacement may require API adjustments (e.g., `String::toStdString()`
becomes identity, `File::getFullPathName()` becomes `.string()`).

### Step 4: Update `#include` directives

Replace `#include <JuceHeader.h>` with specific dc/ and std headers.

### Step 5: Update CMakeLists.txt

Add `dc/foundation/` to `target_sources`. Add `config.h` generation.

---

## Files Affected

Every source file that uses `juce::String`, `juce::File`, etc. — which is
nearly all of them. See [08-migration-guide.md](08-migration-guide.md) for
the complete file-by-file list.

**Highest-impact files** (most foundation type usage):
- `src/model/Project.h/.cpp`
- `src/model/Track.h`
- `src/model/serialization/*.cpp`
- `src/plugins/PluginManager.h/.cpp`
- `src/plugins/PluginHost.h/.cpp`
- `src/engine/AudioEngine.h/.cpp`
- `src/vim/VimEngine.h/.cpp`
- `src/Main.cpp`

## Testing Strategy

1. **Compilation**: Each replacement step must compile cleanly
2. **String operations**: Unit tests for dc::trim, dc::replace, dc::contains, dc::format
3. **File operations**: Read/write round-trip, special location resolution
4. **Base64**: Encode/decode known data, compare against JUCE output
5. **MessageQueue**: Post from worker thread, verify execution on main thread
6. **ListenerList**: Add/remove/call pattern, verify re-entrant safety
7. **SPSCQueue**: Multi-threaded push/pop, verify ordering and completeness
8. **Colour**: Construct from various formats, verify component extraction
9. **Integration**: Full app launch after all replacements

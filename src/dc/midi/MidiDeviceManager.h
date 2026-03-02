#pragma once

#include "dc/midi/MidiMessage.h"

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

class RtMidiIn;
class RtMidiOut;

namespace dc {

/// MIDI device information
struct MidiDeviceInfo
{
    int index;
    std::string name;
    bool isInput;
};

/// Callback for incoming MIDI messages.
/// Called on the RtMidi callback thread — implementations must be thread-safe.
class MidiInputCallback
{
public:
    virtual ~MidiInputCallback() = default;
    virtual void handleMidiMessage (const MidiMessage& msg,
                                     double timestamp) = 0;
};

/// RtMidi wrapper for hardware MIDI I/O.
/// Replaces juce::MidiInput / juce::MidiOutput device management.
class MidiDeviceManager
{
public:
    MidiDeviceManager();
    ~MidiDeviceManager();

    /// Enumerate available input devices
    std::vector<MidiDeviceInfo> getInputDevices() const;

    /// Enumerate available output devices
    std::vector<MidiDeviceInfo> getOutputDevices() const;

    /// Open an input device and start receiving messages.
    /// The callback is invoked on the RtMidi thread.
    bool openInput (int deviceIndex, MidiInputCallback* callback);

    /// Close an input device
    void closeInput (int deviceIndex);

    /// Open an output device
    bool openOutput (int deviceIndex);

    /// Send a message to an output device
    void sendMessage (int deviceIndex, const MidiMessage& msg);

    /// Close an output device
    void closeOutput (int deviceIndex);

    /// Close all open devices
    void closeAll();

private:
    struct InputPort
    {
        std::unique_ptr<RtMidiIn> rtMidi;
        MidiInputCallback* callback = nullptr;
    };

    struct OutputPort
    {
        std::unique_ptr<RtMidiOut> rtMidi;
    };

    std::unordered_map<int, InputPort> inputs_;
    std::unordered_map<int, OutputPort> outputs_;

    /// RtMidi callback — bridges raw bytes to dc::MidiMessage
    static void rtMidiCallback (double timeStamp,
                                 std::vector<unsigned char>* message,
                                 void* userData);

    MidiDeviceManager (const MidiDeviceManager&) = delete;
    MidiDeviceManager& operator= (const MidiDeviceManager&) = delete;
};

} // namespace dc

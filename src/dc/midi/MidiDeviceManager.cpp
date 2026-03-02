#include "dc/midi/MidiDeviceManager.h"
#include "dc/foundation/assert.h"

#include <RtMidi.h>

namespace dc {

MidiDeviceManager::MidiDeviceManager() = default;

MidiDeviceManager::~MidiDeviceManager()
{
    closeAll();
}

std::vector<MidiDeviceInfo> MidiDeviceManager::getInputDevices() const
{
    std::vector<MidiDeviceInfo> result;

    try
    {
        RtMidiIn probe;
        unsigned int count = probe.getPortCount();

        for (unsigned int i = 0; i < count; ++i)
            result.push_back ({ static_cast<int> (i), probe.getPortName (i), true });
    }
    catch (const RtMidiError& e)
    {
        fprintf (stderr, "[DC] RtMidi error enumerating inputs: %s\n", e.what());
    }

    return result;
}

std::vector<MidiDeviceInfo> MidiDeviceManager::getOutputDevices() const
{
    std::vector<MidiDeviceInfo> result;

    try
    {
        RtMidiOut probe;
        unsigned int count = probe.getPortCount();

        for (unsigned int i = 0; i < count; ++i)
            result.push_back ({ static_cast<int> (i), probe.getPortName (i), false });
    }
    catch (const RtMidiError& e)
    {
        fprintf (stderr, "[DC] RtMidi error enumerating outputs: %s\n", e.what());
    }

    return result;
}

bool MidiDeviceManager::openInput (int deviceIndex, MidiInputCallback* callback)
{
    dc_assert (callback != nullptr);

    // Close existing port at this index if open
    closeInput (deviceIndex);

    try
    {
        auto rtIn = std::make_unique<RtMidiIn>();

        if (static_cast<unsigned int> (deviceIndex) >= rtIn->getPortCount())
        {
            fprintf (stderr, "[DC] MIDI input device index %d out of range\n", deviceIndex);
            return false;
        }

        auto& port = inputs_[deviceIndex];
        port.rtMidi = std::move (rtIn);
        port.callback = callback;

        port.rtMidi->openPort (static_cast<unsigned int> (deviceIndex));
        port.rtMidi->setCallback (rtMidiCallback, &port);

        // Don't ignore SysEx, timing, or active sensing by default
        port.rtMidi->ignoreTypes (false, false, false);

        fprintf (stderr, "[DC] Opened MIDI input: %s\n",
                 port.rtMidi->getPortName (static_cast<unsigned int> (deviceIndex)).c_str());
        return true;
    }
    catch (const RtMidiError& e)
    {
        fprintf (stderr, "[DC] Failed to open MIDI input %d: %s\n", deviceIndex, e.what());
        inputs_.erase (deviceIndex);
        return false;
    }
}

void MidiDeviceManager::closeInput (int deviceIndex)
{
    auto it = inputs_.find (deviceIndex);
    if (it == inputs_.end())
        return;

    try
    {
        it->second.rtMidi->closePort();
    }
    catch (const RtMidiError& e)
    {
        fprintf (stderr, "[DC] Error closing MIDI input %d: %s\n", deviceIndex, e.what());
    }

    inputs_.erase (it);
}

bool MidiDeviceManager::openOutput (int deviceIndex)
{
    closeOutput (deviceIndex);

    try
    {
        auto rtOut = std::make_unique<RtMidiOut>();

        if (static_cast<unsigned int> (deviceIndex) >= rtOut->getPortCount())
        {
            fprintf (stderr, "[DC] MIDI output device index %d out of range\n", deviceIndex);
            return false;
        }

        auto& port = outputs_[deviceIndex];
        port.rtMidi = std::move (rtOut);
        port.rtMidi->openPort (static_cast<unsigned int> (deviceIndex));

        fprintf (stderr, "[DC] Opened MIDI output: %s\n",
                 port.rtMidi->getPortName (static_cast<unsigned int> (deviceIndex)).c_str());
        return true;
    }
    catch (const RtMidiError& e)
    {
        fprintf (stderr, "[DC] Failed to open MIDI output %d: %s\n", deviceIndex, e.what());
        outputs_.erase (deviceIndex);
        return false;
    }
}

void MidiDeviceManager::sendMessage (int deviceIndex, const MidiMessage& msg)
{
    auto it = outputs_.find (deviceIndex);
    if (it == outputs_.end())
        return;

    try
    {
        auto* data = msg.getRawData();
        int size = msg.getRawDataSize();
        std::vector<unsigned char> bytes (data, data + size);
        it->second.rtMidi->sendMessage (&bytes);
    }
    catch (const RtMidiError& e)
    {
        fprintf (stderr, "[DC] Error sending MIDI output on port %d: %s\n",
                 deviceIndex, e.what());
    }
}

void MidiDeviceManager::closeOutput (int deviceIndex)
{
    auto it = outputs_.find (deviceIndex);
    if (it == outputs_.end())
        return;

    try
    {
        it->second.rtMidi->closePort();
    }
    catch (const RtMidiError& e)
    {
        fprintf (stderr, "[DC] Error closing MIDI output %d: %s\n", deviceIndex, e.what());
    }

    outputs_.erase (it);
}

void MidiDeviceManager::closeAll()
{
    // Close all inputs
    for (auto& [index, port] : inputs_)
    {
        try { port.rtMidi->closePort(); }
        catch (const RtMidiError&) {}
    }
    inputs_.clear();

    // Close all outputs
    for (auto& [index, port] : outputs_)
    {
        try { port.rtMidi->closePort(); }
        catch (const RtMidiError&) {}
    }
    outputs_.clear();
}

void MidiDeviceManager::rtMidiCallback (double timeStamp,
                                          std::vector<unsigned char>* message,
                                          void* userData)
{
    if (message == nullptr || message->empty() || userData == nullptr)
        return;

    auto* port = static_cast<InputPort*> (userData);

    if (port->callback == nullptr)
        return;

    auto size = static_cast<int> (message->size());
    dc::MidiMessage msg (message->data(), size);
    port->callback->handleMidiMessage (msg, timeStamp);
}

} // namespace dc

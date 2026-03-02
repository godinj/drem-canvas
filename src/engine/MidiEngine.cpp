#include "MidiEngine.h"
#include "dc/foundation/assert.h"
#include "dc/foundation/time.h"

namespace dc
{

MidiEngine::MidiEngine (dc::MessageQueue& mq)
    : messageQueue (mq)
{
}

MidiEngine::~MidiEngine()
{
    shutdown();
}

void MidiEngine::initialise()
{
    auto devices = midiDeviceManager.getInputDevices();

    dc_log ("Available MIDI input devices:");
    for (const auto& device : devices)
        fprintf (stderr, "[DC]   %d: %s\n", device.index, device.name.c_str());

    if (devices.empty())
        dc_log ("No MIDI input devices found.");
}

void MidiEngine::shutdown()
{
    activeInputIndex = -1;
    midiDeviceManager.closeAll();
}

std::vector<std::string> MidiEngine::getAvailableMidiInputs() const
{
    std::vector<std::string> result;
    for (const auto& device : midiDeviceManager.getInputDevices())
        result.push_back (device.name);

    return result;
}

void MidiEngine::setMidiInput (const std::string& deviceName)
{
    // Close existing input
    if (activeInputIndex >= 0)
    {
        midiDeviceManager.closeInput (activeInputIndex);
        activeInputIndex = -1;
    }

    // Find the device by name
    auto devices = midiDeviceManager.getInputDevices();
    for (const auto& device : devices)
    {
        if (device.name == deviceName)
        {
            if (midiDeviceManager.openInput (device.index, this))
            {
                activeInputIndex = device.index;
                fprintf (stderr, "[DC] Opened MIDI input: %s\n", device.name.c_str());
            }
            else
            {
                fprintf (stderr, "[DC] Failed to open MIDI input: %s\n", device.name.c_str());
            }

            return;
        }
    }

    fprintf (stderr, "[DC] MIDI input device not found: %s\n", deviceName.c_str());
}

void MidiEngine::setMidiInputEnabled (const std::string& deviceName, bool enabled)
{
    if (enabled)
        setMidiInput (deviceName);
    else
        shutdown();
}

void MidiEngine::startRecording()
{
    {
        std::lock_guard<std::mutex> lock (sequenceLock);
        recordedSequence.clear();
    }

    recordStartTime = dc::hiResTimeMs() / 1000.0;
    recording.store (true);

    dc_log ("MIDI recording started.");
}

void MidiEngine::stopRecording()
{
    recording.store (false);
    dc_log ("MIDI recording stopped.");
}

MidiSequence MidiEngine::getRecordedSequence() const
{
    std::lock_guard<std::mutex> lock (sequenceLock);
    return recordedSequence;
}

void MidiEngine::clearRecordedSequence()
{
    std::lock_guard<std::mutex> lock (sequenceLock);
    recordedSequence.clear();
}

void MidiEngine::handleMidiMessage (const dc::MidiMessage& message,
                                      double /*timestamp*/)
{
    if (recording.load())
    {
        double currentTime = dc::hiResTimeMs() / 1000.0;
        double relativeTime = currentTime - recordStartTime;

        {
            std::lock_guard<std::mutex> lock (sequenceLock);
            recordedSequence.addEvent (message, relativeTime);
        }
    }

    // Notify listener on the message thread
    if (onMidiMessage)
    {
        auto msg = message;
        messageQueue.post ([this, msg]()
        {
            if (onMidiMessage)
                onMidiMessage (msg);
        });
    }
}

} // namespace dc

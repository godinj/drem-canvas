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
    auto devices = juce::MidiInput::getAvailableDevices();

    dc_log ("Available MIDI input devices:");
    for (const auto& device : devices)
        fprintf (stderr, "[DC]   %s (%s)\n", device.name.toRawUTF8(), device.identifier.toRawUTF8());

    if (devices.isEmpty())
        dc_log ("No MIDI input devices found.");
}

void MidiEngine::shutdown()
{
    if (activeMidiInput != nullptr)
    {
        activeMidiInput->stop();
        activeMidiInput.reset();
    }
}

std::vector<std::string> MidiEngine::getAvailableMidiInputs() const
{
    std::vector<std::string> result;
    for (const auto& device : juce::MidiInput::getAvailableDevices())
        result.push_back (device.name.toStdString());

    return result;
}

void MidiEngine::setMidiInput (const std::string& deviceIdentifier)
{
    // Close existing input
    if (activeMidiInput != nullptr)
    {
        activeMidiInput->stop();
        activeMidiInput.reset();
    }

    // Find the device
    auto devices = juce::MidiInput::getAvailableDevices();
    for (const auto& device : devices)
    {
        if (device.identifier.toStdString() == deviceIdentifier || device.name.toStdString() == deviceIdentifier)
        {
            activeMidiInput = juce::MidiInput::openDevice (device.identifier, this);

            if (activeMidiInput != nullptr)
            {
                activeMidiInput->start();
                fprintf (stderr, "[DC] Opened MIDI input: %s\n", device.name.toRawUTF8());
            }
            else
            {
                fprintf (stderr, "[DC] Failed to open MIDI input: %s\n", device.name.toRawUTF8());
            }

            return;
        }
    }

    fprintf (stderr, "[DC] MIDI input device not found: %s\n", deviceIdentifier.c_str());
}

void MidiEngine::setMidiInputEnabled (const std::string& deviceIdentifier, bool enabled)
{
    if (enabled)
        setMidiInput (deviceIdentifier);
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

juce::MidiMessageSequence MidiEngine::getRecordedSequence() const
{
    std::lock_guard<std::mutex> lock (sequenceLock);
    return recordedSequence;
}

void MidiEngine::clearRecordedSequence()
{
    std::lock_guard<std::mutex> lock (sequenceLock);
    recordedSequence.clear();
}

void MidiEngine::handleIncomingMidiMessage (juce::MidiInput* /*source*/,
                                             const juce::MidiMessage& message)
{
    if (recording.load())
    {
        double currentTime = dc::hiResTimeMs() / 1000.0;
        double timestamp = currentTime - recordStartTime;

        auto timestampedMessage = juce::MidiMessage (message);
        timestampedMessage.setTimeStamp (timestamp);

        {
            std::lock_guard<std::mutex> lock (sequenceLock);
            recordedSequence.addEvent (timestampedMessage);
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

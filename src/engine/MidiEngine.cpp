#include "MidiEngine.h"

namespace dc
{

MidiEngine::MidiEngine() {}

MidiEngine::~MidiEngine()
{
    shutdown();
}

void MidiEngine::initialise()
{
    auto devices = juce::MidiInput::getAvailableDevices();

    DBG ("Available MIDI input devices:");
    for (const auto& device : devices)
        DBG ("  " + device.name + " (" + device.identifier + ")");

    if (devices.isEmpty())
        DBG ("No MIDI input devices found.");
}

void MidiEngine::shutdown()
{
    if (activeMidiInput != nullptr)
    {
        activeMidiInput->stop();
        activeMidiInput.reset();
    }
}

juce::StringArray MidiEngine::getAvailableMidiInputs() const
{
    juce::StringArray result;
    for (const auto& device : juce::MidiInput::getAvailableDevices())
        result.add (device.name);

    return result;
}

void MidiEngine::setMidiInput (const juce::String& deviceIdentifier)
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
        if (device.identifier == deviceIdentifier || device.name == deviceIdentifier)
        {
            activeMidiInput = juce::MidiInput::openDevice (device.identifier, this);

            if (activeMidiInput != nullptr)
            {
                activeMidiInput->start();
                DBG ("Opened MIDI input: " + device.name);
            }
            else
            {
                DBG ("Failed to open MIDI input: " + device.name);
            }

            return;
        }
    }

    DBG ("MIDI input device not found: " + deviceIdentifier);
}

void MidiEngine::setMidiInputEnabled (const juce::String& deviceIdentifier, bool enabled)
{
    if (enabled)
        setMidiInput (deviceIdentifier);
    else
        shutdown();
}

void MidiEngine::startRecording()
{
    {
        const juce::ScopedLock sl (sequenceLock);
        recordedSequence.clear();
    }

    recordStartTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    recording.store (true);

    DBG ("MIDI recording started.");
}

void MidiEngine::stopRecording()
{
    recording.store (false);
    DBG ("MIDI recording stopped.");
}

juce::MidiMessageSequence MidiEngine::getRecordedSequence() const
{
    const juce::ScopedLock sl (sequenceLock);
    return recordedSequence;
}

void MidiEngine::clearRecordedSequence()
{
    const juce::ScopedLock sl (sequenceLock);
    recordedSequence.clear();
}

void MidiEngine::handleIncomingMidiMessage (juce::MidiInput* /*source*/,
                                             const juce::MidiMessage& message)
{
    if (recording.load())
    {
        double currentTime = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        double timestamp = currentTime - recordStartTime;

        auto timestampedMessage = juce::MidiMessage (message);
        timestampedMessage.setTimeStamp (timestamp);

        {
            const juce::ScopedLock sl (sequenceLock);
            recordedSequence.addEvent (timestampedMessage);
        }
    }

    // Notify listener on the message thread
    if (onMidiMessage)
    {
        auto msg = message;
        juce::MessageManager::callAsync ([this, msg]()
        {
            if (onMidiMessage)
                onMidiMessage (msg);
        });
    }
}

} // namespace dc

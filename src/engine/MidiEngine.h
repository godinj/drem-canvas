#pragma once
#include <JuceHeader.h>
#include <atomic>

namespace dc
{

class MidiEngine : private juce::MidiInputCallback
{
public:
    MidiEngine();
    ~MidiEngine() override;

    void initialise();
    void shutdown();

    // Device management
    juce::StringArray getAvailableMidiInputs() const;
    void setMidiInput (const juce::String& deviceIdentifier);
    void setMidiInputEnabled (const juce::String& deviceIdentifier, bool enabled);

    // Recording
    void startRecording();
    void stopRecording();
    bool isRecording() const { return recording.load(); }

    // Get recorded MIDI data (call from message thread)
    juce::MidiMessageSequence getRecordedSequence() const;
    void clearRecordedSequence();

    // Live MIDI output for monitoring
    std::function<void (const juce::MidiMessage&)> onMidiMessage;

private:
    void handleIncomingMidiMessage (juce::MidiInput* source,
                                    const juce::MidiMessage& message) override;

    std::unique_ptr<juce::MidiInput> activeMidiInput;
    juce::MidiMessageSequence recordedSequence;
    juce::CriticalSection sequenceLock;

    std::atomic<bool> recording { false };
    double recordStartTime = 0.0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MidiEngine)
};

} // namespace dc

#pragma once
#include <JuceHeader.h>
#include "dc/foundation/message_queue.h"
#include <atomic>
#include <mutex>
#include <string>
#include <vector>

namespace dc
{

class MidiEngine : private juce::MidiInputCallback
{
public:
    explicit MidiEngine (dc::MessageQueue& mq);
    ~MidiEngine() override;

    void initialise();
    void shutdown();

    // Device management
    std::vector<std::string> getAvailableMidiInputs() const;
    void setMidiInput (const std::string& deviceIdentifier);
    void setMidiInputEnabled (const std::string& deviceIdentifier, bool enabled);

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

    dc::MessageQueue& messageQueue;
    std::unique_ptr<juce::MidiInput> activeMidiInput;
    juce::MidiMessageSequence recordedSequence;
    mutable std::mutex sequenceLock;

    std::atomic<bool> recording { false };
    double recordStartTime = 0.0;

    MidiEngine (const MidiEngine&) = delete;
    MidiEngine& operator= (const MidiEngine&) = delete;
};

} // namespace dc

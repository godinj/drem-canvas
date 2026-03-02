#pragma once

#include "dc/midi/MidiDeviceManager.h"
#include "dc/midi/MidiSequence.h"
#include "dc/foundation/message_queue.h"

#include <atomic>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

namespace dc
{

class MidiEngine : public MidiInputCallback
{
public:
    explicit MidiEngine (dc::MessageQueue& mq);
    ~MidiEngine() override;

    void initialise();
    void shutdown();

    // Device management
    std::vector<std::string> getAvailableMidiInputs() const;
    void setMidiInput (const std::string& deviceName);
    void setMidiInputEnabled (const std::string& deviceName, bool enabled);

    // Recording
    void startRecording();
    void stopRecording();
    bool isRecording() const { return recording.load(); }

    // Get recorded MIDI data (call from message thread)
    MidiSequence getRecordedSequence() const;
    void clearRecordedSequence();

    // Live MIDI output for monitoring
    std::function<void (const dc::MidiMessage&)> onMidiMessage;

private:
    // dc::MidiInputCallback — called on RtMidi thread
    void handleMidiMessage (const dc::MidiMessage& msg,
                             double timestamp) override;

    dc::MessageQueue& messageQueue;
    MidiDeviceManager midiDeviceManager;
    int activeInputIndex = -1;

    MidiSequence recordedSequence;
    mutable std::mutex sequenceLock;

    std::atomic<bool> recording { false };
    double recordStartTime = 0.0;

    MidiEngine (const MidiEngine&) = delete;
    MidiEngine& operator= (const MidiEngine&) = delete;
};

} // namespace dc

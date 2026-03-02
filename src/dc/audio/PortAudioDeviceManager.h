#pragma once

#include "AudioDeviceManager.h"
#include <portaudio.h>
#include <atomic>
#include <string>

namespace dc {

/// PortAudio-backed AudioDeviceManager implementation.
class PortAudioDeviceManager : public AudioDeviceManager
{
public:
    PortAudioDeviceManager();
    ~PortAudioDeviceManager() override;

    std::vector<AudioDeviceInfo> getAvailableDevices() const override;

    bool openDevice (const std::string& deviceName,
                     int numInputChannels, int numOutputChannels,
                     double sampleRate, int bufferSize) override;

    bool openDefaultDevice (int numInputChannels, int numOutputChannels) override;

    void closeDevice() override;
    void setCallback (AudioCallback* callback) override;

    bool isOpen() const override;
    double getSampleRate() const override;
    int getBufferSize() const override;
    std::string getCurrentDeviceName() const override;

private:
    static int paCallback (const void* input, void* output,
                           unsigned long frameCount,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void* userData);

    PaStream* stream_ = nullptr;
    std::atomic<AudioCallback*> callback_ { nullptr };

    double sampleRate_ = 0;
    int bufferSize_ = 0;
    int numInputChannels_ = 0;
    int numOutputChannels_ = 0;
    std::string currentDeviceName_;

    PortAudioDeviceManager (const PortAudioDeviceManager&) = delete;
    PortAudioDeviceManager& operator= (const PortAudioDeviceManager&) = delete;
};

} // namespace dc

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace dc {

/// Callback interface for audio device I/O.
/// Implementations receive audio buffers on the real-time audio thread.
class AudioCallback
{
public:
    virtual ~AudioCallback() = default;

    /// Called on the audio thread to process audio.
    /// @param inputChannelData  array of float* for each input channel (may be nullptr if numInputChannels == 0)
    /// @param numInputChannels  number of input channels
    /// @param outputChannelData array of float* for each output channel
    /// @param numOutputChannels number of output channels
    /// @param numSamples        number of samples per channel in this buffer
    virtual void audioCallback (const float** inputChannelData, int numInputChannels,
                                float** outputChannelData, int numOutputChannels,
                                int numSamples) = 0;

    /// Called before audio starts (on the message thread).
    virtual void audioDeviceAboutToStart (double sampleRate, int bufferSize) {}

    /// Called after audio stops (on the message thread).
    virtual void audioDeviceStopped() {}
};

/// Describes an available audio device.
struct AudioDeviceInfo
{
    std::string name;
    int maxInputChannels = 0;
    int maxOutputChannels = 0;
    std::vector<double> availableSampleRates;
    std::vector<int> availableBufferSizes;
    double defaultSampleRate = 44100.0;
    int defaultBufferSize = 512;
};

/// Abstract audio device manager — platform backends implement this.
class AudioDeviceManager
{
public:
    virtual ~AudioDeviceManager() = default;

    /// Enumerate available audio devices.
    virtual std::vector<AudioDeviceInfo> getAvailableDevices() const = 0;

    /// Open a specific device by name.
    /// @param deviceName  name from AudioDeviceInfo::name
    /// @param numInputChannels  desired input channels (0 = output only)
    /// @param numOutputChannels desired output channels
    /// @param sampleRate  desired sample rate (0 = use device default)
    /// @param bufferSize  desired buffer size (0 = use device default)
    /// @return true on success
    virtual bool openDevice (const std::string& deviceName,
                             int numInputChannels, int numOutputChannels,
                             double sampleRate = 0, int bufferSize = 0) = 0;

    /// Open the system default device with the given channel counts.
    virtual bool openDefaultDevice (int numInputChannels, int numOutputChannels) = 0;

    /// Close the currently open device.
    virtual void closeDevice() = 0;

    /// Set the audio callback (thread-safe — may be called while device is open).
    virtual void setCallback (AudioCallback* callback) = 0;

    /// @return true if a device is currently open and streaming.
    virtual bool isOpen() const = 0;

    /// @return the current sample rate (valid when isOpen()).
    virtual double getSampleRate() const = 0;

    /// @return the current buffer size in samples (valid when isOpen()).
    virtual int getBufferSize() const = 0;

    /// @return the name of the currently open device.
    virtual std::string getCurrentDeviceName() const = 0;

    /// Factory: create the platform-appropriate device manager.
    static std::unique_ptr<AudioDeviceManager> create();
};

} // namespace dc

#include "PortAudioDeviceManager.h"
#include <cstdio>
#include <cstring>

namespace dc {

PortAudioDeviceManager::PortAudioDeviceManager()
{
    PaError err = Pa_Initialize();
    if (err != paNoError)
        fprintf (stderr, "PortAudio init failed: %s\n", Pa_GetErrorText (err));
}

PortAudioDeviceManager::~PortAudioDeviceManager()
{
    closeDevice();
    Pa_Terminate();
}

std::vector<AudioDeviceInfo> PortAudioDeviceManager::getAvailableDevices() const
{
    std::vector<AudioDeviceInfo> devices;
    int count = Pa_GetDeviceCount();

    for (int i = 0; i < count; ++i)
    {
        const PaDeviceInfo* info = Pa_GetDeviceInfo (i);
        if (info == nullptr)
            continue;

        // Only include devices that have output channels (or input channels)
        if (info->maxOutputChannels <= 0 && info->maxInputChannels <= 0)
            continue;

        AudioDeviceInfo dev;
        dev.name = info->name;
        dev.maxInputChannels = info->maxInputChannels;
        dev.maxOutputChannels = info->maxOutputChannels;
        dev.defaultSampleRate = info->defaultSampleRate;
        dev.defaultBufferSize = 512;

        // Common sample rates
        static const double rates[] = { 44100.0, 48000.0, 88200.0, 96000.0 };
        for (double r : rates)
        {
            PaStreamParameters outParams {};
            outParams.device = i;
            outParams.channelCount = (info->maxOutputChannels > 0)
                                         ? std::min (info->maxOutputChannels, 2)
                                         : 0;
            outParams.sampleFormat = paFloat32 | paNonInterleaved;
            outParams.suggestedLatency = info->defaultLowOutputLatency;

            PaStreamParameters inParams {};
            inParams.device = i;
            inParams.channelCount = (info->maxInputChannels > 0)
                                        ? std::min (info->maxInputChannels, 2)
                                        : 0;
            inParams.sampleFormat = paFloat32 | paNonInterleaved;
            inParams.suggestedLatency = info->defaultLowInputLatency;

            PaError result = Pa_IsFormatSupported (
                inParams.channelCount > 0 ? &inParams : nullptr,
                outParams.channelCount > 0 ? &outParams : nullptr,
                r);

            if (result == paFormatIsSupported)
                dev.availableSampleRates.push_back (r);
        }

        // Common buffer sizes
        dev.availableBufferSizes = { 64, 128, 256, 512, 1024, 2048 };

        devices.push_back (std::move (dev));
    }

    return devices;
}

bool PortAudioDeviceManager::openDevice (const std::string& deviceName,
                                         int numInputChannels, int numOutputChannels,
                                         double sampleRate, int bufferSize)
{
    closeDevice();

    // Find the device by name
    int deviceIndex = -1;
    int count = Pa_GetDeviceCount();

    for (int i = 0; i < count; ++i)
    {
        const PaDeviceInfo* info = Pa_GetDeviceInfo (i);
        if (info != nullptr && info->name == deviceName)
        {
            deviceIndex = i;
            break;
        }
    }

    if (deviceIndex < 0)
    {
        fprintf (stderr, "PortAudio: device '%s' not found\n", deviceName.c_str());
        return false;
    }

    const PaDeviceInfo* info = Pa_GetDeviceInfo (deviceIndex);

    numInputChannels = std::min (numInputChannels, info->maxInputChannels);
    numOutputChannels = std::min (numOutputChannels, info->maxOutputChannels);

    if (sampleRate <= 0)
        sampleRate = info->defaultSampleRate;
    if (bufferSize <= 0)
        bufferSize = 512;

    PaStreamParameters outParams {};
    outParams.device = deviceIndex;
    outParams.channelCount = numOutputChannels;
    outParams.sampleFormat = paFloat32 | paNonInterleaved;
    outParams.suggestedLatency = info->defaultLowOutputLatency;

    PaStreamParameters inParams {};
    inParams.device = deviceIndex;
    inParams.channelCount = numInputChannels;
    inParams.sampleFormat = paFloat32 | paNonInterleaved;
    inParams.suggestedLatency = info->defaultLowInputLatency;

    PaError err = Pa_OpenStream (
        &stream_,
        numInputChannels > 0 ? &inParams : nullptr,
        numOutputChannels > 0 ? &outParams : nullptr,
        sampleRate,
        static_cast<unsigned long> (bufferSize),
        paNoFlag,
        paCallback,
        this);

    if (err != paNoError)
    {
        fprintf (stderr, "PortAudio: Pa_OpenStream failed: %s\n", Pa_GetErrorText (err));
        stream_ = nullptr;
        return false;
    }

    sampleRate_ = sampleRate;
    bufferSize_ = bufferSize;
    numInputChannels_ = numInputChannels;
    numOutputChannels_ = numOutputChannels;
    currentDeviceName_ = deviceName;

    // Notify callback before starting
    AudioCallback* cb = callback_.load (std::memory_order_acquire);
    if (cb != nullptr)
        cb->audioDeviceAboutToStart (sampleRate_, bufferSize_);

    err = Pa_StartStream (stream_);
    if (err != paNoError)
    {
        fprintf (stderr, "PortAudio: Pa_StartStream failed: %s\n", Pa_GetErrorText (err));
        Pa_CloseStream (stream_);
        stream_ = nullptr;
        return false;
    }

    fprintf (stderr, "PortAudio: opened '%s' @ %.0f Hz, %d samples, %din/%dout\n",
             currentDeviceName_.c_str(), sampleRate_, bufferSize_,
             numInputChannels_, numOutputChannels_);

    return true;
}

bool PortAudioDeviceManager::openDefaultDevice (int numInputChannels, int numOutputChannels)
{
    PaDeviceIndex outIdx = Pa_GetDefaultOutputDevice();
    PaDeviceIndex inIdx = Pa_GetDefaultInputDevice();

    // Prefer output device for naming
    PaDeviceIndex primary = (outIdx != paNoDevice) ? outIdx : inIdx;
    if (primary == paNoDevice)
    {
        fprintf (stderr, "PortAudio: no default device found\n");
        return false;
    }

    const PaDeviceInfo* info = Pa_GetDeviceInfo (primary);
    if (info == nullptr)
        return false;

    // If input and output are different devices, just use the output device
    // with clamped input channels
    int actualInputChannels = numInputChannels;
    if (inIdx != outIdx && numInputChannels > 0)
    {
        // For separate input device, we'd need duplex — use output device only for now
        // and clamp input to what the output device supports
        actualInputChannels = std::min (numInputChannels, info->maxInputChannels);
    }

    return openDevice (info->name, actualInputChannels, numOutputChannels,
                       info->defaultSampleRate, 0);
}

void PortAudioDeviceManager::closeDevice()
{
    if (stream_ != nullptr)
    {
        Pa_StopStream (stream_);
        Pa_CloseStream (stream_);
        stream_ = nullptr;

        AudioCallback* cb = callback_.load (std::memory_order_acquire);
        if (cb != nullptr)
            cb->audioDeviceStopped();
    }

    sampleRate_ = 0;
    bufferSize_ = 0;
    numInputChannels_ = 0;
    numOutputChannels_ = 0;
    currentDeviceName_.clear();
}

void PortAudioDeviceManager::setCallback (AudioCallback* callback)
{
    callback_.store (callback, std::memory_order_release);
}

bool PortAudioDeviceManager::isOpen() const
{
    return stream_ != nullptr;
}

double PortAudioDeviceManager::getSampleRate() const
{
    return sampleRate_;
}

int PortAudioDeviceManager::getBufferSize() const
{
    return bufferSize_;
}

std::string PortAudioDeviceManager::getCurrentDeviceName() const
{
    return currentDeviceName_;
}

int PortAudioDeviceManager::paCallback (const void* input, void* output,
                                        unsigned long frameCount,
                                        const PaStreamCallbackTimeInfo* /*timeInfo*/,
                                        PaStreamCallbackFlags /*statusFlags*/,
                                        void* userData)
{
    auto* self = static_cast<PortAudioDeviceManager*> (userData);
    int numSamples = static_cast<int> (frameCount);

    // Non-interleaved: PortAudio delivers float** channel arrays directly.
    // input is const void* → treat as array of const float* channel pointers.
    auto** outChannels = static_cast<float**> (output);
    auto** inChannels = reinterpret_cast<const float**> (const_cast<void*> (input));

    AudioCallback* cb = self->callback_.load (std::memory_order_acquire);

    if (cb != nullptr)
    {
        cb->audioCallback (inChannels, self->numInputChannels_,
                           outChannels, self->numOutputChannels_,
                           numSamples);
    }
    else
    {
        // No callback — silence output
        for (int ch = 0; ch < self->numOutputChannels_; ++ch)
            std::memset (outChannels[ch], 0, sizeof (float) * static_cast<size_t> (numSamples));
    }

    return paContinue;
}

// Factory
std::unique_ptr<AudioDeviceManager> AudioDeviceManager::create()
{
    return std::make_unique<PortAudioDeviceManager>();
}

} // namespace dc

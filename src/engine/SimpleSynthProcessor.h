#pragma once
#include <JuceHeader.h>
#include <array>
#include <cmath>

namespace dc
{

/**
 * Minimal polyphonic sine-wave synthesizer for testing MIDI → audio pipeline.
 * Accepts MIDI, generates audio. Used as default instrument on MIDI tracks
 * when no VST/AU plugin is loaded.
 */
class SimpleSynthProcessor : public juce::AudioProcessor
{
public:
    SimpleSynthProcessor()
        : AudioProcessor (BusesProperties()
                            .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
    {
    }

    const juce::String getName() const override { return "SimpleSynth"; }
    void prepareToPlay (double sampleRate, int /*maximumExpectedSamplesPerBlock*/) override
    {
        currentSampleRate = sampleRate;
        for (auto& v : voices)
            v = {};
    }

    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) override
    {
        buffer.clear();

        for (const auto metadata : midiMessages)
        {
            auto msg = metadata.getMessage();
            int sample = metadata.samplePosition;

            if (msg.isNoteOn())
                noteOn (msg.getNoteNumber(), msg.getFloatVelocity(), sample);
            else if (msg.isNoteOff())
                noteOff (msg.getNoteNumber(), sample);
            else if (msg.isAllNotesOff() || msg.isAllSoundOff())
                allNotesOff();
        }

        auto* left  = buffer.getWritePointer (0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1) : nullptr;
        const int numSamples = buffer.getNumSamples();

        for (int s = 0; s < numSamples; ++s)
        {
            float out = 0.0f;

            for (auto& v : voices)
            {
                if (! v.active)
                    continue;

                out += std::sin (static_cast<float> (v.phase)) * v.level;
                v.phase += v.phaseIncrement;

                // Simple envelope decay
                v.level *= 0.99995f;
                if (v.level < 0.0001f)
                    v.active = false;
            }

            // Soft-clip to prevent blowups
            out = std::tanh (out * 0.5f);

            left[s] = out;
            if (right != nullptr)
                right[s] = out;
        }
    }

    double getTailLengthSeconds() const override { return 1.0; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }

    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

private:
    static constexpr int maxVoices = 32;

    struct Voice
    {
        bool   active         = false;
        int    noteNumber     = -1;
        double phase          = 0.0;
        double phaseIncrement = 0.0;
        float  level          = 0.0f;
    };

    std::array<Voice, maxVoices> voices {};
    double currentSampleRate = 44100.0;

    void noteOn (int noteNumber, float velocity, int /*sampleOffset*/)
    {
        // Find free voice (or steal quietest)
        Voice* best = nullptr;
        float lowestLevel = 999.0f;

        for (auto& v : voices)
        {
            if (! v.active)
            {
                best = &v;
                break;
            }
            if (v.level < lowestLevel)
            {
                lowestLevel = v.level;
                best = &v;
            }
        }

        if (best != nullptr)
        {
            double freq = 440.0 * std::pow (2.0, (noteNumber - 69) / 12.0);
            best->active         = true;
            best->noteNumber     = noteNumber;
            best->phase          = 0.0;
            best->phaseIncrement = 2.0 * juce::MathConstants<double>::pi * freq / currentSampleRate;
            best->level          = velocity * 0.3f;
        }
    }

    void noteOff (int noteNumber, int /*sampleOffset*/)
    {
        for (auto& v : voices)
        {
            if (v.active && v.noteNumber == noteNumber)
                v.level *= 0.1f; // fast decay on release
        }
    }

    void allNotesOff()
    {
        for (auto& v : voices)
            v.active = false;
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SimpleSynthProcessor)
};

} // namespace dc

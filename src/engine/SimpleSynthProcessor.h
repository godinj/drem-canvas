#pragma once
#include "dc/engine/AudioNode.h"
#include "dc/engine/MidiBlock.h"
#include "dc/audio/AudioBlock.h"
#include "dc/midi/MidiMessage.h"
#include "dc/foundation/types.h"
#include <array>
#include <cmath>

namespace dc
{

/**
 * Minimal polyphonic sine-wave synthesizer for testing MIDI -> audio pipeline.
 * Accepts MIDI, generates audio. Used as default instrument on MIDI tracks
 * when no VST/AU plugin is loaded.
 */
class SimpleSynthProcessor : public AudioNode
{
public:
    SimpleSynthProcessor() = default;

    std::string getName() const override { return "SimpleSynth"; }
    int getNumInputChannels() const override { return 0; }
    int getNumOutputChannels() const override { return 2; }
    bool acceptsMidi() const override  { return true; }
    bool producesMidi() const override { return false; }

    void prepare (double sampleRate, int /*maxBlockSize*/) override
    {
        currentSampleRate = sampleRate;
        for (auto& v : voices)
            v = {};
    }

    void release() override {}

    void process (AudioBlock& audio, MidiBlock& midi, int numSamples) override
    {
        audio.clear();

        // Read MIDI directly from the MidiBlock parameter (no bridge conversion)
        for (auto it = midi.begin(); it != midi.end(); ++it)
        {
            auto event = *it;
            const auto& msg = event.message;

            if (msg.isNoteOn())
                noteOn (msg.getNoteNumber(), msg.getVelocity(), event.sampleOffset);
            else if (msg.isNoteOff())
                noteOff (msg.getNoteNumber(), event.sampleOffset);
            else if (msg.isController() && (msg.getControllerNumber() == 123 || msg.getControllerNumber() == 120))
                allNotesOff();
        }

        auto* left  = audio.getChannel (0);
        auto* right = audio.getNumChannels() > 1 ? audio.getChannel (1) : nullptr;

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
            best->phaseIncrement = 2.0 * dc::pi<double> * freq / currentSampleRate;
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

    SimpleSynthProcessor (const SimpleSynthProcessor&) = delete;
    SimpleSynthProcessor& operator= (const SimpleSynthProcessor&) = delete;
};

} // namespace dc

#pragma once
#include "dc/engine/AudioNode.h"
#include "dc/plugins/PluginDescription.h"
#include "dc/plugins/ComponentHandler.h"
#include "dc/foundation/spsc_queue.h"
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivsteditcontroller.h>
#include <pluginterfaces/vst/ivstplugview.h>
#include <pluginterfaces/vst/ivstevents.h>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace dc {

class VST3Module;
class PluginEditor;

class PluginInstance : public AudioNode
{
public:
    /// Create from a loaded module and description.
    /// Returns nullptr on failure.
    static std::unique_ptr<PluginInstance> create (
        VST3Module& module,
        const PluginDescription& desc,
        double sampleRate,
        int maxBlockSize);

    ~PluginInstance() override;

    PluginInstance (const PluginInstance&) = delete;
    PluginInstance& operator= (const PluginInstance&) = delete;

    // --- AudioNode interface ---
    void prepare (double sampleRate, int maxBlockSize) override;
    void release() override;
    void process (AudioBlock& audio, MidiBlock& midi, int numSamples) override;
    int getLatencySamples() const override;
    int getNumInputChannels() const override;
    int getNumOutputChannels() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    std::string getName() const override;

    // --- Parameters ---
    int getNumParameters() const;
    std::string getParameterName (int index) const;
    std::string getParameterLabel (int index) const;
    float getParameterValue (int index) const;       // 0.0-1.0 normalized
    void setParameterValue (int index, float value);
    Steinberg::Vst::ParamID getParameterId (int index) const;
    std::string getParameterDisplay (int index) const;

    // --- State ---
    std::vector<uint8_t> getState() const;
    void setState (const std::vector<uint8_t>& data);

    // --- Editor ---
    bool hasEditor() const;
    std::unique_ptr<PluginEditor> createEditor();

    // --- IParameterFinder (spatial hints) ---
    bool supportsParameterFinder() const;
    int findParameterAtPoint (int x, int y) const;

    // --- performEdit snoop ---
    std::optional<EditEvent> popLastEdit();

    // --- Description ---
    const PluginDescription& getDescription() const;

    // --- Internal accessors (for PluginEditor) ---
    Steinberg::Vst::IEditController* getController() const;

private:
    PluginInstance() = default;

    // VST3 interfaces (prevent release in wrong order)
    Steinberg::Vst::IComponent* component_ = nullptr;
    Steinberg::Vst::IAudioProcessor* processor_ = nullptr;
    Steinberg::Vst::IEditController* controller_ = nullptr;
    Steinberg::Vst::IParameterFinder* parameterFinder_ = nullptr;
    std::unique_ptr<ComponentHandler> handler_;
    PluginDescription description_;
    bool controllerIsSameObject_ = false;

    // Audio processing state
    Steinberg::Vst::ProcessData processData_ {};
    Steinberg::Vst::AudioBusBuffers inputBusBuffers_ {};
    Steinberg::Vst::AudioBusBuffers outputBusBuffers_ {};

    // MIDI event conversion buffer (pre-allocated for audio thread)
    std::vector<Steinberg::Vst::Event> eventBuffer_;

    // Parameter cache
    struct ParamInfo
    {
        Steinberg::Vst::ParamID id;
        std::string name;
        std::string label;
    };
    std::vector<ParamInfo> parameters_;

    // performEdit snoop queue
    SPSCQueue<EditEvent> editEvents_ {64};

    // State
    double currentSampleRate_ = 44100.0;
    int currentBlockSize_ = 512;
    bool prepared_ = false;

    // Internal helpers
    void buildParameterList();
    int getParameterIndex (Steinberg::Vst::ParamID id) const;
    void setupProcessing (double sampleRate, int maxBlockSize);
    void connectControllerToComponent();
};

} // namespace dc

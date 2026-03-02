#include "dc/plugins/PluginInstance.h"
#include "dc/plugins/PluginEditor.h"
#include "dc/plugins/VST3Module.h"
#include "dc/audio/AudioBlock.h"
#include "dc/engine/MidiBlock.h"
#include "dc/foundation/assert.h"

#include <pluginterfaces/base/ipluginbase.h>
#include <pluginterfaces/base/ibstream.h>
#include <pluginterfaces/base/funknown.h>
#include <pluginterfaces/vst/ivstaudioprocessor.h>
#include <pluginterfaces/vst/ivstcomponent.h>
#include <pluginterfaces/vst/ivsthostapplication.h>
#include <pluginterfaces/vst/ivstmessage.h>

#include <algorithm>
#include <atomic>
#include <csignal>
#include <csetjmp>
#include <cstring>

namespace dc {

// ─── Minimal IHostApplication for component->initialize() ────────────────

namespace
{

class HostContext : public Steinberg::Vst::IHostApplication
{
public:
    Steinberg::tresult PLUGIN_API getName (Steinberg::Vst::String128 name) override
    {
        const char16_t appName[] = u"Drem Canvas";
        std::memcpy (name, appName, sizeof (appName));
        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API createInstance (Steinberg::TUID /*cid*/,
                                                   Steinberg::TUID /*_iid*/,
                                                   void** obj) override
    {
        *obj = nullptr;
        return Steinberg::kNotImplemented;
    }

    // ── FUnknown ──

    Steinberg::tresult PLUGIN_API queryInterface (const Steinberg::TUID _iid,
                                                   void** obj) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual (_iid,
                Steinberg::Vst::IHostApplication::iid))
        {
            addRef();
            *obj = static_cast<IHostApplication*> (this);
            return Steinberg::kResultOk;
        }
        if (Steinberg::FUnknownPrivate::iidEqual (_iid,
                Steinberg::FUnknown::iid))
        {
            addRef();
            *obj = static_cast<FUnknown*> (this);
            return Steinberg::kResultOk;
        }
        *obj = nullptr;
        return Steinberg::kNoInterface;
    }

    Steinberg::uint32 PLUGIN_API addRef() override  { return ++refCount_; }
    Steinberg::uint32 PLUGIN_API release() override
    {
        auto r = --refCount_;
        if (r == 0) delete this;
        return r;
    }

private:
    std::atomic<Steinberg::uint32> refCount_ { 1 };
};

HostContext& getHostContext()
{
    static HostContext instance;
    return instance;
}

} // anonymous namespace

// ─── MemoryStream — IBStream backed by std::vector<uint8_t> ──────────────

namespace
{

class MemoryStream : public Steinberg::IBStream
{
public:
    virtual ~MemoryStream() = default;
    MemoryStream() = default;

    explicit MemoryStream (const std::vector<uint8_t>& data)
        : data_ (data)
    {
    }

    Steinberg::tresult PLUGIN_API read (void* buffer, Steinberg::int32 numBytes,
                                        Steinberg::int32* numBytesRead) override
    {
        if (buffer == nullptr || numBytes < 0)
            return Steinberg::kInvalidArgument;

        auto available = static_cast<Steinberg::int32> (data_.size() - pos_);
        auto toRead = std::min (numBytes, available);

        if (toRead > 0)
        {
            std::memcpy (buffer, data_.data() + pos_, static_cast<size_t> (toRead));
            pos_ += static_cast<size_t> (toRead);
        }

        if (numBytesRead != nullptr)
            *numBytesRead = toRead;

        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API write (void* buffer, Steinberg::int32 numBytes,
                                         Steinberg::int32* numBytesWritten) override
    {
        if (buffer == nullptr || numBytes < 0)
            return Steinberg::kInvalidArgument;

        auto newSize = pos_ + static_cast<size_t> (numBytes);

        if (newSize > data_.size())
            data_.resize (newSize);

        std::memcpy (data_.data() + pos_, buffer, static_cast<size_t> (numBytes));
        pos_ += static_cast<size_t> (numBytes);

        if (numBytesWritten != nullptr)
            *numBytesWritten = numBytes;

        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API seek (Steinberg::int64 pos, Steinberg::int32 mode,
                                        Steinberg::int64* result) override
    {
        Steinberg::int64 newPos = 0;

        switch (mode)
        {
            case kIBSeekSet:
                newPos = pos;
                break;
            case kIBSeekCur:
                newPos = static_cast<Steinberg::int64> (pos_) + pos;
                break;
            case kIBSeekEnd:
                newPos = static_cast<Steinberg::int64> (data_.size()) + pos;
                break;
            default:
                return Steinberg::kInvalidArgument;
        }

        if (newPos < 0)
            return Steinberg::kInvalidArgument;

        pos_ = static_cast<size_t> (newPos);

        if (result != nullptr)
            *result = static_cast<Steinberg::int64> (pos_);

        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API tell (Steinberg::int64* pos) override
    {
        if (pos == nullptr)
            return Steinberg::kInvalidArgument;

        *pos = static_cast<Steinberg::int64> (pos_);
        return Steinberg::kResultOk;
    }

    // --- FUnknown ---
    Steinberg::tresult PLUGIN_API queryInterface (const Steinberg::TUID _iid, void** obj) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual (_iid, Steinberg::IBStream::iid))
        {
            addRef();
            *obj = static_cast<Steinberg::IBStream*> (this);
            return Steinberg::kResultOk;
        }

        if (Steinberg::FUnknownPrivate::iidEqual (_iid, Steinberg::FUnknown::iid))
        {
            addRef();
            *obj = static_cast<Steinberg::FUnknown*> (this);
            return Steinberg::kResultOk;
        }

        *obj = nullptr;
        return Steinberg::kNoInterface;
    }

    Steinberg::uint32 PLUGIN_API addRef() override { return ++refCount_; }
    Steinberg::uint32 PLUGIN_API release() override
    {
        auto count = --refCount_;
        if (count == 0)
            delete this;
        return count;
    }

    const std::vector<uint8_t>& getData() const { return data_; }

private:
    std::vector<uint8_t> data_;
    size_t pos_ = 0;
    std::atomic<Steinberg::uint32> refCount_ {1};
};

// ─── EventList — IEventList backed by pre-allocated vector ───────────────

class EventList : public Steinberg::Vst::IEventList
{
public:
    virtual ~EventList() = default;
    EventList() = default;

    Steinberg::int32 PLUGIN_API getEventCount() override
    {
        return static_cast<Steinberg::int32> (events_.size());
    }

    Steinberg::tresult PLUGIN_API getEvent (Steinberg::int32 index,
                                            Steinberg::Vst::Event& e) override
    {
        if (index < 0 || static_cast<size_t> (index) >= events_.size())
            return Steinberg::kInvalidArgument;

        e = events_[static_cast<size_t> (index)];
        return Steinberg::kResultOk;
    }

    Steinberg::tresult PLUGIN_API addEvent (Steinberg::Vst::Event& e) override
    {
        events_.push_back (e);
        return Steinberg::kResultOk;
    }

    void clear() { events_.clear(); }

    void reserve (size_t n) { events_.reserve (n); }

    // --- FUnknown ---
    Steinberg::tresult PLUGIN_API queryInterface (const Steinberg::TUID _iid, void** obj) override
    {
        if (Steinberg::FUnknownPrivate::iidEqual (_iid, Steinberg::Vst::IEventList::iid))
        {
            addRef();
            *obj = static_cast<Steinberg::Vst::IEventList*> (this);
            return Steinberg::kResultOk;
        }

        if (Steinberg::FUnknownPrivate::iidEqual (_iid, Steinberg::FUnknown::iid))
        {
            addRef();
            *obj = static_cast<Steinberg::FUnknown*> (this);
            return Steinberg::kResultOk;
        }

        *obj = nullptr;
        return Steinberg::kNoInterface;
    }

    Steinberg::uint32 PLUGIN_API addRef() override { return ++refCount_; }
    Steinberg::uint32 PLUGIN_API release() override
    {
        auto count = --refCount_;
        if (count == 0)
            delete this;
        return count;
    }

private:
    std::vector<Steinberg::Vst::Event> events_;
    std::atomic<Steinberg::uint32> refCount_ {1};
};

// ─── String conversion: Steinberg String128 (UTF-16) to std::string (UTF-8) ─

std::string string128ToString (const Steinberg::Vst::String128& s128)
{
    std::string result;
    result.reserve (128);

    for (int i = 0; i < 128; ++i)
    {
        auto c = static_cast<uint32_t> (s128[i]);

        if (c == 0)
            break;

        // Simple UTF-16 to UTF-8 conversion
        if (c < 0x80)
        {
            result.push_back (static_cast<char> (c));
        }
        else if (c < 0x800)
        {
            result.push_back (static_cast<char> (0xC0 | (c >> 6)));
            result.push_back (static_cast<char> (0x80 | (c & 0x3F)));
        }
        else if (c >= 0xD800 && c <= 0xDBFF)
        {
            // Surrogate pair — combine with next char
            if (i + 1 < 128)
            {
                auto low = static_cast<uint32_t> (s128[i + 1]);

                if (low >= 0xDC00 && low <= 0xDFFF)
                {
                    auto codepoint = ((c - 0xD800) << 10) + (low - 0xDC00) + 0x10000;
                    result.push_back (static_cast<char> (0xF0 | (codepoint >> 18)));
                    result.push_back (static_cast<char> (0x80 | ((codepoint >> 12) & 0x3F)));
                    result.push_back (static_cast<char> (0x80 | ((codepoint >> 6) & 0x3F)));
                    result.push_back (static_cast<char> (0x80 | (codepoint & 0x3F)));
                    ++i;
                }
            }
        }
        else
        {
            result.push_back (static_cast<char> (0xE0 | (c >> 12)));
            result.push_back (static_cast<char> (0x80 | ((c >> 6) & 0x3F)));
            result.push_back (static_cast<char> (0x80 | (c & 0x3F)));
        }
    }

    return result;
}

// ─── Signal-safe crash protection for audio-thread plugin calls ──────────

thread_local sigjmp_buf g_processJmpBuf;
thread_local std::atomic<bool>* g_bypassFlag = nullptr;

void processSignalHandler (int /*sig*/)
{
    if (g_bypassFlag != nullptr)
        g_bypassFlag->store (true, std::memory_order_relaxed);
    siglongjmp (g_processJmpBuf, 1);
}

} // anonymous namespace

// ─── PluginInstance static factory ───────────────────────────────────────

std::unique_ptr<PluginInstance> PluginInstance::create (
    VST3Module& module,
    const PluginDescription& desc,
    double sampleRate,
    int maxBlockSize)
{
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    auto* factory = module.getFactory();

    if (factory == nullptr)
    {
        dc_log ("PluginInstance::create: factory is null");
        return nullptr;
    }

    // 1. Convert UID hex string to TUID and create IComponent
    IComponent* component = nullptr;
    char uid[16] = {};

    if (PluginDescription::hexStringToUid (desc.uid, uid))
    {
        factory->createInstance (
            uid, IComponent::iid, reinterpret_cast<void**> (&component));
    }

    // Fallback: if UID was invalid or createInstance failed, enumerate
    // factory classes and find the first audio effect (handles legacy
    // projects that store integer UIDs from the old format).
    if (component == nullptr)
    {
        auto numClasses = factory->countClasses();

        for (Steinberg::int32 i = 0; i < numClasses; ++i)
        {
            Steinberg::PClassInfo classInfo;

            if (factory->getClassInfo (i, &classInfo) != Steinberg::kResultOk)
                continue;

            if (std::strcmp (classInfo.category, kVstAudioEffectClass) != 0)
                continue;

            factory->createInstance (
                classInfo.cid, IComponent::iid,
                reinterpret_cast<void**> (&component));

            if (component != nullptr)
            {
                dc_log ("PluginInstance::create: resolved by class enumeration");
                break;
            }
        }
    }

    if (component == nullptr)
    {
        dc_log ("PluginInstance::create: failed to create IComponent");
        return nullptr;
    }

    // 2. Initialize component with host context
    auto result = component->initialize (&getHostContext());

    if (result != kResultOk)
    {
        dc_log ("PluginInstance::create: component initialize failed");
        component->release();
        return nullptr;
    }

    // 4. Query IAudioProcessor from component
    IAudioProcessor* processor = nullptr;
    result = component->queryInterface (IAudioProcessor::iid,
                                        reinterpret_cast<void**> (&processor));

    if (result != kResultOk || processor == nullptr)
    {
        dc_log ("PluginInstance::create: failed to get IAudioProcessor");
        component->terminate();
        component->release();
        return nullptr;
    }

    // 5. Get controller class ID
    TUID controllerCid = {};
    result = component->getControllerClassId (controllerCid);

    // 6. Create or query IEditController
    IEditController* controller = nullptr;
    bool controllerIsSameObject = false;

    if (result == kResultOk)
    {
        // Check if the component itself IS the controller
        result = component->queryInterface (IEditController::iid,
                                            reinterpret_cast<void**> (&controller));

        if (result == kResultOk && controller != nullptr)
        {
            controllerIsSameObject = true;
        }
        else
        {
            // Create separate controller from factory
            result = factory->createInstance (
                controllerCid, IEditController::iid,
                reinterpret_cast<void**> (&controller));

            if (result == kResultOk && controller != nullptr)
            {
                // Initialize the separate controller
                controller->initialize (nullptr);
            }
        }
    }
    else
    {
        // No separate controller class — try querying the component directly
        component->queryInterface (IEditController::iid,
                                   reinterpret_cast<void**> (&controller));

        if (controller != nullptr)
            controllerIsSameObject = true;
    }

    if (controller == nullptr)
    {
        dc_log ("PluginInstance::create: failed to get IEditController");
        processor->release();
        component->terminate();
        component->release();
        return nullptr;
    }

    // 7. Build the PluginInstance object
    auto instance = std::unique_ptr<PluginInstance> (new PluginInstance());
    instance->component_ = component;
    instance->processor_ = processor;
    instance->controller_ = controller;
    instance->description_ = desc;
    instance->controllerIsSameObject_ = controllerIsSameObject;

    // 8. Create ComponentHandler and set it on the controller
    instance->handler_ = std::make_unique<ComponentHandler> (instance->editEvents_);
    controller->setComponentHandler (instance->handler_.get());

    // 9. Connect controller <-> component via IConnectionPoint
    instance->connectControllerToComponent();

    // 10. Query IParameterFinder from controller
    controller->queryInterface (IParameterFinder::iid,
                                reinterpret_cast<void**> (&instance->parameterFinder_));

    // 11. Activate audio bus (input/output)
    auto numAudioInputBuses = component->getBusCount (kAudio, kInput);
    auto numAudioOutputBuses = component->getBusCount (kAudio, kOutput);

    if (numAudioInputBuses > 0)
        component->activateBus (kAudio, kInput, 0, true);

    if (numAudioOutputBuses > 0)
        component->activateBus (kAudio, kOutput, 0, true);

    // Activate event bus if plugin accepts MIDI
    auto numEventInputBuses = component->getBusCount (kEvent, kInput);

    if (numEventInputBuses > 0)
        component->activateBus (kEvent, kInput, 0, true);

    auto numEventOutputBuses = component->getBusCount (kEvent, kOutput);

    if (numEventOutputBuses > 0)
        component->activateBus (kEvent, kOutput, 0, true);

    // 12. Setup processing and prepare
    instance->setupProcessing (sampleRate, maxBlockSize);
    instance->prepare (sampleRate, maxBlockSize);

    // 13. Build parameter list
    instance->buildParameterList();

    return instance;
}

PluginInstance::~PluginInstance()
{
    if (prepared_)
        release();

    if (parameterFinder_ != nullptr)
    {
        parameterFinder_->release();
        parameterFinder_ = nullptr;
    }

    if (controller_ != nullptr)
    {
        if (! controllerIsSameObject_)
        {
            controller_->terminate();
        }

        controller_->release();
        controller_ = nullptr;
    }

    if (processor_ != nullptr)
    {
        processor_->release();
        processor_ = nullptr;
    }

    if (component_ != nullptr)
    {
        component_->terminate();
        component_->release();
        component_ = nullptr;
    }
}

// ─── AudioNode interface ─────────────────────────────────────────────────

void PluginInstance::prepare (double sampleRate, int maxBlockSize)
{
    currentSampleRate_ = sampleRate;
    currentBlockSize_ = maxBlockSize;

    // Pre-allocate the MIDI event buffer to avoid allocation on audio thread
    eventBuffer_.reserve (static_cast<size_t> (maxBlockSize));

    prepared_ = true;
}

void PluginInstance::release()
{
    if (processor_ != nullptr)
        processor_->setProcessing (false);

    if (component_ != nullptr)
        component_->setActive (false);

    prepared_ = false;
}

void PluginInstance::process (AudioBlock& audio, MidiBlock& midi, int numSamples)
{
    if (! prepared_ || processor_ == nullptr || bypassed_.load (std::memory_order_relaxed))
        return;

    // --- Build channel pointer array on stack ---
    // AudioBlock::getChannel() returns individual float* values, but VST3
    // needs a float** pointing to a contiguous array of channel pointers.
    constexpr int kMaxChannels = 8;
    float* channelPtrs[kMaxChannels] = {};
    auto numChannels = std::min (audio.getNumChannels(), kMaxChannels);

    for (int ch = 0; ch < numChannels; ++ch)
        channelPtrs[ch] = audio.getChannel (ch);

    // --- Set up input bus buffers ---
    inputBusBuffers_.numChannels = numChannels;
    inputBusBuffers_.silenceFlags = 0;
    inputBusBuffers_.channelBuffers32 = channelPtrs;

    // For effects: in-place processing (output aliases input)
    outputBusBuffers_.numChannels = numChannels;
    outputBusBuffers_.silenceFlags = 0;
    outputBusBuffers_.channelBuffers32 = channelPtrs;

    processData_.processMode = Steinberg::Vst::kRealtime;
    processData_.symbolicSampleSize = Steinberg::Vst::kSample32;
    processData_.numSamples = numSamples;
    processData_.numInputs = 1;
    processData_.numOutputs = 1;
    processData_.inputs = &inputBusBuffers_;
    processData_.outputs = &outputBusBuffers_;
    processData_.inputParameterChanges = nullptr;
    processData_.outputParameterChanges = nullptr;
    processData_.processContext = nullptr;

    // --- Convert MidiBlock events to VST3 Event array ---
    EventList inputEvents;
    inputEvents.reserve (128);

    if (! midi.isEmpty())
    {
        for (auto it = midi.begin(); it != midi.end(); ++it)
        {
            auto event = *it;
            auto& msg = event.message;

            Steinberg::Vst::Event vstEvent = {};
            vstEvent.busIndex = 0;
            vstEvent.sampleOffset = event.sampleOffset;
            vstEvent.ppqPosition = 0;
            vstEvent.flags = 0;

            if (msg.isNoteOn())
            {
                vstEvent.type = Steinberg::Vst::Event::kNoteOnEvent;
                vstEvent.noteOn.channel = static_cast<Steinberg::int16> (msg.getChannel() - 1);
                vstEvent.noteOn.pitch = static_cast<Steinberg::int16> (msg.getNoteNumber());
                vstEvent.noteOn.tuning = 0.0f;
                vstEvent.noteOn.velocity = msg.getVelocity();
                vstEvent.noteOn.length = 0;
                vstEvent.noteOn.noteId = -1;
                inputEvents.addEvent (vstEvent);
            }
            else if (msg.isNoteOff())
            {
                vstEvent.type = Steinberg::Vst::Event::kNoteOffEvent;
                vstEvent.noteOff.channel = static_cast<Steinberg::int16> (msg.getChannel() - 1);
                vstEvent.noteOff.pitch = static_cast<Steinberg::int16> (msg.getNoteNumber());
                vstEvent.noteOff.velocity = msg.getVelocity();
                vstEvent.noteOff.noteId = -1;
                vstEvent.noteOff.tuning = 0.0f;
                inputEvents.addEvent (vstEvent);
            }
            else if (msg.isSysEx())
            {
                vstEvent.type = Steinberg::Vst::Event::kDataEvent;
                vstEvent.data.size = static_cast<Steinberg::uint32> (msg.getRawDataSize());
                vstEvent.data.type = Steinberg::Vst::DataEvent::kMidiSysEx;
                vstEvent.data.bytes = msg.getRawData();
                inputEvents.addEvent (vstEvent);
            }
            else if (msg.isAftertouch())
            {
                vstEvent.type = Steinberg::Vst::Event::kPolyPressureEvent;
                vstEvent.polyPressure.channel = static_cast<Steinberg::int16> (msg.getChannel() - 1);
                vstEvent.polyPressure.pitch = static_cast<Steinberg::int16> (msg.getNoteNumber());
                vstEvent.polyPressure.pressure = static_cast<float> (msg.getRawVelocity()) / 127.0f;
                vstEvent.polyPressure.noteId = -1;
                inputEvents.addEvent (vstEvent);
            }
            // Note: CC / pitch bend / program change are handled via
            // IParameterChanges in a full implementation. For now, skip them.
        }
    }

    processData_.inputEvents = &inputEvents;

    // Output events (for plugins that produce MIDI)
    EventList outputEvents;
    outputEvents.reserve (128);
    processData_.outputEvents = &outputEvents;

    // --- Call the processor (with signal-safe crash protection) ---
    g_bypassFlag = &bypassed_;
    struct sigaction newAction {}, oldSegv {}, oldBus {};
    newAction.sa_handler = processSignalHandler;
    newAction.sa_flags = 0;
    sigemptyset (&newAction.sa_mask);

    sigaction (SIGSEGV, &newAction, &oldSegv);
    sigaction (SIGBUS, &newAction, &oldBus);

    if (sigsetjmp (g_processJmpBuf, 1) == 0)
    {
        processor_->process (processData_);
    }
    else
    {
        dc_log ("PluginInstance::process: plugin '%s' crashed — bypassing",
                description_.name.c_str());
    }

    sigaction (SIGSEGV, &oldSegv, nullptr);
    sigaction (SIGBUS, &oldBus, nullptr);
    g_bypassFlag = nullptr;

    // --- Copy output events back to MidiBlock ---
    if (producesMidi())
    {
        auto numOutputEvents = outputEvents.getEventCount();

        for (Steinberg::int32 i = 0; i < numOutputEvents; ++i)
        {
            Steinberg::Vst::Event vstEvent = {};

            if (outputEvents.getEvent (i, vstEvent) != Steinberg::kResultOk)
                continue;

            if (vstEvent.type == Steinberg::Vst::Event::kNoteOnEvent)
            {
                auto msg = MidiMessage::noteOn (
                    vstEvent.noteOn.channel + 1,
                    vstEvent.noteOn.pitch,
                    vstEvent.noteOn.velocity);
                midi.addEvent (msg, vstEvent.sampleOffset);
            }
            else if (vstEvent.type == Steinberg::Vst::Event::kNoteOffEvent)
            {
                auto msg = MidiMessage::noteOff (
                    vstEvent.noteOff.channel + 1,
                    vstEvent.noteOff.pitch,
                    vstEvent.noteOff.velocity);
                midi.addEvent (msg, vstEvent.sampleOffset);
            }
        }
    }
}

int PluginInstance::getLatencySamples() const
{
    if (processor_ != nullptr)
        return static_cast<int> (processor_->getLatencySamples());

    return 0;
}

int PluginInstance::getNumInputChannels() const
{
    return description_.numInputChannels;
}

int PluginInstance::getNumOutputChannels() const
{
    return description_.numOutputChannels;
}

bool PluginInstance::acceptsMidi() const
{
    return description_.acceptsMidi;
}

bool PluginInstance::producesMidi() const
{
    return description_.producesMidi;
}

std::string PluginInstance::getName() const
{
    return description_.name;
}

// ─── Bypass ──────────────────────────────────────────────────────────────

bool PluginInstance::isBypassed() const
{
    return bypassed_.load (std::memory_order_relaxed);
}

void PluginInstance::resetBypass()
{
    bypassed_.store (false, std::memory_order_relaxed);
}

// ─── Parameters ──────────────────────────────────────────────────────────

int PluginInstance::getNumParameters() const
{
    return static_cast<int> (parameters_.size());
}

std::string PluginInstance::getParameterName (int index) const
{
    if (index < 0 || static_cast<size_t> (index) >= parameters_.size())
        return {};

    return parameters_[static_cast<size_t> (index)].name;
}

std::string PluginInstance::getParameterLabel (int index) const
{
    if (index < 0 || static_cast<size_t> (index) >= parameters_.size())
        return {};

    return parameters_[static_cast<size_t> (index)].label;
}

float PluginInstance::getParameterValue (int index) const
{
    if (controller_ == nullptr ||
        index < 0 || static_cast<size_t> (index) >= parameters_.size())
        return 0.0f;

    auto id = parameters_[static_cast<size_t> (index)].id;
    return static_cast<float> (controller_->getParamNormalized (id));
}

void PluginInstance::setParameterValue (int index, float value)
{
    if (controller_ == nullptr ||
        index < 0 || static_cast<size_t> (index) >= parameters_.size())
        return;

    auto id = parameters_[static_cast<size_t> (index)].id;
    controller_->setParamNormalized (id, static_cast<double> (value));

    // Also notify via the handler for automation recording
    if (handler_)
        handler_->performEdit (id, static_cast<double> (value));
}

Steinberg::Vst::ParamID PluginInstance::getParameterId (int index) const
{
    if (index < 0 || static_cast<size_t> (index) >= parameters_.size())
        return Steinberg::Vst::kNoParamId;

    return parameters_[static_cast<size_t> (index)].id;
}

std::string PluginInstance::getParameterDisplay (int index) const
{
    if (controller_ == nullptr ||
        index < 0 || static_cast<size_t> (index) >= parameters_.size())
        return {};

    auto id = parameters_[static_cast<size_t> (index)].id;
    auto normalized = controller_->getParamNormalized (id);

    Steinberg::Vst::String128 display = {};

    if (controller_->getParamStringByValue (id, normalized, display) == Steinberg::kResultOk)
        return string128ToString (display);

    return {};
}

// ─── State ───────────────────────────────────────────────────────────────

std::vector<uint8_t> PluginInstance::getState() const
{
    if (component_ == nullptr)
        return {};

    // Write component state
    auto* componentStream = new MemoryStream();
    component_->getState (componentStream);

    auto componentData = componentStream->getData();

    // Write controller state (if separate)
    std::vector<uint8_t> controllerData;

    if (controller_ != nullptr)
    {
        auto* controllerStream = new MemoryStream();
        controller_->getState (controllerStream);
        controllerData = controllerStream->getData();
        controllerStream->release();
    }

    // Combine: [4 bytes componentSize][componentData][controllerData]
    auto componentSize = static_cast<uint32_t> (componentData.size());
    std::vector<uint8_t> result;
    result.resize (4 + componentData.size() + controllerData.size());

    std::memcpy (result.data(), &componentSize, 4);
    std::memcpy (result.data() + 4, componentData.data(), componentData.size());

    if (! controllerData.empty())
        std::memcpy (result.data() + 4 + componentData.size(),
                     controllerData.data(), controllerData.size());

    componentStream->release();
    return result;
}

void PluginInstance::setState (const std::vector<uint8_t>& data)
{
    if (component_ == nullptr || data.size() < 4)
        return;

    // VST3 spec requires IComponent::setState to be called while the plugin
    // is NOT active/processing.  Deactivate before restoring state, and
    // always reactivate afterwards — even if setState fails.
    if (processor_ != nullptr)
        processor_->setProcessing (false);
    component_->setActive (false);

    // Parse: [4 bytes componentSize][componentData][controllerData]
    uint32_t componentSize = 0;
    std::memcpy (&componentSize, data.data(), 4);

    // Sanity check: if componentSize is unreasonably large or doesn't fit,
    // this might be legacy-format or other pre-migration data — try raw pass-through
    if (4 + componentSize > data.size())
    {
        dc_log ("PluginInstance::setState: format mismatch (size=%zu, declared=%u) — "
                "attempting raw pass-through",
                data.size(), componentSize);

        // Try passing the entire blob as component state (some formats work this way)
        auto* stream = new MemoryStream (data);
        auto result = component_->setState (stream);
        stream->release();

        if (result == Steinberg::kResultOk)
            dc_log ("PluginInstance::setState: raw pass-through succeeded");
        else
            dc_log ("PluginInstance::setState: raw pass-through also failed — "
                    "state will not be restored (likely legacy format)");

        // Reactivate with full VST3 lifecycle (setupProcessing required before setActive)
        setupProcessing (currentSampleRate_, currentBlockSize_);
        return;
    }

    // Restore component state
    std::vector<uint8_t> componentData (
        data.begin() + 4,
        data.begin() + 4 + static_cast<ptrdiff_t> (componentSize));

    auto* componentStream = new MemoryStream (componentData);
    component_->setState (componentStream);

    // Also pass component state to controller via setComponentState
    if (controller_ != nullptr)
    {
        componentStream->seek (0, Steinberg::IBStream::kIBSeekSet, nullptr);
        controller_->setComponentState (componentStream);
    }

    componentStream->release();

    // Restore controller state (if present)
    if (controller_ != nullptr && 4 + componentSize < data.size())
    {
        std::vector<uint8_t> controllerData (
            data.begin() + 4 + static_cast<ptrdiff_t> (componentSize),
            data.end());

        auto* controllerStream = new MemoryStream (controllerData);
        controller_->setState (controllerStream);
        controllerStream->release();
    }

    // Reactivate with full VST3 lifecycle (setupProcessing required before setActive)
    setupProcessing (currentSampleRate_, currentBlockSize_);
}

// ─── Editor ──────────────────────────────────────────────────────────────

bool PluginInstance::hasEditor() const
{
    return description_.hasEditor;
}

std::unique_ptr<PluginEditor> PluginInstance::createEditor()
{
    return PluginEditor::create (*this);
}

// ─── IParameterFinder ────────────────────────────────────────────────────

bool PluginInstance::supportsParameterFinder() const
{
    return parameterFinder_ != nullptr || viewParameterFinder_ != nullptr;
}

int PluginInstance::findParameterAtPoint (int x, int y) const
{
    auto* finder = parameterFinder_ ? parameterFinder_ : viewParameterFinder_;
    if (finder == nullptr)
        return -1;

    Steinberg::Vst::ParamID resultId = Steinberg::Vst::kNoParamId;

    auto result = finder->findParameter (
        static_cast<Steinberg::int32> (x),
        static_cast<Steinberg::int32> (y),
        resultId);

    if (result != Steinberg::kResultOk)
        return -1;

    return getParameterIndex (resultId);
}

void PluginInstance::setViewParameterFinder (Steinberg::Vst::IParameterFinder* finder)
{
    viewParameterFinder_ = finder;
}

// ─── performEdit snoop ───────────────────────────────────────────────────

std::optional<EditEvent> PluginInstance::popLastEdit()
{
    EditEvent event {};

    if (editEvents_.pop (event))
        return event;

    return std::nullopt;
}

// ─── Description ─────────────────────────────────────────────────────────

const PluginDescription& PluginInstance::getDescription() const
{
    return description_;
}

// ─── Internal accessors ──────────────────────────────────────────────────

Steinberg::Vst::IEditController* PluginInstance::getController() const
{
    return controller_;
}

// ─── Internal helpers ────────────────────────────────────────────────────

void PluginInstance::buildParameterList()
{
    if (controller_ == nullptr)
        return;

    auto count = controller_->getParameterCount();
    parameters_.clear();
    parameters_.reserve (static_cast<size_t> (count));

    for (Steinberg::int32 i = 0; i < count; ++i)
    {
        Steinberg::Vst::ParameterInfo info = {};

        if (controller_->getParameterInfo (i, info) != Steinberg::kResultOk)
            continue;

        ParamInfo pi;
        pi.id = info.id;
        pi.name = string128ToString (info.title);
        pi.label = string128ToString (info.units);
        parameters_.push_back (std::move (pi));
    }
}

int PluginInstance::getParameterIndex (Steinberg::Vst::ParamID id) const
{
    for (size_t i = 0; i < parameters_.size(); ++i)
    {
        if (parameters_[i].id == id)
            return static_cast<int> (i);
    }

    return -1;
}

void PluginInstance::setupProcessing (double sampleRate, int maxBlockSize)
{
    if (processor_ == nullptr || component_ == nullptr)
        return;

    Steinberg::Vst::ProcessSetup setup {};
    setup.processMode = Steinberg::Vst::kRealtime;
    setup.symbolicSampleSize = Steinberg::Vst::kSample32;
    setup.maxSamplesPerBlock = maxBlockSize;
    setup.sampleRate = sampleRate;

    processor_->setupProcessing (setup);
    component_->setActive (true);
    processor_->setProcessing (true);
}

void PluginInstance::connectControllerToComponent()
{
    using namespace Steinberg;
    using namespace Steinberg::Vst;

    if (component_ == nullptr || controller_ == nullptr)
        return;

    // If controller and component are the same object, no connection needed
    if (controllerIsSameObject_)
        return;

    IConnectionPoint* componentConnection = nullptr;
    IConnectionPoint* controllerConnection = nullptr;

    component_->queryInterface (IConnectionPoint::iid,
                                reinterpret_cast<void**> (&componentConnection));
    controller_->queryInterface (IConnectionPoint::iid,
                                 reinterpret_cast<void**> (&controllerConnection));

    if (componentConnection != nullptr && controllerConnection != nullptr)
    {
        componentConnection->connect (controllerConnection);
        controllerConnection->connect (componentConnection);
    }

    if (componentConnection != nullptr)
        componentConnection->release();

    if (controllerConnection != nullptr)
        controllerConnection->release();
}

} // namespace dc

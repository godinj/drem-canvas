#include "dc/midi/MidiSequence.h"

#include <algorithm>
#include <cstring>

namespace {

// The legacy format writes doubles and ints in big-endian byte order.
// These helpers read big-endian values for legacy format detection.

double readDoubleBE(const uint8_t* p)
{
    uint8_t swapped[8];
    for (int i = 0; i < 8; ++i)
        swapped[i] = p[7 - i];

    double result;
    std::memcpy(&result, swapped, 8);
    return result;
}

int32_t readInt32BE(const uint8_t* p)
{
    return static_cast<int32_t>(
        (static_cast<uint32_t>(p[0]) << 24) |
        (static_cast<uint32_t>(p[1]) << 16) |
        (static_cast<uint32_t>(p[2]) << 8) |
        static_cast<uint32_t>(p[3]));
}

} // anonymous namespace

namespace dc {

void MidiSequence::addEvent(const MidiMessage& msg, double timeInBeats)
{
    TimedMidiEvent evt;
    evt.timeInBeats = timeInBeats;
    evt.message = msg;

    // Binary search for insertion point to maintain sorted order
    auto it = std::lower_bound(
        events_.begin(), events_.end(), timeInBeats,
        [](const TimedMidiEvent& e, double t) { return e.timeInBeats < t; });

    events_.insert(it, std::move(evt));
}

void MidiSequence::removeEvent(int index)
{
    if (index >= 0 && index < static_cast<int>(events_.size()))
        events_.erase(events_.begin() + index);
}

const TimedMidiEvent& MidiSequence::getEvent(int index) const
{
    return events_[static_cast<size_t>(index)];
}

TimedMidiEvent& MidiSequence::getEvent(int index)
{
    return events_[static_cast<size_t>(index)];
}

int MidiSequence::getNumEvents() const
{
    return static_cast<int>(events_.size());
}

void MidiSequence::clear()
{
    events_.clear();
}

void MidiSequence::sort()
{
    std::stable_sort(events_.begin(), events_.end(),
        [](const TimedMidiEvent& a, const TimedMidiEvent& b)
        {
            return a.timeInBeats < b.timeInBeats;
        });
}

void MidiSequence::updateMatchedPairs()
{
    // Reset all matched pair indices
    for (auto& evt : events_)
        evt.matchedPairIndex = -1;

    // For each noteOn, find the next matching noteOff
    for (int i = 0; i < static_cast<int>(events_.size()); ++i)
    {
        if (!events_[static_cast<size_t>(i)].message.isNoteOn())
            continue;

        int note = events_[static_cast<size_t>(i)].message.getNoteNumber();
        int chan = events_[static_cast<size_t>(i)].message.getChannel();

        for (int j = i + 1; j < static_cast<int>(events_.size()); ++j)
        {
            auto& candidate = events_[static_cast<size_t>(j)];
            if (candidate.message.isNoteOff()
                && candidate.message.getNoteNumber() == note
                && candidate.message.getChannel() == chan
                && candidate.matchedPairIndex == -1)
            {
                events_[static_cast<size_t>(i)].matchedPairIndex = j;
                candidate.matchedPairIndex = i;
                break;
            }
        }
    }
}

std::pair<int, int> MidiSequence::getEventsInRange(double startBeats,
                                                    double endBeats) const
{
    // Find first event >= startBeats
    auto first = std::lower_bound(
        events_.begin(), events_.end(), startBeats,
        [](const TimedMidiEvent& e, double t) { return e.timeInBeats < t; });

    // Find first event >= endBeats
    auto last = std::lower_bound(
        first, events_.end(), endBeats,
        [](const TimedMidiEvent& e, double t) { return e.timeInBeats < t; });

    return {static_cast<int>(first - events_.begin()),
            static_cast<int>(last - events_.begin())};
}

// --- Serialization ---

std::vector<uint8_t> MidiSequence::toBinary() const
{
    // Calculate total size
    size_t totalSize = sizeof(uint32_t) + sizeof(uint32_t);  // version + numEvents
    for (auto& evt : events_)
        totalSize += sizeof(double) + sizeof(uint16_t) + static_cast<size_t>(evt.message.getRawDataSize());

    std::vector<uint8_t> result;
    result.resize(totalSize);

    auto* dst = result.data();

    // Version
    uint32_t version = kCurrentVersion;
    std::memcpy(dst, &version, sizeof(uint32_t));
    dst += sizeof(uint32_t);

    // Number of events
    auto numEvents = static_cast<uint32_t>(events_.size());
    std::memcpy(dst, &numEvents, sizeof(uint32_t));
    dst += sizeof(uint32_t);

    // Events
    for (auto& evt : events_)
    {
        // timeInBeats
        std::memcpy(dst, &evt.timeInBeats, sizeof(double));
        dst += sizeof(double);

        // message size
        auto msgSize = static_cast<uint16_t>(evt.message.getRawDataSize());
        std::memcpy(dst, &msgSize, sizeof(uint16_t));
        dst += sizeof(uint16_t);

        // message data
        std::memcpy(dst, evt.message.getRawData(), msgSize);
        dst += msgSize;
    }

    return result;
}

MidiSequence MidiSequence::fromBinary(const std::vector<uint8_t>& data)
{
    MidiSequence seq;

    if (data.size() < sizeof(uint32_t))
        return seq;

    const auto* src = data.data();

    // Read version
    uint32_t version;
    std::memcpy(&version, src, sizeof(uint32_t));
    src += sizeof(uint32_t);

    if (version == kCurrentVersion)
    {
        // New format
        if (data.size() < sizeof(uint32_t) * 2)
            return seq;

        uint32_t numEvents;
        std::memcpy(&numEvents, src, sizeof(uint32_t));
        src += sizeof(uint32_t);

        seq.events_.reserve(numEvents);

        const auto* end = data.data() + data.size();
        for (uint32_t i = 0; i < numEvents && src < end; ++i)
        {
            if (src + sizeof(double) + sizeof(uint16_t) > end)
                break;

            TimedMidiEvent evt;

            std::memcpy(&evt.timeInBeats, src, sizeof(double));
            src += sizeof(double);

            uint16_t msgSize;
            std::memcpy(&msgSize, src, sizeof(uint16_t));
            src += sizeof(uint16_t);

            if (src + msgSize > end)
                break;

            evt.message = MidiMessage(src, static_cast<int>(msgSize));
            src += msgSize;

            seq.events_.push_back(std::move(evt));
        }

        seq.updateMatchedPairs();
    }
    else
    {
        // Legacy format (pre-migration): no version header. The first 8 bytes are a
        // big-endian double (timestamp), followed by a big-endian int32
        // (message size), then raw MIDI bytes. Repeated until EOF.
        // We re-read from the beginning since we already consumed 4 bytes
        // of what was actually the start of the first timestamp.
        const auto* p = data.data();
        const auto* dataEnd = data.data() + data.size();

        while (p + sizeof(double) + sizeof(int32_t) <= dataEnd)
        {
            double timestamp = readDoubleBE(p);
            p += sizeof(double);

            int32_t msgSize = readInt32BE(p);
            p += sizeof(int32_t);

            if (msgSize <= 0 || msgSize > 1024 || p + msgSize > dataEnd)
                break;

            TimedMidiEvent evt;
            evt.timeInBeats = timestamp;
            evt.message = MidiMessage(p, msgSize);
            seq.events_.push_back(std::move(evt));

            p += msgSize;
        }

        seq.updateMatchedPairs();
    }

    return seq;
}

const std::vector<TimedMidiEvent>& MidiSequence::getEvents() const
{
    return events_;
}

} // namespace dc

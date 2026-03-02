#pragma once
#include <cstdio>
#include <filesystem>
#include <map>
#include <string>

namespace dc {

struct PluginDescription
{
    std::string name;
    std::string manufacturer;
    std::string category;
    std::string version;
    std::string uid;              // VST3 class UID as hex string (32 chars)
    std::filesystem::path path;   // .vst3 bundle path
    int numInputChannels = 0;
    int numOutputChannels = 0;
    bool hasEditor = false;
    bool acceptsMidi = false;
    bool producesMidi = false;

    /// Serialize to a string map (for YAML persistence)
    std::map<std::string, std::string> toMap() const
    {
        std::map<std::string, std::string> m;
        m["name"] = name;
        m["manufacturer"] = manufacturer;
        m["category"] = category;
        m["version"] = version;
        m["uid"] = uid;
        m["path"] = path.string();
        m["numInputChannels"] = std::to_string(numInputChannels);
        m["numOutputChannels"] = std::to_string(numOutputChannels);
        m["hasEditor"] = std::to_string(static_cast<int>(hasEditor));
        m["acceptsMidi"] = std::to_string(static_cast<int>(acceptsMidi));
        m["producesMidi"] = std::to_string(static_cast<int>(producesMidi));
        return m;
    }

    /// Deserialize from a string map
    static PluginDescription fromMap(const std::map<std::string, std::string>& m)
    {
        PluginDescription d;

        auto get = [&](const std::string& key) -> std::string
        {
            auto it = m.find(key);
            return (it != m.end()) ? it->second : std::string();
        };

        d.name = get("name");
        d.manufacturer = get("manufacturer");
        d.category = get("category");
        d.version = get("version");
        d.uid = get("uid");
        d.path = get("path");

        auto inCh = get("numInputChannels");
        auto outCh = get("numOutputChannels");
        auto hasEd = get("hasEditor");
        auto accMidi = get("acceptsMidi");
        auto prodMidi = get("producesMidi");

        if (! inCh.empty())       d.numInputChannels = std::stoi(inCh);
        if (! outCh.empty())      d.numOutputChannels = std::stoi(outCh);
        if (! hasEd.empty())      d.hasEditor = (std::stoi(hasEd) != 0);
        if (! accMidi.empty())    d.acceptsMidi = (std::stoi(accMidi) != 0);
        if (! prodMidi.empty())   d.producesMidi = (std::stoi(prodMidi) != 0);

        return d;
    }

    /// Convert a Steinberg FUID to a 32-char hex string (lowercase)
    static std::string uidToHexString(const char uid[16])
    {
        std::string result;
        result.reserve(32);

        for (int i = 0; i < 16; ++i)
        {
            char buf[3];
            std::snprintf(buf, sizeof(buf), "%02x",
                          static_cast<unsigned char>(uid[i]));
            result += buf;
        }

        return result;
    }

    /// Convert a 32-char hex string back to a 16-byte UID.
    /// Returns false on invalid input.
    static bool hexStringToUid(const std::string& hex, char uid[16])
    {
        if (hex.size() != 32)
            return false;

        for (int i = 0; i < 16; ++i)
        {
            auto hi = hex[static_cast<size_t>(i * 2)];
            auto lo = hex[static_cast<size_t>(i * 2 + 1)];

            auto hexDigit = [](char c, int& out) -> bool
            {
                if (c >= '0' && c <= '9')      { out = c - '0'; return true; }
                if (c >= 'a' && c <= 'f')      { out = c - 'a' + 10; return true; }
                if (c >= 'A' && c <= 'F')      { out = c - 'A' + 10; return true; }
                return false;
            };

            int hiVal = 0, loVal = 0;

            if (! hexDigit(hi, hiVal) || ! hexDigit(lo, loVal))
                return false;

            uid[i] = static_cast<char>((hiVal << 4) | loVal);
        }

        return true;
    }
};

} // namespace dc

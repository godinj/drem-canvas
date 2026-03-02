#include "dc/plugins/ProbeCache.h"
#include "dc/foundation/assert.h"
#include "dc/foundation/file_utils.h"
#include <yaml-cpp/yaml.h>

namespace dc {

ProbeCache::ProbeCache (const std::filesystem::path& cacheDir)
    : cacheFile_ (cacheDir / "probeCache.yaml"),
      pedalFile_ (cacheDir / ".probe-pedal")
{
}

void ProbeCache::load()
{
    // Handle leftover pedal from a previous crash
    if (auto crashed = checkPedal())
    {
        dc_log ("ProbeCache: previous load of %s crashed — marking blocked",
                crashed->string().c_str());
        entries_[crashed->string()] = { getMtime (*crashed), Status::blocked };
        clearPedal();
    }

    try
    {
        auto content = dc::readFileToString (cacheFile_);
        if (content.empty())
            return;

        YAML::Node root = YAML::Load (content);

        if (! root["modules"] || ! root["modules"].IsMap())
            return;

        for (auto it = root["modules"].begin(); it != root["modules"].end(); ++it)
        {
            auto path = it->first.as<std::string>();
            auto node = it->second;

            Entry entry;
            entry.mtime = node["mtime"].as<std::int64_t> (0);

            auto statusStr = node["status"].as<std::string> ("");
            if (statusStr == "safe")
                entry.status = Status::safe;
            else if (statusStr == "blocked")
                entry.status = Status::blocked;
            else
                entry.status = Status::unknown;

            entries_[path] = entry;
        }
    }
    catch (const YAML::Exception& e)
    {
        dc_log ("ProbeCache: failed to load: %s", e.what());
    }

    // Save immediately to persist any crash-recovery changes
    save();
}

void ProbeCache::save() const
{
    YAML::Emitter out;
    out << YAML::BeginMap;
    out << YAML::Key << "modules";
    out << YAML::Value << YAML::BeginMap;

    for (const auto& [path, entry] : entries_)
    {
        const char* statusStr = "unknown";
        if (entry.status == Status::safe)
            statusStr = "safe";
        else if (entry.status == Status::blocked)
            statusStr = "blocked";

        out << YAML::Key << path;
        out << YAML::Value << YAML::BeginMap;
        out << YAML::Key << "mtime" << YAML::Value << entry.mtime;
        out << YAML::Key << "status" << YAML::Value << statusStr;
        out << YAML::EndMap;
    }

    out << YAML::EndMap;
    out << YAML::EndMap;

    std::filesystem::create_directories (cacheFile_.parent_path());
    dc::writeStringToFile (cacheFile_, out.c_str());
}

ProbeCache::Status ProbeCache::getStatus (const std::filesystem::path& bundlePath) const
{
    auto it = entries_.find (bundlePath.string());
    if (it == entries_.end())
        return Status::unknown;

    auto currentMtime = getMtime (bundlePath);
    if (currentMtime != it->second.mtime)
        return Status::unknown;  // stale entry

    return it->second.status;
}

void ProbeCache::setStatus (const std::filesystem::path& bundlePath, Status status)
{
    entries_[bundlePath.string()] = { getMtime (bundlePath), status };
}

void ProbeCache::resetStatus (const std::filesystem::path& bundlePath)
{
    setStatus (bundlePath, Status::unknown);

    // Clear any leftover pedal for this path
    if (auto pedal = checkPedal())
    {
        if (*pedal == bundlePath)
            clearPedal();
    }
}

std::vector<std::filesystem::path> ProbeCache::getBlockedPlugins() const
{
    std::vector<std::filesystem::path> result;

    for (const auto& [path, entry] : entries_)
    {
        if (entry.status == Status::blocked)
            result.emplace_back (path);
    }

    return result;
}

void ProbeCache::resetAllBlocked()
{
    for (auto& [path, entry] : entries_)
    {
        if (entry.status == Status::blocked)
            entry.status = Status::unknown;
    }

    save();
}

void ProbeCache::setPedal (const std::filesystem::path& bundlePath)
{
    std::filesystem::create_directories (pedalFile_.parent_path());
    dc::writeStringToFile (pedalFile_, bundlePath.string());
}

void ProbeCache::clearPedal()
{
    std::error_code ec;
    std::filesystem::remove (pedalFile_, ec);
}

std::optional<std::filesystem::path> ProbeCache::checkPedal() const
{
    auto content = dc::readFileToString (pedalFile_);
    if (content.empty())
        return std::nullopt;
    return std::filesystem::path (content);
}

std::int64_t ProbeCache::getMtime (const std::filesystem::path& path)
{
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time (path, ec);
    if (ec)
        return 0;

    auto sctp = std::chrono::time_point_cast<std::chrono::seconds> (
        ftime - std::filesystem::file_time_type::clock::now()
        + std::chrono::system_clock::now());
    return sctp.time_since_epoch().count();
}

} // namespace dc

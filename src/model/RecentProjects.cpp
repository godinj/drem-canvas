#include "RecentProjects.h"
#include "serialization/SessionReader.h"
#include "dc/foundation/file_utils.h"
#include "dc/foundation/time.h"
#include <yaml-cpp/yaml.h>
#include <algorithm>
#include <fstream>

namespace dc
{

void RecentProjects::load()
{
    entries.clear();

    auto file = getRecentProjectsFile();
    if (! std::filesystem::exists (file))
        return;

    try
    {
        auto root = YAML::LoadFile (file.string());
        auto projects = root["recent_projects"];

        if (! projects.IsDefined() || ! projects.IsSequence())
            return;

        for (auto node : projects)
        {
            RecentProjectEntry entry;
            entry.path = node["path"].as<std::string> ("");
            entry.displayName = node["name"].as<std::string> ("");
            entry.lastAccessed = node["last_accessed"].as<int64_t> (0);

            if (! entry.path.empty())
                entries.push_back (std::move (entry));
        }
    }
    catch (const YAML::Exception&)
    {
        entries.clear();
    }

    pruneInvalid();
}

void RecentProjects::save()
{
    auto file = getRecentProjectsFile();
    std::filesystem::create_directories (file.parent_path());

    YAML::Emitter emitter;
    emitter << YAML::BeginMap;
    emitter << YAML::Key << "recent_projects" << YAML::Value;
    emitter << YAML::BeginSeq;

    for (auto& entry : entries)
    {
        emitter << YAML::BeginMap;
        emitter << YAML::Key << "path" << YAML::Value << entry.path;
        emitter << YAML::Key << "name" << YAML::Value << entry.displayName;
        emitter << YAML::Key << "last_accessed" << YAML::Value << entry.lastAccessed;
        emitter << YAML::EndMap;
    }

    emitter << YAML::EndSeq;
    emitter << YAML::EndMap;

    dc::writeStringToFile (file, emitter.c_str());
}

void RecentProjects::addProject (const std::filesystem::path& dir)
{
    auto fullPath = dir.string();
    auto name = dir.filename().string();
    auto now = dc::currentTimeMillis() / 1000;

    // Remove existing entry with the same path
    entries.erase (
        std::remove_if (entries.begin(), entries.end(),
            [&fullPath] (const RecentProjectEntry& e) { return e.path == fullPath; }),
        entries.end());

    // Prepend new entry
    entries.insert (entries.begin(), { fullPath, name, now });

    // Truncate to max
    if (static_cast<int> (entries.size()) > maxRecentProjects)
        entries.resize (static_cast<size_t> (maxRecentProjects));

    save();
}

void RecentProjects::removeProject (const std::string& path)
{
    entries.erase (
        std::remove_if (entries.begin(), entries.end(),
            [&path] (const RecentProjectEntry& e) { return e.path == path; }),
        entries.end());

    save();
}

void RecentProjects::pruneInvalid()
{
    entries.erase (
        std::remove_if (entries.begin(), entries.end(),
            [] (const RecentProjectEntry& e)
            {
                return ! SessionReader::isValidSessionDirectory (e.path);
            }),
        entries.end());
}

std::filesystem::path RecentProjects::getRecentProjectsFile()
{
    return dc::getUserAppDataDirectory() / "recent.yaml";
}

} // namespace dc

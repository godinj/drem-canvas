#include "RecentProjects.h"
#include "serialization/SessionReader.h"
#include <yaml-cpp/yaml.h>
#include <algorithm>

namespace dc
{

void RecentProjects::load()
{
    entries.clear();

    auto file = getRecentProjectsFile();
    if (! file.existsAsFile())
        return;

    try
    {
        auto root = YAML::LoadFile (file.getFullPathName().toStdString());
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
    file.getParentDirectory().createDirectory();

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

    // Atomic write: write to .tmp, then move into place
    auto tmpFile = file.getSiblingFile (file.getFileName() + ".tmp");
    if (tmpFile.replaceWithText (juce::String (emitter.c_str())))
        tmpFile.moveFileTo (file);
}

void RecentProjects::addProject (const juce::File& dir)
{
    auto fullPath = dir.getFullPathName().toStdString();
    auto name = dir.getFileName().toStdString();
    auto now = juce::Time::currentTimeMillis() / 1000;

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
                return ! SessionReader::isValidSessionDirectory (juce::File (e.path));
            }),
        entries.end());
}

juce::File RecentProjects::getRecentProjectsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("DremCanvas")
               .getChildFile ("recent.yaml");
}

} // namespace dc

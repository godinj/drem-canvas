#pragma once

#include <JuceHeader.h>
#include <string>
#include <vector>

namespace dc
{

struct RecentProjectEntry
{
    std::string path;
    std::string displayName;
    int64_t lastAccessed = 0;
};

class RecentProjects
{
public:
    static constexpr int maxRecentProjects = 15;

    void load();
    void save();

    void addProject (const juce::File& dir);
    void removeProject (const std::string& path);

    const std::vector<RecentProjectEntry>& getEntries() const { return entries; }

private:
    void pruneInvalid();
    static juce::File getRecentProjectsFile();

    std::vector<RecentProjectEntry> entries;
};

} // namespace dc

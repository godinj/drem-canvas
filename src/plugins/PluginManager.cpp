#include "PluginManager.h"
#include "dc/foundation/file_utils.h"

namespace dc
{

PluginManager::PluginManager()
{
    juce::addDefaultFormatsToManager (formatManager);
}

void PluginManager::scanForPlugins()
{
    scanDefaultPaths();
    savePluginList (getDefaultPluginListFile());
}

void PluginManager::scanDefaultPaths()
{
    for (int i = 0; i < formatManager.getNumFormats(); ++i)
    {
        auto* format = formatManager.getFormat (i);

        auto defaultLocations = format->getDefaultLocationsToSearch();

        juce::PluginDirectoryScanner scanner (
            knownPlugins,
            *format,
            defaultLocations,
            true,   // recursive
            juce::File()  // JUCE API boundary — dead-mans-pedal file (none)
        );

        juce::String pluginName; // JUCE API boundary — scanner output param

        while (scanner.scanNextFile (true, pluginName))
        {
            // Keep scanning until all files have been checked
        }
    }
}

void PluginManager::savePluginList (const std::filesystem::path& file) const
{
    if (auto xml = knownPlugins.createXml())
    {
        std::filesystem::create_directories (file.parent_path());
        dc::writeStringToFile (file, xml->toString().toStdString());
    }
}

void PluginManager::loadPluginList (const std::filesystem::path& file)
{
    if (auto xml = juce::parseXML (dc::readFileToString (file)))
    {
        knownPlugins.recreateFromXml (*xml);
    }
}

std::filesystem::path PluginManager::getDefaultPluginListFile() const
{
    return dc::getUserAppDataDirectory() / "pluginList.xml";
}

} // namespace dc

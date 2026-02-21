#include "PluginManager.h"

namespace dc
{

PluginManager::PluginManager()
{
    juce::addDefaultFormatsToManager (formatManager);
}

void PluginManager::scanForPlugins()
{
    scanDefaultPaths();
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
            juce::File()  // dead-mans-pedal file (none)
        );

        juce::String pluginName;

        while (scanner.scanNextFile (true, pluginName))
        {
            // Keep scanning until all files have been checked
        }
    }
}

void PluginManager::savePluginList (const juce::File& file) const
{
    if (auto xml = knownPlugins.createXml())
    {
        file.getParentDirectory().createDirectory();
        xml->writeTo (file);
    }
}

void PluginManager::loadPluginList (const juce::File& file)
{
    if (auto xml = juce::parseXML (file))
    {
        knownPlugins.recreateFromXml (*xml);
    }
}

juce::File PluginManager::getDefaultPluginListFile() const
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("DremCanvas")
               .getChildFile ("pluginList.xml");
}

} // namespace dc

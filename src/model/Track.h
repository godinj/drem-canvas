#pragma once
#include <string>
#include <filesystem>
#include "dc/foundation/types.h"
#include "Project.h"

namespace dc
{

class Track
{
public:
    explicit Track (const PropertyTree& state);

    bool isValid() const { return state.isValid(); }

    std::string getName() const;
    void setName (const std::string& name, UndoManager* um = nullptr);

    float getVolume() const;
    void setVolume (float vol, UndoManager* um = nullptr);

    float getPan() const;
    void setPan (float p, UndoManager* um = nullptr);

    bool isMuted() const;
    void setMuted (bool m, UndoManager* um = nullptr);

    bool isSolo() const;
    void setSolo (bool s, UndoManager* um = nullptr);

    bool isArmed() const;
    void setArmed (bool a, UndoManager* um = nullptr);

    dc::Colour getColour() const;

    // Clip management
    PropertyTree addAudioClip (const std::filesystem::path& sourceFile, int64_t startPosition, int64_t length);
    PropertyTree addMidiClip (int64_t startPosition, int64_t length);
    int getNumClips() const;
    PropertyTree getClip (int index) const;
    void removeClip (int index, UndoManager* um = nullptr);

    // Plugin chain management
    PropertyTree getPluginChain();
    PropertyTree addPlugin (const std::string& name, const std::string& format,
                            const std::string& manufacturer, int uniqueId,
                            const std::string& fileOrIdentifier,
                            UndoManager* um = nullptr);
    void removePlugin (int index, UndoManager* um = nullptr);
    void movePlugin (int fromIndex, int toIndex, UndoManager* um = nullptr);
    int getNumPlugins() const;
    PropertyTree getPlugin (int index) const;
    void setPluginEnabled (int index, bool enabled, UndoManager* um = nullptr);
    bool isPluginEnabled (int index) const;
    void setPluginState (int index, const std::string& base64State, UndoManager* um = nullptr);

    PropertyTree& getState() { return state; }
    const PropertyTree& getState() const { return state; }

private:
    PropertyTree state;
};

} // namespace dc

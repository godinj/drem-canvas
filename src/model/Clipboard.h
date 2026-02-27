#pragma once
#include <JuceHeader.h>

namespace dc
{

class Clipboard
{
public:
    enum ContentType { Empty, ClipContent, NoteContent };

    struct ClipEntry
    {
        juce::ValueTree clipData;
        int trackOffset = 0;       // relative to topmost yanked track
        int64_t timeOffset = 0;    // samples from earliest clip's startPosition
    };

    struct NoteEntry
    {
        juce::ValueTree noteData;
        double beatOffset = 0.0;   // beats from earliest note's startBeat
    };

    void storeClips (const juce::Array<ClipEntry>& entries, bool linewise);
    void storeNotes (const juce::Array<NoteEntry>& entries);
    void clear();

    ContentType getContentType() const { return contentType; }
    bool isEmpty() const { return contentType == Empty; }
    bool hasClips() const { return contentType == ClipContent; }
    bool hasNotes() const { return contentType == NoteContent; }
    bool isLinewise() const { return linewise; }

    const juce::Array<ClipEntry>& getClipEntries() const { return clipEntries; }
    const juce::Array<NoteEntry>& getNoteEntries() const { return noteEntries; }

    int getTrackSpan() const;

private:
    ContentType contentType = Empty;
    bool linewise = false;
    juce::Array<ClipEntry> clipEntries;
    juce::Array<NoteEntry> noteEntries;
};

} // namespace dc

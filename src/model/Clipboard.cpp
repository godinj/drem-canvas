#include "Clipboard.h"

namespace dc
{

void Clipboard::storeClips (const juce::Array<ClipEntry>& entries, bool isLinewise)
{
    clipEntries.clear();
    noteEntries.clear();

    for (auto& e : entries)
        clipEntries.add ({ e.clipData.createCopy(), e.trackOffset, e.timeOffset });

    linewise = isLinewise;
    contentType = clipEntries.isEmpty() ? Empty : ClipContent;
}

void Clipboard::storeNotes (const juce::Array<NoteEntry>& entries)
{
    clipEntries.clear();
    noteEntries.clear();

    for (auto& e : entries)
        noteEntries.add ({ e.noteData.createCopy(), e.beatOffset });

    linewise = false;
    contentType = noteEntries.isEmpty() ? Empty : NoteContent;
}

void Clipboard::clear()
{
    clipEntries.clear();
    noteEntries.clear();
    contentType = Empty;
    linewise = false;
}

int Clipboard::getTrackSpan() const
{
    int maxOffset = 0;

    for (auto& e : clipEntries)
        maxOffset = std::max (maxOffset, e.trackOffset);

    return maxOffset + 1;
}

} // namespace dc

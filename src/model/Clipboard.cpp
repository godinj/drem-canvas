#include "Clipboard.h"

namespace dc
{

const Clipboard::RegisterEntry Clipboard::emptyRegister {};

// ─── RegisterEntry ──────────────────────────────────────────────────────────

int Clipboard::RegisterEntry::getTrackSpan() const
{
    int maxOffset = 0;
    for (auto& e : clipEntries)
        maxOffset = std::max (maxOffset, e.trackOffset);
    return maxOffset + 1;
}

void Clipboard::RegisterEntry::clear()
{
    clipEntries.clear();
    noteEntries.clear();
    type = Empty;
    linewise = false;
}

// ─── Register validation ────────────────────────────────────────────────────

bool Clipboard::isValidRegister (char c)
{
    return c == '\0'
        || isNamedRegister (c) || isAppendRegister (c)
        || isNumberedRegister (c);
}

bool Clipboard::isNamedRegister (char c)   { return c >= 'a' && c <= 'z'; }
bool Clipboard::isAppendRegister (char c)  { return c >= 'A' && c <= 'Z'; }
bool Clipboard::isNumberedRegister (char c) { return c >= '0' && c <= '9'; }

// ─── Private helpers ────────────────────────────────────────────────────────

Clipboard::RegisterEntry& Clipboard::resolve (char reg)
{
    if (reg == '\0')                 return unnamed;
    if (isNamedRegister (reg))       return named[reg - 'a'];
    if (isAppendRegister (reg))      return named[reg - 'A'];
    if (isNumberedRegister (reg))    return numbered[reg - '0'];
    return unnamed;
}

const Clipboard::RegisterEntry& Clipboard::resolveConst (char reg) const
{
    if (reg == '\0')                 return unnamed;
    if (isNamedRegister (reg))       return named[reg - 'a'];
    if (isAppendRegister (reg))      return named[reg - 'A'];
    if (isNumberedRegister (reg))    return numbered[reg - '0'];
    return unnamed;
}

void Clipboard::setRegister (RegisterEntry& reg,
                             const juce::Array<ClipEntry>& entries, bool isLinewise)
{
    reg.clipEntries.clear();
    reg.noteEntries.clear();

    for (auto& e : entries)
        reg.clipEntries.add ({ e.clipData.createCopy(), e.trackOffset, e.timeOffset });

    reg.linewise = isLinewise;
    reg.type = reg.clipEntries.isEmpty() ? RegisterEntry::Empty
                                         : RegisterEntry::ClipContent;
}

void Clipboard::setRegister (RegisterEntry& reg,
                             const juce::Array<NoteEntry>& entries)
{
    reg.clipEntries.clear();
    reg.noteEntries.clear();

    for (auto& e : entries)
        reg.noteEntries.add ({ e.noteData.createCopy(), e.beatOffset });

    reg.linewise = false;
    reg.type = reg.noteEntries.isEmpty() ? RegisterEntry::Empty
                                         : RegisterEntry::NoteContent;
}

void Clipboard::appendRegister (RegisterEntry& reg,
                                const juce::Array<ClipEntry>& entries)
{
    for (auto& e : entries)
        reg.clipEntries.add ({ e.clipData.createCopy(), e.trackOffset, e.timeOffset });

    if (! reg.clipEntries.isEmpty())
        reg.type = RegisterEntry::ClipContent;
}

void Clipboard::appendRegister (RegisterEntry& reg,
                                const juce::Array<NoteEntry>& entries)
{
    for (auto& e : entries)
        reg.noteEntries.add ({ e.noteData.createCopy(), e.beatOffset });

    if (! reg.noteEntries.isEmpty())
        reg.type = RegisterEntry::NoteContent;
}

void Clipboard::rotateDeleteHistory()
{
    // Shift "1→"2→...→"9 (oldest in "9 is dropped)
    numbered[9] = std::move (numbered[8]);
    numbered[8] = std::move (numbered[7]);
    numbered[7] = std::move (numbered[6]);
    numbered[6] = std::move (numbered[5]);
    numbered[5] = std::move (numbered[4]);
    numbered[4] = std::move (numbered[3]);
    numbered[3] = std::move (numbered[2]);
    numbered[2] = std::move (numbered[1]);
    numbered[1].clear();
}

// ─── Public store methods ───────────────────────────────────────────────────

void Clipboard::storeClips (char reg, const juce::Array<ClipEntry>& entries,
                            bool linewise, bool isYank)
{
    // Always write the unnamed register
    setRegister (unnamed, entries, linewise);

    if (isAppendRegister (reg))
    {
        appendRegister (resolve (reg), entries);
    }
    else if (reg != '\0')
    {
        setRegister (resolve (reg), entries, linewise);
    }

    if (isYank)
    {
        // "0 always gets the latest yank
        setRegister (numbered[0], entries, linewise);
    }
    else
    {
        // Rotate delete history: "1-"9
        rotateDeleteHistory();
        setRegister (numbered[1], entries, linewise);
    }
}

void Clipboard::storeNotes (char reg, const juce::Array<NoteEntry>& entries,
                            bool isYank)
{
    setRegister (unnamed, entries);

    if (isAppendRegister (reg))
    {
        appendRegister (resolve (reg), entries);
    }
    else if (reg != '\0')
    {
        setRegister (resolve (reg), entries);
    }

    if (isYank)
    {
        setRegister (numbered[0], entries);
    }
    else
    {
        rotateDeleteHistory();
        setRegister (numbered[1], entries);
    }
}

// Backwards compat: unnamed register, yank semantics
void Clipboard::storeClips (const juce::Array<ClipEntry>& entries, bool linewise)
{
    storeClips ('\0', entries, linewise, true);
}

void Clipboard::storeNotes (const juce::Array<NoteEntry>& entries)
{
    storeNotes ('\0', entries, true);
}

// ─── Read access ────────────────────────────────────────────────────────────

const Clipboard::RegisterEntry& Clipboard::get (char reg) const
{
    return resolveConst (reg);
}

} // namespace dc

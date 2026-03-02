#pragma once
#include "dc/model/PropertyTree.h"
#include <vector>

namespace dc
{

class Clipboard
{
public:
    struct ClipEntry
    {
        PropertyTree clipData;
        int trackOffset = 0;       // relative to topmost yanked track
        int64_t timeOffset = 0;    // samples from earliest clip's startPosition
    };

    struct NoteEntry
    {
        PropertyTree noteData;
        double beatOffset = 0.0;   // beats from earliest note's startBeat
    };

    struct RegisterEntry
    {
        enum ContentType { Empty, ClipContent, NoteContent };

        ContentType type = Empty;
        bool linewise = false;
        std::vector<ClipEntry> clipEntries;
        std::vector<NoteEntry> noteEntries;

        bool isEmpty() const    { return type == Empty; }
        bool hasClips() const   { return type == ClipContent; }
        bool hasNotes() const   { return type == NoteContent; }

        int getTrackSpan() const;
        void clear();
    };

    // ─── Register-aware storage ─────────────────────────────────────
    // reg = '\0' means unnamed register only (no explicit "x prefix)
    // isYank = true  → also writes "0 (yank register)
    // isYank = false → also rotates "1-"9 (delete history)
    void storeClips (char reg, const std::vector<ClipEntry>& entries,
                     bool linewise, bool isYank);
    void storeNotes (char reg, const std::vector<NoteEntry>& entries,
                     bool isYank);

    // Convenience: store into unnamed register (backwards compat)
    void storeClips (const std::vector<ClipEntry>& entries, bool linewise);
    void storeNotes (const std::vector<NoteEntry>& entries);

    // ─── Read access ────────────────────────────────────────────────
    // reg = '\0' means unnamed register
    const RegisterEntry& get (char reg = '\0') const;

    // Shortcut accessors on the unnamed register (backwards compat)
    bool isEmpty() const    { return unnamed.isEmpty(); }
    bool hasClips() const   { return unnamed.hasClips(); }
    bool hasNotes() const   { return unnamed.hasNotes(); }
    bool isLinewise() const { return unnamed.linewise; }

    const std::vector<ClipEntry>& getClipEntries() const { return unnamed.clipEntries; }
    const std::vector<NoteEntry>& getNoteEntries() const { return unnamed.noteEntries; }
    int getTrackSpan() const { return unnamed.getTrackSpan(); }

    // ─── Register validation ────────────────────────────────────────
    static bool isValidRegister (char c);
    static bool isNamedRegister (char c);       // a-z
    static bool isAppendRegister (char c);      // A-Z
    static bool isNumberedRegister (char c);    // 0-9

private:
    void setRegister (RegisterEntry& reg, const std::vector<ClipEntry>& entries, bool linewise);
    void setRegister (RegisterEntry& reg, const std::vector<NoteEntry>& entries);
    void appendRegister (RegisterEntry& reg, const std::vector<ClipEntry>& entries);
    void appendRegister (RegisterEntry& reg, const std::vector<NoteEntry>& entries);
    void rotateDeleteHistory();

    RegisterEntry& resolve (char reg);
    const RegisterEntry& resolveConst (char reg) const;

    RegisterEntry unnamed;
    RegisterEntry named[26];        // "a - "z
    RegisterEntry numbered[10];     // "0 = yank, "1-"9 = delete history

    static const RegisterEntry emptyRegister;
};

} // namespace dc

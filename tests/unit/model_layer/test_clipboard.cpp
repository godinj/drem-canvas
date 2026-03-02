#include <catch2/catch_test_macros.hpp>
#include "model/Clipboard.h"

TEST_CASE ("Clipboard starts empty", "[model_layer][clipboard]")
{
    dc::Clipboard clip;

    CHECK (clip.isEmpty());
    CHECK_FALSE (clip.hasClips());
    CHECK_FALSE (clip.hasNotes());
}

TEST_CASE ("Clipboard storeClips into unnamed register", "[model_layer][clipboard]")
{
    dc::Clipboard clip;

    dc::PropertyTree clipData (dc::PropertyId ("AUDIO_CLIP"));
    clipData.setProperty (dc::PropertyId ("startPosition"), dc::Variant (int64_t (1000)), nullptr);

    std::vector<dc::Clipboard::ClipEntry> entries;
    entries.push_back ({ clipData, 0, 0 });

    clip.storeClips (entries, true); // linewise, unnamed register

    CHECK_FALSE (clip.isEmpty());
    CHECK (clip.hasClips());
    CHECK (clip.isLinewise());
    CHECK (clip.getClipEntries().size() == 1);
}

TEST_CASE ("Clipboard storeNotes into unnamed register", "[model_layer][clipboard]")
{
    dc::Clipboard clip;

    dc::PropertyTree noteData (dc::PropertyId ("NOTE"));
    noteData.setProperty (dc::PropertyId ("startBeat"), dc::Variant (1.0), nullptr);

    std::vector<dc::Clipboard::NoteEntry> entries;
    entries.push_back ({ noteData, 0.0 });

    clip.storeNotes (entries);

    CHECK_FALSE (clip.isEmpty());
    CHECK (clip.hasNotes());
    CHECK_FALSE (clip.isLinewise());
    CHECK (clip.getNoteEntries().size() == 1);
}

TEST_CASE ("Clipboard named registers a-z", "[model_layer][clipboard]")
{
    dc::Clipboard clip;

    dc::PropertyTree clipData (dc::PropertyId ("AUDIO_CLIP"));
    std::vector<dc::Clipboard::ClipEntry> entries;
    entries.push_back ({ clipData, 0, 0 });

    clip.storeClips ('a', entries, false, true);

    // Check named register "a
    auto& regA = clip.get ('a');
    CHECK (regA.hasClips());
    CHECK (regA.clipEntries.size() == 1);

    // Other named register should be empty
    auto& regB = clip.get ('b');
    CHECK (regB.isEmpty());
}

TEST_CASE ("Clipboard append register A-Z appends to a-z", "[model_layer][clipboard]")
{
    dc::Clipboard clip;

    dc::PropertyTree clipData1 (dc::PropertyId ("AUDIO_CLIP"));
    clipData1.setProperty (dc::PropertyId ("name"), dc::Variant (std::string ("clip1")), nullptr);
    std::vector<dc::Clipboard::ClipEntry> entries1;
    entries1.push_back ({ clipData1, 0, 0 });

    // Store into 'a'
    clip.storeClips ('a', entries1, false, true);

    dc::PropertyTree clipData2 (dc::PropertyId ("AUDIO_CLIP"));
    clipData2.setProperty (dc::PropertyId ("name"), dc::Variant (std::string ("clip2")), nullptr);
    std::vector<dc::Clipboard::ClipEntry> entries2;
    entries2.push_back ({ clipData2, 1, 0 });

    // Append to 'A' (which appends to 'a')
    clip.storeClips ('A', entries2, false, true);

    auto& regA = clip.get ('a');
    CHECK (regA.hasClips());
    CHECK (regA.clipEntries.size() == 2);
}

TEST_CASE ("Clipboard yank register '0' gets latest yank", "[model_layer][clipboard]")
{
    dc::Clipboard clip;

    dc::PropertyTree clipData (dc::PropertyId ("AUDIO_CLIP"));
    std::vector<dc::Clipboard::ClipEntry> entries;
    entries.push_back ({ clipData, 0, 0 });

    clip.storeClips ('\0', entries, false, true); // isYank=true

    auto& reg0 = clip.get ('0');
    CHECK (reg0.hasClips());
    CHECK (reg0.clipEntries.size() == 1);
}

TEST_CASE ("Clipboard delete rotates numbered registers 1-9", "[model_layer][clipboard]")
{
    dc::Clipboard clip;

    // Perform 3 deletes (isYank=false)
    for (int i = 0; i < 3; ++i)
    {
        dc::PropertyTree clipData (dc::PropertyId ("AUDIO_CLIP"));
        clipData.setProperty (dc::PropertyId ("index"), dc::Variant (i), nullptr);
        std::vector<dc::Clipboard::ClipEntry> entries;
        entries.push_back ({ clipData, 0, 0 });

        clip.storeClips ('\0', entries, false, false); // isYank=false
    }

    // "1 should have the most recent delete (index=2)
    auto& reg1 = clip.get ('1');
    CHECK (reg1.hasClips());

    // "2 should have the previous delete (index=1)
    auto& reg2 = clip.get ('2');
    CHECK (reg2.hasClips());

    // "3 should have the oldest delete (index=0)
    auto& reg3 = clip.get ('3');
    CHECK (reg3.hasClips());

    // "4 should be empty (only 3 deletes)
    auto& reg4 = clip.get ('4');
    CHECK (reg4.isEmpty());
}

TEST_CASE ("Clipboard getTrackSpan", "[model_layer][clipboard]")
{
    dc::Clipboard clip;

    dc::PropertyTree clipData1 (dc::PropertyId ("AUDIO_CLIP"));
    dc::PropertyTree clipData2 (dc::PropertyId ("AUDIO_CLIP"));

    std::vector<dc::Clipboard::ClipEntry> entries;
    entries.push_back ({ clipData1, 0, 0 });
    entries.push_back ({ clipData2, 2, 0 });

    clip.storeClips (entries, true);

    CHECK (clip.getTrackSpan() == 3); // trackOffset 0..2 -> span = 3
}

TEST_CASE ("Clipboard register validation", "[model_layer][clipboard]")
{
    CHECK (dc::Clipboard::isValidRegister ('\0'));
    CHECK (dc::Clipboard::isValidRegister ('a'));
    CHECK (dc::Clipboard::isValidRegister ('z'));
    CHECK (dc::Clipboard::isValidRegister ('A'));
    CHECK (dc::Clipboard::isValidRegister ('Z'));
    CHECK (dc::Clipboard::isValidRegister ('0'));
    CHECK (dc::Clipboard::isValidRegister ('9'));
    CHECK_FALSE (dc::Clipboard::isValidRegister ('!'));
    CHECK_FALSE (dc::Clipboard::isValidRegister (' '));

    CHECK (dc::Clipboard::isNamedRegister ('a'));
    CHECK_FALSE (dc::Clipboard::isNamedRegister ('A'));
    CHECK (dc::Clipboard::isAppendRegister ('A'));
    CHECK_FALSE (dc::Clipboard::isAppendRegister ('a'));
    CHECK (dc::Clipboard::isNumberedRegister ('0'));
    CHECK_FALSE (dc::Clipboard::isNumberedRegister ('a'));
}

TEST_CASE ("Clipboard RegisterEntry::clear resets state", "[model_layer][clipboard]")
{
    dc::Clipboard clip;

    dc::PropertyTree clipData (dc::PropertyId ("AUDIO_CLIP"));
    std::vector<dc::Clipboard::ClipEntry> entries;
    entries.push_back ({ clipData, 0, 0 });

    clip.storeClips (entries, true);
    CHECK (clip.hasClips());

    // Store an empty set overwrites
    std::vector<dc::Clipboard::NoteEntry> noteEntries;
    dc::PropertyTree noteData (dc::PropertyId ("NOTE"));
    noteEntries.push_back ({ noteData, 0.0 });
    clip.storeNotes (noteEntries);

    CHECK (clip.hasNotes());
    CHECK_FALSE (clip.hasClips()); // clips replaced by notes
}

TEST_CASE ("Clipboard deep copies stored data", "[model_layer][clipboard]")
{
    dc::Clipboard clip;

    dc::PropertyTree clipData (dc::PropertyId ("AUDIO_CLIP"));
    clipData.setProperty (dc::PropertyId ("startPosition"), dc::Variant (int64_t (1000)), nullptr);

    std::vector<dc::Clipboard::ClipEntry> entries;
    entries.push_back ({ clipData, 0, 0 });

    clip.storeClips (entries, false);

    // Modify original — clipboard should be unaffected
    clipData.setProperty (dc::PropertyId ("startPosition"), dc::Variant (int64_t (9999)), nullptr);

    auto& stored = clip.getClipEntries();
    REQUIRE (stored.size() == 1);
    CHECK (stored[0].clipData.getProperty (dc::PropertyId ("startPosition")).getIntOr (0) == 1000);
}

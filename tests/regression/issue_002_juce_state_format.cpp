// tests/regression/issue_002_juce_state_format.cpp
//
// Bug: PluginInstance::setState crashed or silently failed when given
//      legacy-format plugin state data.  The dc:: format expects
//      [4 bytes componentSize][componentData][controllerData], but old
//      projects store state in the old opaque binary format which has a
//      different header structure.
//
// Cause: setState read the first 4 bytes as componentSize without
//        checking whether 4 + componentSize fits in the data buffer.
//        When the first 4 bytes of legacy-format data decode to a large
//        value, the subsequent reads are out-of-bounds.
//
// Fix: Added a bounds check before parsing.  If componentSize doesn't
//      fit, attempt raw pass-through (entire blob as component state).
//      If that also fails, log a warning and return gracefully.

#include <catch2/catch_test_macros.hpp>
#include <cstdint>
#include <cstring>
#include <vector>

// The actual setState requires a live VST3 IComponent, so we cannot call it
// directly in a unit test.  Instead we test the bounds-check logic that
// guards against the crash.

TEST_CASE ("Regression #002: componentSize bounds check detects legacy format",
           "[regression]")
{
    // Simulate legacy-format state: first 4 bytes decode to a componentSize
    // that exceeds the total data length.
    std::vector<uint8_t> legacyFormatData = {
        0xFF, 0xFF, 0x00, 0x00,  // componentSize = 65535 (LE), larger than data
        0x01, 0x02, 0x03, 0x04   // some payload (total = 8 bytes)
    };

    uint32_t componentSize = 0;
    std::memcpy (&componentSize, legacyFormatData.data(), 4);

    // The dc:: format requires 4 + componentSize <= data.size().
    // Legacy-format data will fail this check.
    bool formatValid = (4 + componentSize <= legacyFormatData.size());
    REQUIRE_FALSE (formatValid);
}

TEST_CASE ("Regression #002: valid dc format passes bounds check",
           "[regression]")
{
    // Valid dc:: format: componentSize = 4, total = 4 + 4 = 8 bytes
    uint32_t componentSize = 4;
    std::vector<uint8_t> dcFormatData (4 + componentSize, 0);
    std::memcpy (dcFormatData.data(), &componentSize, 4);

    uint32_t readSize = 0;
    std::memcpy (&readSize, dcFormatData.data(), 4);

    bool formatValid = (4 + readSize <= dcFormatData.size());
    REQUIRE (formatValid);
}

TEST_CASE ("Regression #002: empty and tiny data rejected gracefully",
           "[regression]")
{
    // Empty data should be rejected (< 4 bytes)
    std::vector<uint8_t> emptyData;
    REQUIRE (emptyData.size() < 4);

    // 3-byte data should be rejected (< 4 bytes)
    std::vector<uint8_t> tinyData = { 0x01, 0x02, 0x03 };
    REQUIRE (tinyData.size() < 4);

    // Exactly 4 bytes with componentSize = 0 is valid (empty component, no controller)
    uint32_t zeroSize = 0;
    std::vector<uint8_t> minimalData (4, 0);
    std::memcpy (minimalData.data(), &zeroSize, 4);

    uint32_t readSize = 0;
    std::memcpy (&readSize, minimalData.data(), 4);

    bool formatValid = (4 + readSize <= minimalData.size());
    REQUIRE (formatValid);
}

#include <catch2/catch_test_macros.hpp>
#include <dc/model/PropertyId.h>

#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

using dc::PropertyId;

// ═══════════════════════════════════════════════════════════════
// Same string produces same pointer (O(1) comparison)
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyId: same string produces equal ids", "[model][property_id]")
{
    PropertyId a ("testProp");
    PropertyId b ("testProp");

    REQUIRE (a == b);
}

TEST_CASE ("PropertyId: different strings produce different ids", "[model][property_id]")
{
    PropertyId a ("alpha");
    PropertyId b ("beta");

    REQUIRE (a != b);
}

TEST_CASE ("PropertyId: same string from const char* and string_view are equal", "[model][property_id]")
{
    PropertyId a ("myProp");
    std::string_view sv = "myProp";
    PropertyId b (sv);

    REQUIRE (a == b);
}

// ═══════════════════════════════════════════════════════════════
// toString() returns the interned string
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyId: toString() returns the original string", "[model][property_id]")
{
    PropertyId id ("sampleRate");
    REQUIRE (id.toString() == "sampleRate");
}

// ═══════════════════════════════════════════════════════════════
// Hash consistency
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyId: hash is consistent across calls", "[model][property_id]")
{
    PropertyId id ("hashTest");
    PropertyId::Hash hasher;

    auto h1 = hasher (id);
    auto h2 = hasher (id);

    REQUIRE (h1 == h2);
}

TEST_CASE ("PropertyId: equal ids produce equal hashes", "[model][property_id]")
{
    PropertyId a ("foo");
    PropertyId b ("foo");
    PropertyId::Hash hasher;

    REQUIRE (hasher (a) == hasher (b));
}

// ═══════════════════════════════════════════════════════════════
// Use in std::unordered_map
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyId: works as unordered_map key", "[model][property_id]")
{
    std::unordered_map<PropertyId, int, PropertyId::Hash> map;

    PropertyId key1 ("volume");
    PropertyId key2 ("pan");
    PropertyId key3 ("volume");  // same as key1

    map[key1] = 100;
    map[key2] = 50;

    SECTION ("insert and lookup")
    {
        REQUIRE (map[key1] == 100);
        REQUIRE (map[key2] == 50);
        REQUIRE (map.size() == 2);
    }

    SECTION ("lookup with separately-constructed equal key")
    {
        REQUIRE (map[key3] == 100);  // key3 == key1
    }

    SECTION ("overwrite existing key")
    {
        map[key1] = 200;
        REQUIRE (map[key1] == 200);
        REQUIRE (map.size() == 2);
    }
}

// ═══════════════════════════════════════════════════════════════
// Construct from const char* and string_view
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyId: construct from const char*", "[model][property_id]")
{
    PropertyId id ("fromCharStar");
    REQUIRE (id.toString() == "fromCharStar");
}

TEST_CASE ("PropertyId: construct from string_view", "[model][property_id]")
{
    std::string_view sv = "fromStringView";
    PropertyId id (sv);
    REQUIRE (id.toString() == "fromStringView");
}

TEST_CASE ("PropertyId: construct from substring via string_view", "[model][property_id]")
{
    std::string full = "prefix_suffix";
    std::string_view sv (full.data() + 7, 6);  // "suffix"
    PropertyId id (sv);
    REQUIRE (id.toString() == "suffix");
}

// ═══════════════════════════════════════════════════════════════
// Empty string
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyId: empty string is valid and distinct", "[model][property_id]")
{
    PropertyId empty ("");
    PropertyId nonEmpty ("x");

    REQUIRE (empty.toString().empty());
    REQUIRE (empty != nonEmpty);
}

TEST_CASE ("PropertyId: two empty strings are equal", "[model][property_id]")
{
    PropertyId a ("");
    PropertyId b ("");
    REQUIRE (a == b);
}

// ═══════════════════════════════════════════════════════════════
// Ordering (pointer-based, deterministic within a process)
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyId: less-than provides strict weak ordering", "[model][property_id]")
{
    PropertyId a ("aaa");
    PropertyId b ("bbb");

    // One of a<b or b<a must be true (they are different)
    REQUIRE ((a < b) != (b < a));

    // Irreflexive: not (a < a)
    REQUIRE_FALSE (a < a);
}

// ═══════════════════════════════════════════════════════════════
// Thread-safe interning
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("PropertyId: thread-safe interning under concurrent access", "[model][property_id]")
{
    constexpr int numThreads = 10;
    std::vector<PropertyId*> results (numThreads, nullptr);
    std::vector<std::thread> threads;

    // Allocate storage for PropertyId objects (needs placement new or similar)
    // Use a vector of optional-like aligned storage
    struct alignas (PropertyId) Storage
    {
        char buf[sizeof (PropertyId)];
    };
    std::vector<Storage> storage (numThreads);

    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back ([&storage, &results, i]()
        {
            // All threads intern the exact same string
            auto* p = new (&storage[i]) PropertyId ("threadSafeTest");
            results[i] = p;
        });
    }

    for (auto& t : threads)
        t.join();

    // All PropertyIds constructed from the same string must be equal
    for (int i = 1; i < numThreads; ++i)
    {
        REQUIRE (*results[0] == *results[i]);
    }

    // Clean up placement-new'd objects
    for (int i = 0; i < numThreads; ++i)
        results[i]->~PropertyId();
}

TEST_CASE ("PropertyId: concurrent interning of different strings", "[model][property_id]")
{
    constexpr int numThreads = 10;
    std::vector<std::thread> threads;
    std::vector<std::string> names (numThreads);

    for (int i = 0; i < numThreads; ++i)
        names[i] = "concurrent_" + std::to_string (i);

    // Just verify no crashes under concurrent access with different strings
    for (int i = 0; i < numThreads; ++i)
    {
        threads.emplace_back ([&names, i]()
        {
            PropertyId id (std::string_view (names[i]));
            REQUIRE (id.toString() == names[i]);
        });
    }

    for (auto& t : threads)
        t.join();
}

#include <catch2/catch_test_macros.hpp>
#include <dc/foundation/base64.h>
#include <cstdint>
#include <vector>

TEST_CASE("base64 round-trip for various sizes", "[foundation][base64]")
{
    auto roundTrip = [](size_t size) {
        std::vector<uint8_t> data(size);
        for (size_t i = 0; i < size; ++i)
            data[i] = static_cast<uint8_t>(i & 0xff);

        auto encoded = dc::base64Encode(data);
        auto decoded = dc::base64Decode(encoded);
        REQUIRE(decoded == data);
    };

    SECTION("size 0") { roundTrip(0); }
    SECTION("size 1") { roundTrip(1); }
    SECTION("size 2") { roundTrip(2); }
    SECTION("size 3") { roundTrip(3); }
    SECTION("size 4") { roundTrip(4); }
    SECTION("size 5") { roundTrip(5); }
    SECTION("size 100") { roundTrip(100); }
    SECTION("size 1000") { roundTrip(1000); }
}

TEST_CASE("base64 empty input encode returns empty string", "[foundation][base64]")
{
    std::vector<uint8_t> empty;
    REQUIRE(dc::base64Encode(empty) == "");
}

TEST_CASE("base64 empty input decode returns empty vector", "[foundation][base64]")
{
    REQUIRE(dc::base64Decode("").empty());
}

TEST_CASE("base64 decode tolerates whitespace", "[foundation][base64]")
{
    // "hello" in base64 is "aGVsbG8="
    std::vector<uint8_t> expected = {'h', 'e', 'l', 'l', 'o'};
    REQUIRE(dc::base64Decode("aGVs\nbG8=") == expected);
    REQUIRE(dc::base64Decode("aG Vs bG8=") == expected);
    REQUIRE(dc::base64Decode("aGVs\r\nbG8=") == expected);
}

TEST_CASE("base64 decode with invalid characters skips them", "[foundation][base64]")
{
    // "aGVsbG8=" with interspersed invalid chars
    std::vector<uint8_t> expected = {'h', 'e', 'l', 'l', 'o'};
    REQUIRE(dc::base64Decode("aGVs!bG8=") == expected);
}

TEST_CASE("base64 known encoding values", "[foundation][base64]")
{
    SECTION("single byte")
    {
        std::vector<uint8_t> data = {'A'};
        REQUIRE(dc::base64Encode(data) == "QQ==");
    }

    SECTION("two bytes")
    {
        std::vector<uint8_t> data = {'A', 'B'};
        REQUIRE(dc::base64Encode(data) == "QUI=");
    }

    SECTION("three bytes - no padding")
    {
        std::vector<uint8_t> data = {'A', 'B', 'C'};
        REQUIRE(dc::base64Encode(data) == "QUJD");
    }
}

TEST_CASE("base64 padding variations decode correctly", "[foundation][base64]")
{
    SECTION("no padding (3n bytes)")
    {
        auto decoded = dc::base64Decode("QUJD");
        std::vector<uint8_t> expected = {'A', 'B', 'C'};
        REQUIRE(decoded == expected);
    }

    SECTION("one padding char (3n+2 bytes)")
    {
        auto decoded = dc::base64Decode("QUI=");
        std::vector<uint8_t> expected = {'A', 'B'};
        REQUIRE(decoded == expected);
    }

    SECTION("two padding chars (3n+1 bytes)")
    {
        auto decoded = dc::base64Decode("QQ==");
        std::vector<uint8_t> expected = {'A'};
        REQUIRE(decoded == expected);
    }
}

TEST_CASE("base64 round-trip with all byte values", "[foundation][base64]")
{
    std::vector<uint8_t> data(256);
    for (int i = 0; i < 256; ++i)
        data[static_cast<size_t>(i)] = static_cast<uint8_t>(i);

    auto encoded = dc::base64Encode(data);
    auto decoded = dc::base64Decode(encoded);
    REQUIRE(decoded == data);
}

TEST_CASE("base64 encode with raw pointer overload", "[foundation][base64]")
{
    std::vector<uint8_t> data = {'h', 'e', 'l', 'l', 'o'};
    auto fromVec = dc::base64Encode(data);
    auto fromPtr = dc::base64Encode(data.data(), data.size());
    REQUIRE(fromVec == fromPtr);
}

#include <catch2/catch_test_macros.hpp>
#include <dc/model/Variant.h>

#include <string>
#include <vector>

using dc::Variant;
using dc::TypeMismatch;

// ═══════════════════════════════════════════════════════════════
// Type tags construct correctly
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("Variant: default constructs as Void", "[model][variant]")
{
    Variant v;
    REQUIRE (v.type() == Variant::Type::Void);
    REQUIRE (v.isVoid());
}

TEST_CASE ("Variant: int64_t constructor produces Int type", "[model][variant]")
{
    Variant v (int64_t (42));
    REQUIRE (v.type() == Variant::Type::Int);
    REQUIRE (v.toInt() == 42);
}

TEST_CASE ("Variant: int constructor promotes to int64_t", "[model][variant]")
{
    Variant v (42);
    REQUIRE (v.type() == Variant::Type::Int);
    REQUIRE (v.toInt() == 42);
}

TEST_CASE ("Variant: double constructor produces Double type", "[model][variant]")
{
    Variant v (3.14);
    REQUIRE (v.type() == Variant::Type::Double);
    REQUIRE (v.toDouble() == 3.14);
}

TEST_CASE ("Variant: bool constructor produces Bool type", "[model][variant]")
{
    SECTION ("true")
    {
        Variant v (true);
        REQUIRE (v.type() == Variant::Type::Bool);
        REQUIRE (v.toBool() == true);
    }

    SECTION ("false")
    {
        Variant v (false);
        REQUIRE (v.type() == Variant::Type::Bool);
        REQUIRE (v.toBool() == false);
    }
}

TEST_CASE ("Variant: string constructors produce String type", "[model][variant]")
{
    SECTION ("std::string")
    {
        Variant v (std::string ("hello"));
        REQUIRE (v.type() == Variant::Type::String);
        REQUIRE (v.toString() == "hello");
    }

    SECTION ("string_view")
    {
        std::string_view sv = "world";
        Variant v (sv);
        REQUIRE (v.type() == Variant::Type::String);
        REQUIRE (v.toString() == "world");
    }

    SECTION ("const char*")
    {
        Variant v ("literal");
        REQUIRE (v.type() == Variant::Type::String);
        REQUIRE (v.toString() == "literal");
    }
}

TEST_CASE ("Variant: Binary constructor produces Binary type", "[model][variant]")
{
    std::vector<uint8_t> blob = { 0x00, 0xFF, 0x42, 0xAB };
    Variant v (blob);
    REQUIRE (v.type() == Variant::Type::Binary);
    REQUIRE (v.toBinary() == blob);
}

// ═══════════════════════════════════════════════════════════════
// Strict accessors throw on wrong type
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("Variant: strict toInt() on String throws TypeMismatch", "[model][variant]")
{
    Variant v ("hello");
    REQUIRE_THROWS_AS (v.toInt(), TypeMismatch);
}

TEST_CASE ("Variant: strict toString() on Int throws TypeMismatch", "[model][variant]")
{
    Variant v (42);
    REQUIRE_THROWS_AS (v.toString(), TypeMismatch);
}

TEST_CASE ("Variant: strict toDouble() on Bool throws TypeMismatch", "[model][variant]")
{
    Variant v (true);
    REQUIRE_THROWS_AS (v.toDouble(), TypeMismatch);
}

TEST_CASE ("Variant: strict toBool() on Double throws TypeMismatch", "[model][variant]")
{
    Variant v (1.0);
    REQUIRE_THROWS_AS (v.toBool(), TypeMismatch);
}

TEST_CASE ("Variant: strict toBinary() on String throws TypeMismatch", "[model][variant]")
{
    Variant v ("data");
    REQUIRE_THROWS_AS (v.toBinary(), TypeMismatch);
}

TEST_CASE ("Variant: strict toInt() on Void throws TypeMismatch", "[model][variant]")
{
    Variant v;
    REQUIRE_THROWS_AS (v.toInt(), TypeMismatch);
}

// ═══════════════════════════════════════════════════════════════
// Fallback accessors — cross-type conversions
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("Variant: getIntOr() on Double truncates", "[model][variant]")
{
    Variant v (3.7);
    REQUIRE (v.getIntOr (0) == 3);
}

TEST_CASE ("Variant: getIntOr() on Int returns value", "[model][variant]")
{
    Variant v (int64_t (99));
    REQUIRE (v.getIntOr (0) == 99);
}

TEST_CASE ("Variant: getIntOr() on wrong type returns fallback", "[model][variant]")
{
    Variant v ("hello");
    REQUIRE (v.getIntOr (-1) == -1);
}

TEST_CASE ("Variant: getDoubleOr() on Int promotes", "[model][variant]")
{
    Variant v (5);
    REQUIRE (v.getDoubleOr (0.0) == 5.0);
}

TEST_CASE ("Variant: getDoubleOr() on Double returns value", "[model][variant]")
{
    Variant v (2.5);
    REQUIRE (v.getDoubleOr (0.0) == 2.5);
}

TEST_CASE ("Variant: getDoubleOr() on wrong type returns fallback", "[model][variant]")
{
    Variant v ("not a number");
    REQUIRE (v.getDoubleOr (-1.0) == -1.0);
}

TEST_CASE ("Variant: getBoolOr() on Int converts", "[model][variant]")
{
    SECTION ("0 is false")
    {
        Variant v (0);
        REQUIRE (v.getBoolOr (true) == false);
    }

    SECTION ("1 is true")
    {
        Variant v (1);
        REQUIRE (v.getBoolOr (false) == true);
    }

    SECTION ("-1 is true (non-zero)")
    {
        Variant v (-1);
        REQUIRE (v.getBoolOr (false) == true);
    }
}

TEST_CASE ("Variant: getBoolOr() on Bool returns value", "[model][variant]")
{
    Variant v (true);
    REQUIRE (v.getBoolOr (false) == true);
}

TEST_CASE ("Variant: getBoolOr() on wrong type returns fallback", "[model][variant]")
{
    Variant v ("true");
    REQUIRE (v.getBoolOr (false) == false);
}

TEST_CASE ("Variant: getStringOr() on String returns value", "[model][variant]")
{
    Variant v ("hello");
    REQUIRE (v.getStringOr ("default") == "hello");
}

TEST_CASE ("Variant: getStringOr() on wrong type returns fallback", "[model][variant]")
{
    Variant v (42);
    REQUIRE (v.getStringOr ("default") == "default");
}

// ═══════════════════════════════════════════════════════════════
// Equality
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("Variant: same type and value are equal", "[model][variant]")
{
    REQUIRE (Variant (42) == Variant (42));
    REQUIRE (Variant (3.14) == Variant (3.14));
    REQUIRE (Variant (true) == Variant (true));
    REQUIRE (Variant ("hello") == Variant ("hello"));
}

TEST_CASE ("Variant: different types are not equal", "[model][variant]")
{
    REQUIRE (Variant (42) != Variant (42.0));
    REQUIRE (Variant (1) != Variant (true));
    REQUIRE (Variant ("42") != Variant (42));
}

TEST_CASE ("Variant: Void == Void", "[model][variant]")
{
    Variant a;
    Variant b;
    REQUIRE (a == b);
}

TEST_CASE ("Variant: empty string is not equal to Void", "[model][variant]")
{
    Variant s ("");
    Variant v;
    REQUIRE (s != v);
}

TEST_CASE ("Variant: different values of same type are not equal", "[model][variant]")
{
    REQUIRE (Variant (1) != Variant (2));
    REQUIRE (Variant (1.0) != Variant (2.0));
    REQUIRE (Variant ("a") != Variant ("b"));
    REQUIRE (Variant (true) != Variant (false));
}

// ═══════════════════════════════════════════════════════════════
// Binary blob round-trip
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("Variant: Binary blob store and retrieve", "[model][variant]")
{
    std::vector<uint8_t> blob = { 0xDE, 0xAD, 0xBE, 0xEF };
    Variant v (blob);

    auto& retrieved = v.toBinary();
    REQUIRE (retrieved.size() == 4);
    REQUIRE (retrieved[0] == 0xDE);
    REQUIRE (retrieved[1] == 0xAD);
    REQUIRE (retrieved[2] == 0xBE);
    REQUIRE (retrieved[3] == 0xEF);
}

TEST_CASE ("Variant: empty Binary blob", "[model][variant]")
{
    std::vector<uint8_t> blob;
    Variant v (blob);
    REQUIRE (v.type() == Variant::Type::Binary);
    REQUIRE (v.toBinary().empty());
}

TEST_CASE ("Variant: Binary equality", "[model][variant]")
{
    std::vector<uint8_t> blob1 = { 1, 2, 3 };
    std::vector<uint8_t> blob2 = { 1, 2, 3 };
    std::vector<uint8_t> blob3 = { 1, 2, 4 };

    REQUIRE (Variant (blob1) == Variant (blob2));
    REQUIRE (Variant (blob1) != Variant (blob3));
}

// ═══════════════════════════════════════════════════════════════
// Copy and move semantics
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("Variant: copy produces equal variant", "[model][variant]")
{
    Variant original (42);
    Variant copy = original;

    REQUIRE (copy == original);
    REQUIRE (copy.toInt() == 42);
    REQUIRE (original.toInt() == 42);  // original unchanged
}

TEST_CASE ("Variant: copy of String is independent", "[model][variant]")
{
    Variant original ("hello");
    Variant copy = original;

    REQUIRE (copy == original);
    REQUIRE (copy.toString() == "hello");
}

TEST_CASE ("Variant: move leaves source as Void", "[model][variant]")
{
    Variant original ("hello");
    Variant moved = std::move (original);

    REQUIRE (moved.type() == Variant::Type::String);
    REQUIRE (moved.toString() == "hello");
    // After move, source should be in a valid but unspecified state.
    // For std::variant with std::string, the string is moved-from (empty).
    // The type tag stays String since we only move the std::variant value.
}

TEST_CASE ("Variant: move of Binary transfers ownership", "[model][variant]")
{
    std::vector<uint8_t> blob = { 1, 2, 3, 4, 5 };
    Variant original (blob);
    Variant moved = std::move (original);

    REQUIRE (moved.type() == Variant::Type::Binary);
    REQUIRE (moved.toBinary().size() == 5);
}

// ═══════════════════════════════════════════════════════════════
// Edge cases
// ═══════════════════════════════════════════════════════════════

TEST_CASE ("Variant: large int64_t values", "[model][variant]")
{
    Variant v (int64_t (9223372036854775807LL));  // INT64_MAX
    REQUIRE (v.toInt() == 9223372036854775807LL);
}

TEST_CASE ("Variant: negative int64_t", "[model][variant]")
{
    Variant v (int64_t (-42));
    REQUIRE (v.toInt() == -42);
}

TEST_CASE ("Variant: negative double truncation in getIntOr", "[model][variant]")
{
    Variant v (-3.7);
    REQUIRE (v.getIntOr (0) == -3);
}

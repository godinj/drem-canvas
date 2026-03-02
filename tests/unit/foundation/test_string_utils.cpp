#include <catch2/catch_test_macros.hpp>
#include <dc/foundation/string_utils.h>

TEST_CASE("trim on empty string returns empty", "[foundation][string_utils]")
{
    REQUIRE(dc::trim("") == "");
}

TEST_CASE("trim removes leading and trailing whitespace", "[foundation][string_utils]")
{
    REQUIRE(dc::trim("  hello  ") == "hello");
    REQUIRE(dc::trim("\thello\n") == "hello");
    REQUIRE(dc::trim("  \t hello world \r\n ") == "hello world");
}

TEST_CASE("trim on all-whitespace returns empty", "[foundation][string_utils]")
{
    REQUIRE(dc::trim("   \t\n\r  ") == "");
}

TEST_CASE("trim preserves string with no whitespace", "[foundation][string_utils]")
{
    REQUIRE(dc::trim("hello") == "hello");
}

TEST_CASE("replace replaces all occurrences", "[foundation][string_utils]")
{
    REQUIRE(dc::replace("aaba", "a", "x") == "xxbx");
}

TEST_CASE("replace with empty from is a no-op", "[foundation][string_utils]")
{
    REQUIRE(dc::replace("hello", "", "x") == "hello");
}

TEST_CASE("replace greedy left-to-right for overlapping patterns", "[foundation][string_utils]")
{
    REQUIRE(dc::replace("aaa", "aa", "b") == "ba");
}

TEST_CASE("replace with no matches returns original", "[foundation][string_utils]")
{
    REQUIRE(dc::replace("hello", "xyz", "abc") == "hello");
}

TEST_CASE("replace with empty to removes occurrences", "[foundation][string_utils]")
{
    REQUIRE(dc::replace("hello world", " ", "") == "helloworld");
}

TEST_CASE("replace with longer replacement", "[foundation][string_utils]")
{
    REQUIRE(dc::replace("ab", "a", "xyz") == "xyzb");
}

TEST_CASE("contains finds present substring", "[foundation][string_utils]")
{
    REQUIRE(dc::contains("hello world", "world") == true);
    REQUIRE(dc::contains("hello world", "hello") == true);
    REQUIRE(dc::contains("hello world", "lo wo") == true);
}

TEST_CASE("contains returns false for absent substring", "[foundation][string_utils]")
{
    REQUIRE(dc::contains("hello world", "xyz") == false);
}

TEST_CASE("contains with empty needle returns true", "[foundation][string_utils]")
{
    REQUIRE(dc::contains("hello", "") == true);
}

TEST_CASE("contains with empty haystack returns false for non-empty needle", "[foundation][string_utils]")
{
    REQUIRE(dc::contains("", "x") == false);
}

TEST_CASE("startsWith matches prefix", "[foundation][string_utils]")
{
    REQUIRE(dc::startsWith("hello world", "hello") == true);
}

TEST_CASE("startsWith returns false for mismatch", "[foundation][string_utils]")
{
    REQUIRE(dc::startsWith("hello world", "world") == false);
}

TEST_CASE("startsWith with empty prefix returns true", "[foundation][string_utils]")
{
    REQUIRE(dc::startsWith("hello", "") == true);
}

TEST_CASE("startsWith with string equal to prefix returns true", "[foundation][string_utils]")
{
    REQUIRE(dc::startsWith("hello", "hello") == true);
}

TEST_CASE("startsWith with prefix longer than string returns false", "[foundation][string_utils]")
{
    REQUIRE(dc::startsWith("hi", "hello") == false);
}

TEST_CASE("afterFirst returns substring after delimiter", "[foundation][string_utils]")
{
    REQUIRE(dc::afterFirst("key=value", "=") == "value");
    REQUIRE(dc::afterFirst("a/b/c", "/") == "b/c");
}

TEST_CASE("afterFirst with delimiter at end returns empty", "[foundation][string_utils]")
{
    REQUIRE(dc::afterFirst("hello=", "=") == "");
}

TEST_CASE("afterFirst with absent delimiter returns empty", "[foundation][string_utils]")
{
    REQUIRE(dc::afterFirst("hello", "=") == "");
}

TEST_CASE("afterFirst with multi-char delimiter", "[foundation][string_utils]")
{
    REQUIRE(dc::afterFirst("hello::world::end", "::") == "world::end");
}

TEST_CASE("shellQuote wraps in single quotes", "[foundation][string_utils]")
{
    REQUIRE(dc::shellQuote("hello world") == "'hello world'");
}

TEST_CASE("shellQuote escapes embedded single quotes", "[foundation][string_utils]")
{
    REQUIRE(dc::shellQuote("it's") == "'it'\\''s'");
}

TEST_CASE("shellQuote on empty string returns pair of quotes", "[foundation][string_utils]")
{
    REQUIRE(dc::shellQuote("") == "''");
}

TEST_CASE("shellQuote preserves control characters inside quotes", "[foundation][string_utils]")
{
    auto result = dc::shellQuote("hello\tworld\n");
    REQUIRE(result == "'hello\tworld\n'");
}

TEST_CASE("shellQuote with no special characters", "[foundation][string_utils]")
{
    REQUIRE(dc::shellQuote("simple") == "'simple'");
}

TEST_CASE("format with valid format string", "[foundation][string_utils]")
{
    REQUIRE(dc::format("x=%d", 42) == "x=42");
}

TEST_CASE("format with multiple arguments", "[foundation][string_utils]")
{
    REQUIRE(dc::format("%s=%d", "answer", 42) == "answer=42");
}

TEST_CASE("format with floating point", "[foundation][string_utils]")
{
    REQUIRE(dc::format("%.2f", 3.14) == "3.14");
}

TEST_CASE("format with no args returns format string as-is", "[foundation][string_utils]")
{
    REQUIRE(dc::format("hello") == "hello");
}

TEST_CASE("format with empty format string", "[foundation][string_utils]")
{
    REQUIRE(dc::format("") == "");
}

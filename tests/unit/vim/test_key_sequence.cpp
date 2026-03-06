#include <catch2/catch_test_macros.hpp>
#include "vim/KeySequence.h"

TEST_CASE("Parse single character key", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("j");
    REQUIRE(seq.steps.size() == 1);
    REQUIRE(seq.steps[0].character == 'j');
    REQUIRE(seq.steps[0].specialKey.empty());
    REQUIRE(seq.steps[0].shift == false);
    REQUIRE(seq.steps[0].control == false);
    REQUIRE(seq.steps[0].alt == false);
    REQUIRE(seq.steps[0].command == false);
    REQUIRE(seq.isSingleKey());
}

TEST_CASE("Parse uppercase character key", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("M");
    REQUIRE(seq.steps.size() == 1);
    REQUIRE(seq.steps[0].character == 'M');
}

TEST_CASE("Parse modifier key Ctrl+R", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("Ctrl+R");
    REQUIRE(seq.steps.size() == 1);
    REQUIRE(seq.steps[0].character == 'R');
    REQUIRE(seq.steps[0].control == true);
    REQUIRE(seq.steps[0].shift == false);
    REQUIRE(seq.steps[0].alt == false);
    REQUIRE(seq.isSingleKey());
}

TEST_CASE("Parse multi-modifier Ctrl+Shift+j", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("Ctrl+Shift+j");
    REQUIRE(seq.steps.size() == 1);
    REQUIRE(seq.steps[0].character == 'j');
    REQUIRE(seq.steps[0].shift == true);
    REQUIRE(seq.steps[0].control == true);
    REQUIRE(seq.steps[0].alt == false);
}

TEST_CASE("Parse special key Space", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("Space");
    REQUIRE(seq.steps.size() == 1);
    REQUIRE(seq.steps[0].specialKey == "Space");
    REQUIRE(seq.steps[0].character == 0);
    REQUIRE(seq.isSingleKey());
}

TEST_CASE("Parse special key Return", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("Return");
    REQUIRE(seq.steps.size() == 1);
    REQUIRE(seq.steps[0].specialKey == "Return");
}

TEST_CASE("Parse special key Escape", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("Escape");
    REQUIRE(seq.steps.size() == 1);
    REQUIRE(seq.steps[0].specialKey == "Escape");
}

TEST_CASE("Parse special key Tab", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("Tab");
    REQUIRE(seq.steps.size() == 1);
    REQUIRE(seq.steps[0].specialKey == "Tab");
}

TEST_CASE("Parse multi-step gg", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("gg");
    REQUIRE(seq.steps.size() == 2);
    REQUIRE(seq.steps[0].character == 'g');
    REQUIRE(seq.steps[1].character == 'g');
    REQUIRE_FALSE(seq.isSingleKey());
}

TEST_CASE("Parse multi-step zi", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("zi");
    REQUIRE(seq.steps.size() == 2);
    REQUIRE(seq.steps[0].character == 'z');
    REQUIRE(seq.steps[1].character == 'i');
}

TEST_CASE("Parse multi-step gp", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("gp");
    REQUIRE(seq.steps.size() == 2);
    REQUIRE(seq.steps[0].character == 'g');
    REQUIRE(seq.steps[1].character == 'p');
}

TEST_CASE("Parse modifier with special key Ctrl+Space", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("Ctrl+Space");
    REQUIRE(seq.steps.size() == 1);
    REQUIRE(seq.steps[0].specialKey == "Space");
    REQUIRE(seq.steps[0].control == true);
}

TEST_CASE("Round-trip toString single char", "[vim][key_sequence]")
{
    REQUIRE(dc::KeySequence::parse("j").toString() == "j");
    REQUIRE(dc::KeySequence::parse("M").toString() == "M");
    REQUIRE(dc::KeySequence::parse("$").toString() == "$");
}

TEST_CASE("Round-trip toString modifier", "[vim][key_sequence]")
{
    REQUIRE(dc::KeySequence::parse("Ctrl+R").toString() == "Ctrl+R");
    REQUIRE(dc::KeySequence::parse("Ctrl+z").toString() == "Ctrl+z");
}

TEST_CASE("Round-trip toString special key", "[vim][key_sequence]")
{
    REQUIRE(dc::KeySequence::parse("Space").toString() == "Space");
    REQUIRE(dc::KeySequence::parse("Return").toString() == "Return");
    REQUIRE(dc::KeySequence::parse("Tab").toString() == "Tab");
    REQUIRE(dc::KeySequence::parse("Escape").toString() == "Escape");
}

TEST_CASE("Round-trip toString multi-step", "[vim][key_sequence]")
{
    REQUIRE(dc::KeySequence::parse("gg").toString() == "gg");
    REQUIRE(dc::KeySequence::parse("zi").toString() == "zi");
    REQUIRE(dc::KeySequence::parse("gp").toString() == "gp");
}

TEST_CASE("Step matches character exactly", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("j");
    REQUIRE(seq.steps[0].matches('j', false, false, false, false));
    REQUIRE_FALSE(seq.steps[0].matches('k', false, false, false, false));
}

TEST_CASE("Step matches modifier exactly", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("Ctrl+R");
    REQUIRE(seq.steps[0].matches('R', false, true, false, false));
    REQUIRE_FALSE(seq.steps[0].matches('R', false, false, false, false));
    REQUIRE_FALSE(seq.steps[0].matches('R', true, true, false, false));
}

TEST_CASE("Step matches special key Space", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("Space");
    REQUIRE(seq.steps[0].matches(0x20, false, false, false, false));
    REQUIRE_FALSE(seq.steps[0].matches('s', false, false, false, false));
}

TEST_CASE("Step matches special key Return", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("Return");
    REQUIRE(seq.steps[0].matches(0x0D, false, false, false, false));
}

TEST_CASE("Equality operator for KeySequence", "[vim][key_sequence]")
{
    REQUIRE(dc::KeySequence::parse("gg") == dc::KeySequence::parse("gg"));
    REQUIRE(dc::KeySequence::parse("Ctrl+R") == dc::KeySequence::parse("Ctrl+R"));
    REQUIRE_FALSE(dc::KeySequence::parse("gg") == dc::KeySequence::parse("gp"));
    REQUIRE_FALSE(dc::KeySequence::parse("j") == dc::KeySequence::parse("Ctrl+j"));
}

TEST_CASE("Less-than operator for KeySequence", "[vim][key_sequence]")
{
    // Just verify it provides a total ordering
    auto a = dc::KeySequence::parse("a");
    auto b = dc::KeySequence::parse("b");
    auto gg = dc::KeySequence::parse("gg");
    auto g = dc::KeySequence::parse("g");

    REQUIRE(a < b);
    REQUIRE_FALSE(b < a);
    REQUIRE(g < gg); // shorter sequence is less
}

TEST_CASE("Parse empty string returns empty sequence", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("");
    REQUIRE(seq.steps.empty());
}

TEST_CASE("Parse single special character +", "[vim][key_sequence]")
{
    auto seq = dc::KeySequence::parse("+");
    REQUIRE(seq.steps.size() == 1);
    REQUIRE(seq.steps[0].character == '+');
}

TEST_CASE("Parse Ctrl+Shift+j round-trip", "[vim][key_sequence]")
{
    REQUIRE(dc::KeySequence::parse("Ctrl+Shift+j").toString() == "Ctrl+Shift+j");
}

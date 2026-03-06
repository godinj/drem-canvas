#include <catch2/catch_test_macros.hpp>
#include "vim/VimGrammar.h"

using dc::VimGrammar;

// Helper: feed a single unmodified key
static VimGrammar::ParseResult feedKey (VimGrammar& g, char32_t c)
{
    return g.feed (c, false, false, false, false);
}

// ─── Simple motions ─────────────────────────────────────────────────────────

TEST_CASE ("VimGrammar simple motion j", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r = feedKey (g, 'j');
    CHECK (r.type == VimGrammar::ParseResult::Motion);
    CHECK (r.motionKey == 'j');
    CHECK (r.count == 1);
}

TEST_CASE ("VimGrammar simple motion k", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r = feedKey (g, 'k');
    CHECK (r.type == VimGrammar::ParseResult::Motion);
    CHECK (r.motionKey == 'k');
    CHECK (r.count == 1);
}

TEST_CASE ("VimGrammar simple motion h", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r = feedKey (g, 'h');
    CHECK (r.type == VimGrammar::ParseResult::Motion);
    CHECK (r.motionKey == 'h');
    CHECK (r.count == 1);
}

TEST_CASE ("VimGrammar simple motion l", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r = feedKey (g, 'l');
    CHECK (r.type == VimGrammar::ParseResult::Motion);
    CHECK (r.motionKey == 'l');
    CHECK (r.count == 1);
}

TEST_CASE ("VimGrammar motion $", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r = feedKey (g, '$');
    CHECK (r.type == VimGrammar::ParseResult::Motion);
    CHECK (r.motionKey == '$');
}

TEST_CASE ("VimGrammar motion G", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r = feedKey (g, 'G');
    CHECK (r.type == VimGrammar::ParseResult::Motion);
    CHECK (r.motionKey == 'G');
}

TEST_CASE ("VimGrammar motion w", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r = feedKey (g, 'w');
    CHECK (r.type == VimGrammar::ParseResult::Motion);
    CHECK (r.motionKey == 'w');
}

// ─── Counted motions ────────────────────────────────────────────────────────

TEST_CASE ("VimGrammar counted motion 3j", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r1 = feedKey (g, '3');
    CHECK (r1.type == VimGrammar::ParseResult::Incomplete);

    auto r2 = feedKey (g, 'j');
    CHECK (r2.type == VimGrammar::ParseResult::Motion);
    CHECK (r2.motionKey == 'j');
    CHECK (r2.count == 3);
}

TEST_CASE ("VimGrammar multi-digit count 12j", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '1');
    feedKey (g, '2');
    auto r = feedKey (g, 'j');
    CHECK (r.type == VimGrammar::ParseResult::Motion);
    CHECK (r.motionKey == 'j');
    CHECK (r.count == 12);
}

TEST_CASE ("VimGrammar count with zero 10j", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '1');
    feedKey (g, '0');
    auto r = feedKey (g, 'j');
    CHECK (r.type == VimGrammar::ParseResult::Motion);
    CHECK (r.motionKey == 'j');
    CHECK (r.count == 10);
}

TEST_CASE ("VimGrammar 0 without count is motion", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r = feedKey (g, '0');
    CHECK (r.type == VimGrammar::ParseResult::Motion);
    CHECK (r.motionKey == '0');
}

// ─── Operator + motion ──────────────────────────────────────────────────────

TEST_CASE ("VimGrammar operator + motion dj", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r1 = feedKey (g, 'd');
    CHECK (r1.type == VimGrammar::ParseResult::Incomplete);

    auto r2 = feedKey (g, 'j');
    CHECK (r2.type == VimGrammar::ParseResult::OperatorMotion);
    CHECK (r2.op == VimGrammar::OpDelete);
    CHECK (r2.motionKey == 'j');
    CHECK (r2.count == 1);
}

TEST_CASE ("VimGrammar operator + motion yk", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, 'y');
    auto r = feedKey (g, 'k');
    CHECK (r.type == VimGrammar::ParseResult::OperatorMotion);
    CHECK (r.op == VimGrammar::OpYank);
    CHECK (r.motionKey == 'k');
}

TEST_CASE ("VimGrammar operator + motion c$", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, 'c');
    auto r = feedKey (g, '$');
    CHECK (r.type == VimGrammar::ParseResult::OperatorMotion);
    CHECK (r.op == VimGrammar::OpChange);
    CHECK (r.motionKey == '$');
}

// ─── Counted operator + motion ──────────────────────────────────────────────

TEST_CASE ("VimGrammar counted operator 3d2j", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '3');
    feedKey (g, 'd');
    feedKey (g, '2');
    auto r = feedKey (g, 'j');
    CHECK (r.type == VimGrammar::ParseResult::OperatorMotion);
    CHECK (r.op == VimGrammar::OpDelete);
    CHECK (r.motionKey == 'j');
    CHECK (r.count == 6);  // 3 * 2
}

TEST_CASE ("VimGrammar precount only d3j", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, 'd');
    feedKey (g, '3');
    auto r = feedKey (g, 'j');
    CHECK (r.type == VimGrammar::ParseResult::OperatorMotion);
    CHECK (r.count == 3);
}

TEST_CASE ("VimGrammar postcount only 2dj", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '2');
    feedKey (g, 'd');
    auto r = feedKey (g, 'j');
    CHECK (r.type == VimGrammar::ParseResult::OperatorMotion);
    CHECK (r.count == 2);
}

// ─── Doubled operator (linewise) ────────────────────────────────────────────

TEST_CASE ("VimGrammar doubled operator dd", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r1 = feedKey (g, 'd');
    CHECK (r1.type == VimGrammar::ParseResult::Incomplete);

    auto r2 = feedKey (g, 'd');
    CHECK (r2.type == VimGrammar::ParseResult::LinewiseOperator);
    CHECK (r2.op == VimGrammar::OpDelete);
    CHECK (r2.count == 1);
}

TEST_CASE ("VimGrammar doubled operator yy", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, 'y');
    auto r = feedKey (g, 'y');
    CHECK (r.type == VimGrammar::ParseResult::LinewiseOperator);
    CHECK (r.op == VimGrammar::OpYank);
    CHECK (r.count == 1);
}

TEST_CASE ("VimGrammar doubled operator cc", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, 'c');
    auto r = feedKey (g, 'c');
    CHECK (r.type == VimGrammar::ParseResult::LinewiseOperator);
    CHECK (r.op == VimGrammar::OpChange);
}

TEST_CASE ("VimGrammar counted doubled 3dd", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '3');
    feedKey (g, 'd');
    auto r = feedKey (g, 'd');
    CHECK (r.type == VimGrammar::ParseResult::LinewiseOperator);
    CHECK (r.op == VimGrammar::OpDelete);
    CHECK (r.count == 3);
}

// ─── Register prefix ────────────────────────────────────────────────────────

TEST_CASE ("VimGrammar register prefix \"adj", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r1 = feedKey (g, '"');
    CHECK (r1.type == VimGrammar::ParseResult::Incomplete);

    auto r2 = feedKey (g, 'a');
    CHECK (r2.type == VimGrammar::ParseResult::Incomplete);

    auto r3 = feedKey (g, 'd');
    CHECK (r3.type == VimGrammar::ParseResult::Incomplete);

    auto r4 = feedKey (g, 'j');
    CHECK (r4.type == VimGrammar::ParseResult::OperatorMotion);
    CHECK (r4.op == VimGrammar::OpDelete);
    CHECK (r4.reg == 'a');
    CHECK (r4.motionKey == 'j');
}

TEST_CASE ("VimGrammar register prefix \"ayy", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '"');
    feedKey (g, 'a');
    feedKey (g, 'y');
    auto r = feedKey (g, 'y');
    CHECK (r.type == VimGrammar::ParseResult::LinewiseOperator);
    CHECK (r.op == VimGrammar::OpYank);
    CHECK (r.reg == 'a');
}

TEST_CASE ("VimGrammar register with count \"a3dj", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '"');
    feedKey (g, 'a');
    feedKey (g, '3');
    feedKey (g, 'd');
    auto r = feedKey (g, 'j');
    CHECK (r.type == VimGrammar::ParseResult::OperatorMotion);
    CHECK (r.reg == 'a');
    CHECK (r.count == 3);
}

// ─── Pending gg ─────────────────────────────────────────────────────────────

TEST_CASE ("VimGrammar pending gg", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r1 = feedKey (g, 'g');
    CHECK (r1.type == VimGrammar::ParseResult::Incomplete);

    auto r2 = feedKey (g, 'g');
    CHECK (r2.type == VimGrammar::ParseResult::Motion);
    CHECK (r2.motionKey == 'g');
    CHECK (r2.count == 1);
}

TEST_CASE ("VimGrammar counted gg (3gg)", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '3');
    feedKey (g, 'g');
    auto r = feedKey (g, 'g');
    CHECK (r.type == VimGrammar::ParseResult::Motion);
    CHECK (r.motionKey == 'g');
    CHECK (r.count == 3);
}

TEST_CASE ("VimGrammar dgg", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, 'd');
    feedKey (g, 'g');
    auto r = feedKey (g, 'g');
    CHECK (r.type == VimGrammar::ParseResult::OperatorMotion);
    CHECK (r.op == VimGrammar::OpDelete);
    CHECK (r.motionKey == 'g');
}

// ─── Reset cancels ──────────────────────────────────────────────────────────

TEST_CASE ("VimGrammar reset cancels pending operator", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, 'd');
    CHECK (g.isOperatorPending());
    CHECK (g.hasPendingState());

    g.reset();
    CHECK_FALSE (g.isOperatorPending());
    CHECK_FALSE (g.hasPendingState());
}

TEST_CASE ("VimGrammar reset cancels count", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '3');
    CHECK (g.hasPendingState());

    g.reset();
    CHECK_FALSE (g.hasPendingState());

    // After reset, next motion should have count=1
    auto r = feedKey (g, 'j');
    CHECK (r.count == 1);
}

TEST_CASE ("VimGrammar reset cancels register", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '"');
    feedKey (g, 'a');
    CHECK (g.hasPendingState());

    g.reset();
    CHECK_FALSE (g.hasPendingState());

    // After reset, register should be null
    auto r = feedKey (g, 'j');
    CHECK (r.reg == '\0');
}

// ─── Non-grammar key ────────────────────────────────────────────────────────

TEST_CASE ("VimGrammar non-grammar key returns NoMatch", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r = feedKey (g, 'M');
    CHECK (r.type == VimGrammar::ParseResult::NoMatch);
}

TEST_CASE ("VimGrammar non-grammar key i returns NoMatch", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r = feedKey (g, 'i');
    CHECK (r.type == VimGrammar::ParseResult::NoMatch);
}

TEST_CASE ("VimGrammar non-grammar key : returns NoMatch", "[unit][vim][grammar]")
{
    VimGrammar g;
    auto r = feedKey (g, ':');
    CHECK (r.type == VimGrammar::ParseResult::NoMatch);
}

// ─── Configurable operators ─────────────────────────────────────────────────

TEST_CASE ("VimGrammar configurable operators", "[unit][vim][grammar]")
{
    VimGrammar g;
    g.setOperatorChars ("d");

    // 'y' should no longer be an operator
    auto r = feedKey (g, 'y');
    CHECK (r.type == VimGrammar::ParseResult::NoMatch);

    // 'd' should still work
    auto r2 = feedKey (g, 'd');
    CHECK (r2.type == VimGrammar::ParseResult::Incomplete);
}

// ─── Configurable motions ───────────────────────────────────────────────────

TEST_CASE ("VimGrammar configurable motions", "[unit][vim][grammar]")
{
    VimGrammar g;
    g.setMotionChars ("jk");

    // 'l' should no longer be a motion
    auto r = feedKey (g, 'l');
    CHECK (r.type == VimGrammar::ParseResult::NoMatch);

    // 'j' should still work
    auto r2 = feedKey (g, 'j');
    CHECK (r2.type == VimGrammar::ParseResult::Motion);
}

// ─── getPendingDisplay ──────────────────────────────────────────────────────

TEST_CASE ("VimGrammar getPendingDisplay empty", "[unit][vim][grammar]")
{
    VimGrammar g;
    CHECK (g.getPendingDisplay() == "");
}

TEST_CASE ("VimGrammar getPendingDisplay with count", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '3');
    CHECK (g.getPendingDisplay() == "3");
}

TEST_CASE ("VimGrammar getPendingDisplay with operator", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, 'd');
    CHECK (g.getPendingDisplay() == "d");
}

TEST_CASE ("VimGrammar getPendingDisplay 3d", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '3');
    feedKey (g, 'd');
    CHECK (g.getPendingDisplay() == "3d");
}

TEST_CASE ("VimGrammar getPendingDisplay d2", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, 'd');
    feedKey (g, '2');
    CHECK (g.getPendingDisplay() == "d2");
}

TEST_CASE ("VimGrammar getPendingDisplay register \"a3d", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '"');
    feedKey (g, 'a');
    feedKey (g, '3');
    feedKey (g, 'd');
    CHECK (g.getPendingDisplay() == "\"a3d");
}

TEST_CASE ("VimGrammar getPendingDisplay awaiting register", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '"');
    CHECK (g.getPendingDisplay() == "\"");
}

// ─── consumeRegister ────────────────────────────────────────────────────────

TEST_CASE ("VimGrammar consumeRegister returns and clears", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '"');
    feedKey (g, 'b');

    char reg = g.consumeRegister();
    CHECK (reg == 'b');

    // After consume, register should be cleared
    char reg2 = g.consumeRegister();
    CHECK (reg2 == '\0');
}

// ─── Sequential operations ──────────────────────────────────────────────────

TEST_CASE ("VimGrammar state resets after completed motion", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, '3');
    auto r1 = feedKey (g, 'j');
    CHECK (r1.type == VimGrammar::ParseResult::Motion);
    CHECK (r1.count == 3);

    // Second motion should have count=1
    auto r2 = feedKey (g, 'k');
    CHECK (r2.type == VimGrammar::ParseResult::Motion);
    CHECK (r2.count == 1);
}

TEST_CASE ("VimGrammar state resets after operator+motion", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, 'd');
    feedKey (g, 'j');

    // Next key should have clean state
    CHECK_FALSE (g.isOperatorPending());
    auto r = feedKey (g, 'k');
    CHECK (r.type == VimGrammar::ParseResult::Motion);
    CHECK (r.count == 1);
}

TEST_CASE ("VimGrammar state resets after doubled operator", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, 'd');
    feedKey (g, 'd');

    // State should be clean
    CHECK_FALSE (g.isOperatorPending());
    auto r = feedKey (g, 'j');
    CHECK (r.type == VimGrammar::ParseResult::Motion);
    CHECK (r.count == 1);
}

// ─── Different operator after first ─────────────────────────────────────────

TEST_CASE ("VimGrammar different operator after pending (dy)", "[unit][vim][grammar]")
{
    VimGrammar g;
    feedKey (g, 'd');

    // 'y' is an operator char but different from pending 'd'
    // In vim, dy is not valid — but our grammar doesn't catch this;
    // the existing VimEngine just starts a new operator.
    // Since dy changes to yank pending, it should return Incomplete
    auto r = feedKey (g, 'y');
    CHECK (r.type == VimGrammar::ParseResult::Incomplete);
    CHECK (g.isOperatorPending());
}

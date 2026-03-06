#include "VimGrammar.h"
#include "dc/foundation/time.h"
#include <algorithm>

namespace dc
{

VimGrammar::ParseResult VimGrammar::feed (char32_t keyChar, bool /*shift*/,
                                          bool /*control*/, bool /*alt*/, bool /*command*/)
{
    ParseResult result;

    // Phase 1: Pending key resolution (e.g. gg)
    if (pendingKey != 0)
    {
        if (keyChar == pendingKey
            && (dc::currentTimeMillis() - pendingTimestamp) < pendingTimeoutMs)
        {
            // Matched pending key sequence (e.g. g + g = gg)
            char32_t resolvedKey = pendingKey;
            pendingKey = 0;
            pendingTimestamp = 0;

            result.type = isOperatorPending() ? ParseResult::OperatorMotion : ParseResult::Motion;
            result.motionKey = resolvedKey;
            result.op = pendingOperator;
            result.count = getEffectiveCount();
            result.reg = pendingRegister;

            // Reset state after completing the parse
            if (isOperatorPending())
                pendingOperator = OpNone;
            preCount = 0;
            postCount = 0;

            return result;
        }

        // Timeout or different key — clear pending key and fall through
        pendingKey = 0;
        pendingTimestamp = 0;
    }

    // Phase 2: Register prefix ("x)
    if (awaitingRegisterChar)
    {
        char c = static_cast<char> (keyChar);
        // Accept a-z, A-Z, 0-9 as valid register characters
        if (c != '\0' && ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9')))
        {
            pendingRegister = c;
            awaitingRegisterChar = false;
            result.type = ParseResult::Incomplete;
            return result;
        }
        // Invalid register char — cancel
        awaitingRegisterChar = false;
        result.type = ParseResult::Incomplete;
        return result;
    }

    if (keyChar == '"')
    {
        awaitingRegisterChar = true;
        result.type = ParseResult::Incomplete;
        return result;
    }

    // Phase 3: Digit accumulation
    if (isDigitForCount (keyChar))
    {
        int digit = keyChar - '0';
        if (isOperatorPending())
            postCount = postCount * 10 + digit;
        else
            preCount = preCount * 10 + digit;

        result.type = ParseResult::Incomplete;
        return result;
    }

    // Phase 4: Operator keys
    auto op = charToOperator (keyChar);
    if (op != OpNone)
    {
        if (isOperatorPending() && pendingOperator == op)
        {
            // Doubled operator (dd / yy / cc) — linewise on current track(s)
            result.type = ParseResult::LinewiseOperator;
            result.op = op;
            result.count = getEffectiveCount();
            result.reg = pendingRegister;

            pendingOperator = OpNone;
            preCount = 0;
            postCount = 0;

            return result;
        }

        // Start a new operator-pending state
        pendingOperator = op;
        postCount = 0;
        result.type = ParseResult::Incomplete;
        return result;
    }

    // Phase 5: Motion keys
    if (isMotionChar (keyChar))
    {
        // Check if this is a pending key char that needs a second key (e.g. 'g' for gg)
        if (isPendingKeyChar (keyChar) && ! isOperatorPending())
        {
            // In standalone motion context, 'g' starts gg pending sequence
            pendingKey = keyChar;
            pendingTimestamp = dc::currentTimeMillis();
            result.type = ParseResult::Incomplete;
            return result;
        }

        if (isPendingKeyChar (keyChar) && isOperatorPending())
        {
            // Operator pending + pending key char (e.g. d + g for dgg)
            pendingKey = keyChar;
            pendingTimestamp = dc::currentTimeMillis();
            result.type = ParseResult::Incomplete;
            return result;
        }

        int count = getEffectiveCount();

        if (isOperatorPending())
        {
            result.type = ParseResult::OperatorMotion;
            result.motionKey = keyChar;
            result.op = pendingOperator;
            result.count = count;
            result.reg = pendingRegister;

            pendingOperator = OpNone;
            preCount = 0;
            postCount = 0;

            return result;
        }

        result.type = ParseResult::Motion;
        result.motionKey = keyChar;
        result.count = count;
        result.reg = pendingRegister;

        preCount = 0;
        postCount = 0;

        return result;
    }

    // Phase 6: No match — fall through to caller's keymap
    result.type = ParseResult::NoMatch;
    return result;
}

void VimGrammar::reset()
{
    pendingOperator = OpNone;
    preCount = 0;
    postCount = 0;
    pendingRegister = '\0';
    awaitingRegisterChar = false;
    pendingKey = 0;
    pendingTimestamp = 0;
}

bool VimGrammar::isOperatorPending() const
{
    return pendingOperator != OpNone;
}

bool VimGrammar::hasPendingState() const
{
    return pendingOperator != OpNone || preCount > 0
        || postCount > 0 || pendingKey != 0
        || pendingRegister != '\0' || awaitingRegisterChar;
}

std::string VimGrammar::getPendingDisplay() const
{
    std::string display;

    if (pendingRegister != '\0')
    {
        display += "\"";
        display += std::string (1, pendingRegister);
    }
    else if (awaitingRegisterChar)
    {
        display += "\"";
    }

    if (preCount > 0)
        display += std::to_string (preCount);

    switch (pendingOperator)
    {
        case OpDelete: display += "d"; break;
        case OpYank:   display += "y"; break;
        case OpChange: display += "c"; break;
        case OpNone:   break;
    }

    if (postCount > 0)
        display += std::to_string (postCount);

    if (pendingKey != 0)
        display += std::string (1, char (pendingKey));

    return display;
}

void VimGrammar::setOperatorChars (const std::string& chars)
{
    operatorChars = chars;
}

void VimGrammar::setMotionChars (const std::string& chars)
{
    motionChars = chars;
}

void VimGrammar::setPendingKeys (const std::string& chars)
{
    pendingKeyChars = chars;
}

bool VimGrammar::hasPendingKey() const
{
    return pendingKey != 0;
}

char32_t VimGrammar::getPendingKey() const
{
    return pendingKey;
}

void VimGrammar::clearPendingKey()
{
    pendingKey = 0;
    pendingTimestamp = 0;
}

void VimGrammar::setPendingKey (char32_t c)
{
    pendingKey = c;
    pendingTimestamp = dc::currentTimeMillis();
}

char VimGrammar::consumeRegister()
{
    char reg = pendingRegister;
    pendingRegister = '\0';
    awaitingRegisterChar = false;
    return reg;
}

bool VimGrammar::isDigitForCount (char32_t c) const
{
    if (c >= '1' && c <= '9')
        return true;

    // '0' is a count digit only when we're already accumulating a count
    if (c == '0' && (preCount > 0 || postCount > 0))
        return true;

    return false;
}

bool VimGrammar::isOperatorChar (char32_t c) const
{
    return operatorChars.find (static_cast<char> (c)) != std::string::npos;
}

bool VimGrammar::isMotionChar (char32_t c) const
{
    return motionChars.find (static_cast<char> (c)) != std::string::npos;
}

bool VimGrammar::isPendingKeyChar (char32_t c) const
{
    return pendingKeyChars.find (static_cast<char> (c)) != std::string::npos;
}

VimGrammar::Operator VimGrammar::charToOperator (char32_t c) const
{
    if (! isOperatorChar (c))
        return OpNone;

    if (c == 'd') return OpDelete;
    if (c == 'y') return OpYank;
    if (c == 'c') return OpChange;
    return OpNone;
}

int VimGrammar::getEffectiveCount() const
{
    return std::max (1, preCount) * std::max (1, postCount);
}

} // namespace dc

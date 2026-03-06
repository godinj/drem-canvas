#pragma once
#include <string>
#include <cstdint>

namespace dc
{

class VimGrammar
{
public:
    enum Operator { OpNone, OpDelete, OpYank, OpChange };

    struct ParseResult
    {
        enum Type
        {
            NoMatch,          // key is not part of grammar (fall through to keymap)
            Motion,           // standalone motion (e.g. 3j)
            OperatorMotion,   // operator + motion (e.g. d3j)
            LinewiseOperator, // doubled operator (e.g. dd, 3yy)
            Incomplete        // waiting for more input (operator pending, count in progress)
        };

        Type type = NoMatch;
        char32_t motionKey = 0;
        Operator op = OpNone;
        int count = 1;        // effective count (preCount * postCount)
        char reg = '\0';      // register from "x prefix
    };

    // Feed one key into the grammar. Returns parse result.
    ParseResult feed (char32_t keyChar, bool shift, bool control, bool alt, bool command);

    // Cancel all pending state (Escape/Ctrl-C)
    void reset();

    // State queries (for status bar display)
    bool isOperatorPending() const;
    bool hasPendingState() const;
    std::string getPendingDisplay() const;

    // Configure which characters are operators/motions (panel-dependent)
    void setOperatorChars (const std::string& chars);
    void setMotionChars (const std::string& chars);

    // Pending multi-key support (for "gg" sequences)
    void setPendingKeys (const std::string& chars);  // e.g. "g" for gg
    bool hasPendingKey() const;
    char32_t getPendingKey() const;
    void clearPendingKey();
    void setPendingKey (char32_t c);  // manual pending key for panel-specific handlers

    // Register state accessors
    char consumeRegister();

private:
    Operator pendingOperator = OpNone;
    int preCount = 0;       // count before operator (the 3 in 3d2j)
    int postCount = 0;      // count after operator (the 2 in 3d2j)
    char pendingRegister = '\0';
    bool awaitingRegisterChar = false;
    char32_t pendingKey = 0;
    int64_t pendingTimestamp = 0;
    static constexpr int64_t pendingTimeoutMs = 1000;

    std::string operatorChars = "dyc";
    std::string motionChars = "hjkl0$GgwbeW";
    std::string pendingKeyChars = "g";

    bool isDigitForCount (char32_t c) const;
    bool isOperatorChar (char32_t c) const;
    bool isMotionChar (char32_t c) const;
    bool isPendingKeyChar (char32_t c) const;
    Operator charToOperator (char32_t c) const;
    int getEffectiveCount() const;
};

} // namespace dc

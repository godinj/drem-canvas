#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace dc {

/// Thrown when a Variant accessor is called with the wrong type
class TypeMismatch : public std::runtime_error
{
public:
    using std::runtime_error::runtime_error;
};

class Variant
{
public:
    enum class Type { Void, Int, Double, Bool, String, Binary };

    Variant();
    Variant(int64_t v);
    Variant(int v);                         // promotes to int64_t
    Variant(double v);
    Variant(bool v);
    Variant(std::string v);
    Variant(std::string_view v);
    Variant(const char* v);
    Variant(std::vector<uint8_t> blob);     // Binary

    Type type() const { return type_; }
    bool isVoid() const { return type_ == Type::Void; }

    // Strict accessors (throw dc::TypeMismatch if wrong type)
    int64_t                      toInt() const;
    double                       toDouble() const;
    bool                         toBool() const;
    const std::string&           toString() const;
    const std::vector<uint8_t>&  toBinary() const;

    // Fallback accessors (return fallback on type mismatch)
    int64_t     getIntOr(int64_t fallback) const;
    double      getDoubleOr(double fallback) const;
    bool        getBoolOr(bool fallback) const;
    std::string getStringOr(std::string_view fallback) const;

    bool operator==(const Variant& other) const;
    bool operator!=(const Variant& other) const { return !(*this == other); }

private:
    Type type_;
    std::variant<std::monostate, int64_t, double, bool,
                 std::string, std::vector<uint8_t>> value_;
};

} // namespace dc

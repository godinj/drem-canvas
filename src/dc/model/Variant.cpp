#include "dc/model/Variant.h"

#include <cmath>

namespace dc {

Variant::Variant()
    : type_(Type::Void), value_(std::monostate{})
{
}

Variant::Variant(int64_t v)
    : type_(Type::Int), value_(v)
{
}

Variant::Variant(int v)
    : type_(Type::Int), value_(static_cast<int64_t>(v))
{
}

Variant::Variant(double v)
    : type_(Type::Double), value_(v)
{
}

Variant::Variant(bool v)
    : type_(Type::Bool), value_(v)
{
}

Variant::Variant(std::string v)
    : type_(Type::String), value_(std::move(v))
{
}

Variant::Variant(std::string_view v)
    : type_(Type::String), value_(std::string(v))
{
}

Variant::Variant(const char* v)
    : type_(Type::String), value_(std::string(v))
{
}

Variant::Variant(std::vector<uint8_t> blob)
    : type_(Type::Binary), value_(std::move(blob))
{
}

//==============================================================================
// Strict accessors
//==============================================================================

int64_t Variant::toInt() const
{
    if (type_ != Type::Int)
        throw TypeMismatch("Variant is not Int");
    return std::get<int64_t>(value_);
}

double Variant::toDouble() const
{
    if (type_ != Type::Double)
        throw TypeMismatch("Variant is not Double");
    return std::get<double>(value_);
}

bool Variant::toBool() const
{
    if (type_ != Type::Bool)
        throw TypeMismatch("Variant is not Bool");
    return std::get<bool>(value_);
}

const std::string& Variant::toString() const
{
    if (type_ != Type::String)
        throw TypeMismatch("Variant is not String");
    return std::get<std::string>(value_);
}

const std::vector<uint8_t>& Variant::toBinary() const
{
    if (type_ != Type::Binary)
        throw TypeMismatch("Variant is not Binary");
    return std::get<std::vector<uint8_t>>(value_);
}

//==============================================================================
// Fallback accessors
//==============================================================================

int64_t Variant::getIntOr(int64_t fallback) const
{
    if (type_ == Type::Int)
        return std::get<int64_t>(value_);
    if (type_ == Type::Double)
        return static_cast<int64_t>(std::get<double>(value_));
    return fallback;
}

double Variant::getDoubleOr(double fallback) const
{
    if (type_ == Type::Double)
        return std::get<double>(value_);
    if (type_ == Type::Int)
        return static_cast<double>(std::get<int64_t>(value_));
    return fallback;
}

bool Variant::getBoolOr(bool fallback) const
{
    if (type_ == Type::Bool)
        return std::get<bool>(value_);
    if (type_ == Type::Int)
        return std::get<int64_t>(value_) != 0;
    return fallback;
}

std::string Variant::getStringOr(std::string_view fallback) const
{
    if (type_ == Type::String)
        return std::get<std::string>(value_);
    return std::string(fallback);
}

//==============================================================================
// Comparison
//==============================================================================

bool Variant::operator==(const Variant& other) const
{
    if (type_ != other.type_)
        return false;
    return value_ == other.value_;
}

} // namespace dc

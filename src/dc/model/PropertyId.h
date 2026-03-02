#pragma once

#include <functional>
#include <string>
#include <string_view>

namespace dc {

class PropertyId
{
public:
    /// Construct from string literal (interned on first use)
    explicit PropertyId(const char* name);
    explicit PropertyId(std::string_view name);

    /// O(1) pointer comparison
    bool operator==(const PropertyId& other) const { return ptr_ == other.ptr_; }
    bool operator!=(const PropertyId& other) const { return ptr_ != other.ptr_; }
    bool operator<(const PropertyId& other) const  { return ptr_ < other.ptr_; }

    /// Access the interned string
    const std::string& toString() const { return *ptr_; }

    /// Hash support (for unordered containers)
    struct Hash
    {
        size_t operator()(const PropertyId& id) const
        {
            return std::hash<const std::string*>{}(id.ptr_);
        }
    };

private:
    const std::string* ptr_;  // points into global intern table

    static const std::string* intern(std::string_view name);
};

} // namespace dc

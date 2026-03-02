#include "dc/model/PropertyId.h"

#include <mutex>
#include <shared_mutex>
#include <unordered_set>

namespace dc {

static std::unordered_set<std::string>& getInternTable()
{
    static std::unordered_set<std::string> table;
    return table;
}

static std::shared_mutex& getInternMutex()
{
    static std::shared_mutex mutex;
    return mutex;
}

const std::string* PropertyId::intern(std::string_view name)
{
    auto& table = getInternTable();
    auto& mutex = getInternMutex();

    // Fast path: read lock
    {
        std::shared_lock lock(mutex);
        auto it = table.find(std::string(name));
        if (it != table.end())
            return &(*it);
    }

    // Slow path: write lock
    std::unique_lock lock(mutex);
    auto [it, inserted] = table.emplace(name);
    return &(*it);
}

PropertyId::PropertyId(const char* name)
    : ptr_(intern(std::string_view(name)))
{
}

PropertyId::PropertyId(std::string_view name)
    : ptr_(intern(name))
{
}

} // namespace dc

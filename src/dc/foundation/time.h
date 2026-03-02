#pragma once

#include <chrono>
#include <cstdint>

namespace dc {

inline int64_t currentTimeMillis()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(
        system_clock::now().time_since_epoch()).count();
}

inline double hiResTimeMs()
{
    using namespace std::chrono;
    return duration_cast<microseconds>(
        high_resolution_clock::now().time_since_epoch()).count() / 1000.0;
}

} // namespace dc

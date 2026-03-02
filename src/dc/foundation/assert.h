#pragma once

#include <cassert>
#include <cstdio>

#ifdef NDEBUG
    #define dc_assert(expr) ((void)0)
#else
    #define dc_assert(expr) assert(expr)
#endif

#define dc_log(msg) std::fprintf(stderr, "[DC] %s\n", (msg))

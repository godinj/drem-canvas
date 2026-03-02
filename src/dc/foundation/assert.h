#pragma once

#include <cassert>
#include <cstdio>

#ifdef NDEBUG
    #define dc_assert(expr) ((void)0)
#else
    #define dc_assert(expr) assert(expr)
#endif

#define dc_log(fmt, ...) std::fprintf(stderr, "[DC] " fmt "\n" __VA_OPT__(,) __VA_ARGS__)

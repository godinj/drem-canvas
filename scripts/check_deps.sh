#!/bin/bash
# Lightweight dependency checker for Drem Canvas
# Exits 0 if ready to build, non-zero with diagnostics

set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Detect OS
OS="$(uname -s)"

ok=0
fail=0

check() {
    local label="$1"
    local result="$2"
    if [ "$result" -eq 0 ]; then
        printf "  [\033[32mOK\033[0m]   %s\n" "$label"
        ((ok++))
    else
        printf "  [\033[31mMISS\033[0m] %s\n" "$label"
        ((fail++))
    fi
}

# cmake >= 3.22
if command -v cmake &>/dev/null; then
    cmake_ver="$(cmake --version | head -1 | sed 's/[^0-9.]//g')"
    cmake_major="${cmake_ver%%.*}"
    cmake_minor="${cmake_ver#*.}"; cmake_minor="${cmake_minor%%.*}"
    if [ "$cmake_major" -gt 3 ] || { [ "$cmake_major" -eq 3 ] && [ "$cmake_minor" -ge 22 ]; }; then
        check "cmake >= 3.22 (found $cmake_ver)" 0
    else
        check "cmake >= 3.22 (found $cmake_ver)" 1
    fi
else
    check "cmake >= 3.22" 1
fi

# ninja
command -v ninja &>/dev/null
check "ninja" $?

# python3
command -v python3 &>/dev/null
check "python3" $?

# libpng (via pkg-config)
pkg-config --exists libpng 2>/dev/null
check "libpng (pkg-config)" $?

# Platform-specific checks
case "$OS" in
    Darwin)
        xcode-select -p &>/dev/null
        check "xcode-select (CLI tools)" $?
        ;;
    Linux)
        pkg-config --exists vulkan 2>/dev/null
        check "vulkan (pkg-config)" $?

        pkg-config --exists glfw3 2>/dev/null
        check "glfw3 (pkg-config)" $?

        pkg-config --exists fontconfig 2>/dev/null
        check "fontconfig (pkg-config)" $?

        pkg-config --exists alsa 2>/dev/null
        check "alsa (pkg-config)" $?

        # Wine version check — must be < 9.22 for yabridge plugin UIs
        if command -v wine &>/dev/null; then
            wine_ver="$(wine --version 2>/dev/null || echo 'unknown')"
            wine_major="$(echo "$wine_ver" | sed -n 's/^wine-\([0-9]*\)\..*/\1/p')"
            wine_minor="$(echo "$wine_ver" | sed -n 's/^wine-[0-9]*\.\([0-9]*\).*/\1/p')"
            if [ -n "$wine_major" ] && [ -n "$wine_minor" ]; then
                if [ "$wine_major" -gt 9 ] || { [ "$wine_major" -eq 9 ] && [ "$wine_minor" -ge 22 ]; }; then
                    check "Wine <= 9.21 (found $wine_ver — broken mouse coords in plugin UIs)" 1
                else
                    check "Wine <= 9.21 (found $wine_ver)" 0
                fi
            else
                check "Wine (found $wine_ver, could not parse version)" 0
            fi
        else
            check "Wine (not installed — needed for VST plugins via yabridge)" 1
        fi
        ;;
esac

# JUCE submodule
if [ -f "$PROJECT_ROOT/libs/JUCE/CMakeLists.txt" ]; then
    check "JUCE submodule" 0
else
    check "JUCE submodule (libs/JUCE/)" 1
fi

# Skia library
if [ -f "$PROJECT_ROOT/libs/skia/lib/libskia.a" ]; then
    check "Skia library (libs/skia/lib/libskia.a)" 0
else
    check "Skia library (libs/skia/lib/libskia.a)" 1
fi

# CMake configured
if [ -f "$PROJECT_ROOT/build/CMakeCache.txt" ]; then
    check "CMake configured (build/)" 0
else
    check "CMake configured (build/)" 1
fi

echo ""
echo "  $ok passed, $fail missing"

if [ "$fail" -gt 0 ]; then
    echo "  Run scripts/bootstrap.sh to fix missing dependencies."
    exit 1
fi
exit 0

#!/bin/bash
set -euo pipefail

# Drem Canvas — Bootstrap Script
# Takes any worktree from zero to buildable. Idempotent (safe to re-run).
#
# Usage: scripts/bootstrap.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

echo "=== Drem Canvas Bootstrap ==="
echo ""

# Detect OS
OS="$(uname -s)"

# ── 1. Check system prerequisites ──────────────────────────────────────────

echo "[1/5] Checking system prerequisites..."

missing=()

# cmake >= 3.22
if command -v cmake &>/dev/null; then
    cmake_ver="$(cmake --version | head -1 | sed 's/[^0-9.]//g')"
    cmake_major="${cmake_ver%%.*}"
    cmake_minor="${cmake_ver#*.}"; cmake_minor="${cmake_minor%%.*}"
    if [ "$cmake_major" -lt 3 ] || { [ "$cmake_major" -eq 3 ] && [ "$cmake_minor" -lt 22 ]; }; then
        missing+=("cmake")
    fi
else
    missing+=("cmake")
fi

command -v ninja &>/dev/null || missing+=("ninja")
command -v python3 &>/dev/null || missing+=("python3")
pkg-config --exists libpng 2>/dev/null || missing+=("libpng")

case "$OS" in
    Darwin)
        if ! xcode-select -p &>/dev/null; then
            echo "Error: Xcode command line tools not installed."
            echo "  Run: xcode-select --install"
            exit 1
        fi

        # Auto-install missing brew packages
        if [ ${#missing[@]} -gt 0 ]; then
            if command -v brew &>/dev/null; then
                echo "  Installing missing packages via Homebrew: ${missing[*]}"
                brew install "${missing[@]}"
            else
                echo "Error: Missing prerequisites: ${missing[*]}"
                echo "  Install Homebrew (https://brew.sh) or install manually:"
                echo "    brew install ${missing[*]}"
                exit 1
            fi
        fi
        ;;
    Linux)
        # Additional Linux-specific checks
        pkg-config --exists vulkan 2>/dev/null || missing+=("vulkan")
        pkg-config --exists glfw3 2>/dev/null || missing+=("glfw3")
        pkg-config --exists fontconfig 2>/dev/null || missing+=("fontconfig")
        pkg-config --exists alsa 2>/dev/null || missing+=("alsa")

        if [ ${#missing[@]} -gt 0 ]; then
            echo "Error: Missing prerequisites: ${missing[*]}"
            echo ""
            echo "  Install on Debian/Ubuntu:"
            echo "    sudo apt install cmake ninja-build python3 libpng-dev \\"
            echo "        libvulkan-dev libglfw3-dev libfontconfig-dev libasound2-dev"
            echo ""
            echo "  Install on Fedora:"
            echo "    sudo dnf install cmake ninja-build python3 libpng-devel \\"
            echo "        vulkan-devel glfw-devel fontconfig-devel alsa-lib-devel"
            exit 1
        fi
        ;;
    *)
        echo "Error: Unsupported OS: $OS"
        exit 1
        ;;
esac

echo "  All system prerequisites satisfied."

# ── 2. Init JUCE submodule ─────────────────────────────────────────────────

echo ""
echo "[2/5] Initializing JUCE submodule..."

if [ -f "libs/JUCE/CMakeLists.txt" ]; then
    echo "  Already initialized."
else
    git submodule update --init --depth 1 libs/JUCE
    echo "  Done."
fi

# ── 3. Skia — shared cache strategy ───────────────────────────────────────

echo ""
echo "[3/5] Setting up Skia..."

if [ -f "libs/skia/lib/libskia.a" ]; then
    echo "  Skia already available."
else
    # Detect bare repo root for shared cache
    git_common_dir="$(git rev-parse --git-common-dir 2>/dev/null || true)"
    bare_root=""

    if [ -n "$git_common_dir" ] && [ "$git_common_dir" != ".git" ]; then
        # We're in a worktree — bare repo root is the parent of .git-common-dir
        bare_root="$(cd "$git_common_dir" && pwd)"
        bare_root="$(dirname "$bare_root")"
    fi

    if [ -n "$bare_root" ] && [ "$bare_root" != "$PROJECT_ROOT" ]; then
        # Worktree mode: use shared cache
        shared_cache="$bare_root/.cache/skia"

        if [ -f "$shared_cache/lib/libskia.a" ]; then
            echo "  Symlinking shared Skia cache..."
            mkdir -p libs
            ln -sfn "$shared_cache" libs/skia
            echo "  Done: libs/skia -> $shared_cache"
        else
            echo "  Building Skia to shared cache (this takes 10-30 minutes)..."
            mkdir -p "$bare_root/.cache"
            "$SCRIPT_DIR/build_skia.sh" --output "$shared_cache"
            mkdir -p libs
            ln -sfn "$shared_cache" libs/skia
            echo "  Done: libs/skia -> $shared_cache"
        fi
    else
        # Standalone clone: build directly to libs/skia/
        echo "  Building Skia (this takes 10-30 minutes)..."
        "$SCRIPT_DIR/build_skia.sh"
    fi
fi

# ── 4. CMake configure ────────────────────────────────────────────────────

echo ""
echo "[4/5] Configuring CMake..."

if [ -f "build/CMakeCache.txt" ]; then
    echo "  Already configured."
else
    cmake --preset release
    echo "  Done."
fi

# ── 5. Done ───────────────────────────────────────────────────────────────

echo ""
echo "[5/5] Bootstrap complete!"
echo ""
echo "  Next steps:"
echo "    cmake --build --preset release"

case "$OS" in
    Darwin)
        echo '    open "build/DremCanvas_artefacts/Release/Drem Canvas.app"'
        ;;
    Linux)
        echo '    ./build/DremCanvas_artefacts/Release/DremCanvas'
        ;;
esac

echo ""
echo "  Check status anytime with: scripts/check_deps.sh"

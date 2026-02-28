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

        # Wine version check — Wine >= 9.22 breaks mouse coordinates in yabridge plugin UIs
        if command -v wine &>/dev/null; then
            wine_ver="$(wine --version 2>/dev/null || echo 'unknown')"
            wine_major="$(echo "$wine_ver" | sed -n 's/^wine-\([0-9]*\)\..*/\1/p')"
            wine_minor="$(echo "$wine_ver" | sed -n 's/^wine-[0-9]*\.\([0-9]*\).*/\1/p')"
            if [ -n "$wine_major" ] && [ -n "$wine_minor" ]; then
                if [ "$wine_major" -gt 9 ] || { [ "$wine_major" -eq 9 ] && [ "$wine_minor" -ge 22 ]; }; then
                    echo ""
                    echo "  WARNING: Wine $wine_ver is installed but >= 9.22."
                    echo "  Wine >= 9.22 has a regression that breaks mouse coordinates in"
                    echo "  yabridge-bridged VST plugin UIs. Pin to 9.21 with:"
                    echo ""
                    echo "    scripts/install-kilohearts.sh   (handles Wine pinning automatically)"
                    echo ""
                    echo "  Or manually:"
                    echo "    sudo apt install --allow-downgrades winehq-staging=9.21~\$(lsb_release -cs)-1 \\"
                    echo "        wine-staging=9.21~\$(lsb_release -cs)-1 \\"
                    echo "        wine-staging-amd64=9.21~\$(lsb_release -cs)-1 \\"
                    echo "        wine-staging-i386:i386=9.21~\$(lsb_release -cs)-1"
                    echo "    sudo apt-mark hold winehq-staging wine-staging wine-staging-amd64 wine-staging-i386"
                fi
            fi
        fi
        ;;
    *)
        echo "Error: Unsupported OS: $OS"
        exit 1
        ;;
esac

echo "  All system prerequisites satisfied."

# ── 2. Init JUCE submodule ─────────────────────────────────────────────────
#
# JUCE has local patches (IParameterFinder spatial hints, performEdit snoop)
# committed inside the submodule's git history. Each worktree has its OWN
# submodule git directory, so patch commits don't share across worktrees.
#
# Three-strategy fallback:
#   1. Normal submodule init (works if tracked commit exists upstream)
#   2. Fetch from a sibling worktree that has the patched commit
#   3. Apply .patch files from scripts/juce-patches/ (last resort)

echo ""
echo "[2/5] Initializing JUCE submodule..."

JUCE_PATCHED_COMMIT="b4474ca1ce2fdf48ed9d1155cc2420f9d3a1a861"
JUCE_UPSTREAM_BASE="501c07674e1ad693085a7e7c398f205c2677f5da"

if [ -f "libs/JUCE/CMakeLists.txt" ]; then
    echo "  Already initialized."
else
    juce_ok=false

    # Strategy 1: normal submodule init (works for upstream commits)
    echo "  Strategy 1: normal submodule init..."
    if git submodule update --init --depth 1 libs/JUCE 2>/dev/null; then
        if [ -f "libs/JUCE/CMakeLists.txt" ]; then
            juce_ok=true
            echo "  Done (upstream init)."
        fi
    fi

    # Strategy 2: fetch the patched commit from a sibling worktree
    if [ "$juce_ok" = false ]; then
        echo "  Strategy 2: fetching from sibling worktree..."
        git_common_dir="$(git rev-parse --git-common-dir 2>/dev/null || true)"
        bare_root=""

        if [ -n "$git_common_dir" ] && [ "$git_common_dir" != ".git" ]; then
            bare_root="$(cd "$git_common_dir" && pwd)"
            bare_root="$(dirname "$bare_root")"
        fi

        if [ -n "$bare_root" ]; then
            # Ensure submodule is at least initialized (even if wrong commit)
            git submodule init libs/JUCE 2>/dev/null || true

            # Search sibling worktrees for one that has the patched commit
            for sibling in "$bare_root"/*/libs/JUCE; do
                [ -d "$sibling/.git" ] || [ -f "$sibling/.git" ] || continue
                [ "$sibling" = "$PROJECT_ROOT/libs/JUCE" ] && continue

                if git -C "$sibling" cat-file -e "$JUCE_PATCHED_COMMIT" 2>/dev/null; then
                    echo "  Found patched commit in: $sibling"
                    git -C libs/JUCE fetch "$sibling" "$JUCE_PATCHED_COMMIT" 2>/dev/null || continue
                    git -C libs/JUCE checkout "$JUCE_PATCHED_COMMIT" 2>/dev/null || continue
                    if [ -f "libs/JUCE/CMakeLists.txt" ]; then
                        juce_ok=true
                        echo "  Done (fetched from sibling)."
                        break
                    fi
                fi
            done
        fi
    fi

    # Strategy 3: init upstream base and apply patches
    if [ "$juce_ok" = false ]; then
        echo "  Strategy 3: applying patches from scripts/juce-patches/..."
        patch_dir="$SCRIPT_DIR/juce-patches"

        if [ -d "$patch_dir" ] && ls "$patch_dir"/*.patch &>/dev/null; then
            # Init submodule at whatever upstream has
            git submodule update --init --depth 50 libs/JUCE 2>/dev/null || true

            # Try to checkout the upstream base commit the patches were built against
            git -C libs/JUCE checkout "$JUCE_UPSTREAM_BASE" 2>/dev/null || true

            # Apply each patch
            for p in "$patch_dir"/*.patch; do
                echo "  Applying: $(basename "$p")"
                git -C libs/JUCE am --3way "$p" || {
                    echo "  Warning: git am failed, trying git apply..."
                    git -C libs/JUCE am --abort 2>/dev/null || true
                    git -C libs/JUCE apply --3way "$p" || git -C libs/JUCE apply "$p"
                }
            done

            if [ -f "libs/JUCE/CMakeLists.txt" ]; then
                juce_ok=true
                echo "  Done (patches applied). Note: commit hash differs from tracked."
            fi
        else
            echo "  Error: No patch files found in $patch_dir"
        fi
    fi

    # Final verification
    if [ ! -f "libs/JUCE/CMakeLists.txt" ]; then
        echo "  ERROR: JUCE submodule initialization failed."
        echo "  libs/JUCE/CMakeLists.txt is missing."
        echo "  Try manually: git submodule update --init libs/JUCE"
        exit 1
    fi
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

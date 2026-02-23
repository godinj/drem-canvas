#!/bin/bash
set -euo pipefail

# Build Skia for macOS arm64 with Metal support
# Output: libs/skia/{include,lib/libskia.a}
#
# Usage:
#   scripts/build_skia.sh                    # build to libs/skia/
#   scripts/build_skia.sh --output /path/to  # build to custom directory

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

# Parse arguments
OUTPUT_DIR="$PROJECT_ROOT/libs/skia"
while [ $# -gt 0 ]; do
    case "$1" in
        --output)
            OUTPUT_DIR="$2"
            shift 2
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 [--output DIR]"
            exit 1
            ;;
    esac
done

# Skip if already built
if [ -f "$OUTPUT_DIR/lib/libskia.a" ]; then
    echo "Skia already built at $OUTPUT_DIR/lib/libskia.a — skipping."
    exit 0
fi

# Check prerequisites
for cmd in python3 ninja git; do
    if ! command -v "$cmd" &>/dev/null; then
        echo "Error: $cmd is required but not found."
        exit 1
    fi
done

# Pinned Skia milestone
SKIA_BRANCH="chrome/m131"

# Determine source clone location
# Use a sibling directory to the output: <parent>/skia-src/
SKIA_DIR="$(dirname "$OUTPUT_DIR")/skia-src"

# Fall back to legacy third_party/skia/ if it already exists
if [ -d "$PROJECT_ROOT/third_party/skia/.git" ]; then
    SKIA_DIR="$PROJECT_ROOT/third_party/skia"
    echo "Using existing Skia source at $SKIA_DIR"
fi

echo "=== Building Skia for macOS arm64 + Metal ==="
echo "  Source:  $SKIA_DIR"
echo "  Output:  $OUTPUT_DIR"
echo ""

# [1/5] Clone if not present
echo "[1/5] Cloning Skia source..."
if [ ! -d "$SKIA_DIR" ]; then
    mkdir -p "$(dirname "$SKIA_DIR")"
    git clone https://skia.googlesource.com/skia.git "$SKIA_DIR"
else
    echo "  Already cloned."
fi

cd "$SKIA_DIR"

# [2/5] Checkout pinned version
echo "[2/5] Checking out $SKIA_BRANCH..."
git fetch origin "$SKIA_BRANCH"
git checkout FETCH_HEAD

# [3/5] Sync dependencies (this also fetches bin/gn)
echo "[3/5] Syncing dependencies..."
python3 tools/git-sync-deps

# Verify gn exists
if [ ! -x "bin/gn" ]; then
    echo "Error: bin/gn not found after tools/git-sync-deps."
    echo "This tool is fetched by Skia's sync-deps script."
    echo "Try re-running or check network connectivity."
    exit 1
fi

# Detect libpng paths from pkg-config (Homebrew installs outside default paths)
PNG_CFLAGS="$(pkg-config --cflags libpng 2>/dev/null || true)"
PNG_LDFLAGS="$(pkg-config --libs-only-L libpng 2>/dev/null || true)"

# Build extra_cflags array
EXTRA_CFLAGS='["-mmacosx-version-min=13.0"'
for flag in $PNG_CFLAGS; do
    EXTRA_CFLAGS="$EXTRA_CFLAGS, \"$flag\""
done
EXTRA_CFLAGS="$EXTRA_CFLAGS]"

EXTRA_LDFLAGS='[]'
if [ -n "$PNG_LDFLAGS" ]; then
    EXTRA_LDFLAGS='['
    first=true
    for flag in $PNG_LDFLAGS; do
        if [ "$first" = true ]; then first=false; else EXTRA_LDFLAGS="$EXTRA_LDFLAGS, "; fi
        EXTRA_LDFLAGS="$EXTRA_LDFLAGS\"$flag\""
    done
    EXTRA_LDFLAGS="$EXTRA_LDFLAGS]"
fi

# [4/5] Generate build files and build
echo "[4/5] Generating build files with gn and building..."
bin/gn gen out/Release --args="
  is_official_build=true
  is_debug=false
  target_cpu=\"arm64\"
  target_os=\"mac\"
  skia_use_metal=true
  skia_enable_gpu=true
  skia_use_gl=false
  skia_use_vulkan=false
  skia_use_dawn=false
  skia_enable_skottie=false
  skia_enable_pdf=false
  skia_enable_svg=false
  skia_use_icu=false
  skia_use_harfbuzz=false
  skia_use_piex=false
  skia_use_wuffs=false
  skia_use_libwebp_decode=false
  skia_use_libwebp_encode=false
  skia_use_libpng_decode=true
  skia_use_libpng_encode=true
  skia_use_zlib=true
  skia_use_libjpeg_turbo_decode=false
  skia_use_libjpeg_turbo_encode=false
  extra_cflags=$EXTRA_CFLAGS
  extra_ldflags=$EXTRA_LDFLAGS
"

ninja -C out/Release skia

# [5/5] Install to output directory
echo "[5/5] Installing to $OUTPUT_DIR..."
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR/lib"
mkdir -p "$OUTPUT_DIR/include"

# Copy static library
cp out/Release/libskia.a "$OUTPUT_DIR/lib/"

# Copy headers (preserve directory structure)
cd "$SKIA_DIR"
find include -name '*.h' | while read -r header; do
    dest="$OUTPUT_DIR/$header"
    mkdir -p "$(dirname "$dest")"
    cp "$header" "$dest"
done

# Copy gpu/ganesh headers needed for Metal backend
find src/gpu -name '*.h' -path '*/ganesh/*' | while read -r header; do
    dest="$OUTPUT_DIR/$header"
    mkdir -p "$(dirname "$dest")"
    cp "$header" "$dest"
done

# Copy modules/skcms headers (required by SkColorSpace.h)
find modules/skcms -name '*.h' 2>/dev/null | while read -r header; do
    dest="$OUTPUT_DIR/$header"
    mkdir -p "$(dirname "$dest")"
    cp "$header" "$dest"
done


echo ""
echo "=== Skia build complete ==="
echo "  Library: $OUTPUT_DIR/lib/libskia.a"
echo "  Headers: $OUTPUT_DIR/include/"

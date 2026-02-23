#!/bin/bash
set -euo pipefail

# Build Skia for macOS arm64 with Metal support
# Output: libs/skia/{include,lib/libskia.a}

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
SKIA_DIR="$PROJECT_ROOT/third_party/skia"
OUTPUT_DIR="$PROJECT_ROOT/libs/skia"

# Pinned Skia milestone
SKIA_BRANCH="chrome/m131"

echo "=== Building Skia for macOS arm64 + Metal ==="

# Clone if not present
if [ ! -d "$SKIA_DIR" ]; then
    echo "Cloning Skia..."
    mkdir -p "$PROJECT_ROOT/third_party"
    git clone https://skia.googlesource.com/skia.git "$SKIA_DIR"
fi

cd "$SKIA_DIR"

# Checkout pinned version
echo "Checking out $SKIA_BRANCH..."
git fetch origin "$SKIA_BRANCH"
git checkout FETCH_HEAD

# Sync dependencies
echo "Syncing dependencies..."
python3 tools/git-sync-deps

# Generate build files
echo "Generating build files with gn..."
bin/gn gen out/Release --args='
  is_official_build=true
  is_debug=false
  target_cpu="arm64"
  target_os="mac"
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
  extra_cflags=["-mmacosx-version-min=13.0"]
'

# Build
echo "Building with ninja..."
ninja -C out/Release skia

# Install to libs/skia
echo "Installing to $OUTPUT_DIR..."
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

echo "=== Skia build complete ==="
echo "  Library: $OUTPUT_DIR/lib/libskia.a"
echo "  Headers: $OUTPUT_DIR/include/"

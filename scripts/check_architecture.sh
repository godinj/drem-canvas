#!/bin/sh
# Architecture enforcement script for Drem Canvas
#
# Checks that engine processors are real-time safe and dc:: headers are
# self-contained.
#
# Usage:
#   scripts/check_architecture.sh          # run all checks
#   scripts/check_architecture.sh --check N  # run only check N (1-2)
#
# Exit codes:
#   0 — all checks pass
#   1 — one or more checks failed

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_ROOT"

FAIL=0
RUN_CHECK=""

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --check) RUN_CHECK="$2"; shift 2 ;;
        *) echo "Unknown option: $1"; exit 1 ;;
    esac
done

should_run() {
    [ -z "$RUN_CHECK" ] || [ "$RUN_CHECK" = "$1" ]
}

#==============================================================================
# Check 1: Real-time safety in engine Processor files
#
# Engine *Processor.cpp files must not contain heap allocation, blocking,
# or I/O. Override comments: '// RT-safe:' and '// not on audio thread'.
#==============================================================================

if should_run 1; then
    printf "Check 1: Real-time safety (engine processors) ... "

    # Build the forbidden pattern list
    RT_FORBIDDEN='(^|[^a-zA-Z_])(new |delete |malloc|free|realloc|calloc)[^a-zA-Z_]'
    RT_FORBIDDEN="$RT_FORBIDDEN|std::mutex|lock_guard|unique_lock|condition_variable"
    RT_FORBIDDEN="$RT_FORBIDDEN|std::cout|std::cerr|printf|fprintf|fopen|fwrite"
    RT_FORBIDDEN="$RT_FORBIDDEN|std::thread|pthread_create"

    # Find all *Processor.cpp files under src/engine/
    PROC_FILES=$(find src/engine -name '*Processor.cpp' 2>/dev/null || true)

    hits=""
    if [ -n "$PROC_FILES" ]; then
        for f in $PROC_FILES; do
            found=$(grep -n -E "$RT_FORBIDDEN" "$f" 2>/dev/null \
                    | grep -v '// RT-safe:' \
                    | grep -v '// not on audio thread' \
                    || true)
            if [ -n "$found" ]; then
                hits="$hits
$f:
$found"
            fi
        done
    fi

    if [ -n "$hits" ]; then
        echo "FAIL"
        echo "$hits"
        echo ""
        echo "ERROR: Potentially unsafe operations in audio processor code."
        echo "Add '// RT-safe:' or '// not on audio thread' comment to suppress."
        FAIL=1
    else
        echo "OK"
    fi
fi

#==============================================================================
# Check 2: dc:: header self-containment
#
# Each dc:: .h file must compile independently (no missing includes).
# Headers that depend on external system libraries (portaudio, RtMidi)
# are tested with the appropriate pkg-config flags when available,
# and skipped when the library is not installed.
#==============================================================================

if should_run 2; then
    printf "Check 2: dc:: header self-containment ... "

    # Only run if a C++ compiler is available
    if command -v c++ >/dev/null 2>&1; then
        HEADER_FAIL=""
        HEADER_SKIP=""
        HEADERS=$(find src/dc -name '*.h' 2>/dev/null || true)

        # Gather system library flags
        PORTAUDIO_FLAGS=""
        if command -v pkg-config >/dev/null 2>&1; then
            PORTAUDIO_FLAGS=$(pkg-config --cflags portaudio-2.0 2>/dev/null || true)
        fi

        RTMIDI_FLAGS=""
        if command -v pkg-config >/dev/null 2>&1; then
            RTMIDI_FLAGS=$(pkg-config --cflags rtmidi 2>/dev/null || true)
        fi

        # VST3 SDK (fetched by CMake into build/_deps/vst3sdk-src/)
        VST3_SDK_DIR=""
        for build_dir in build build-debug build-coverage; do
            candidate="$build_dir/_deps/vst3sdk-src"
            if [ -d "$candidate/pluginterfaces" ]; then
                VST3_SDK_DIR="$candidate"
                break
            fi
        done

        for header in $HEADERS; do
            # Determine extra flags needed for external dependencies
            EXTRA_FLAGS=""
            NEEDS_SYSTEM_LIB=""

            case "$header" in
                *PortAudio*)
                    EXTRA_FLAGS="$PORTAUDIO_FLAGS"
                    NEEDS_SYSTEM_LIB="portaudio"
                    ;;
                *MidiDeviceManager*)
                    EXTRA_FLAGS="$RTMIDI_FLAGS"
                    NEEDS_SYSTEM_LIB="rtmidi"
                    ;;
                *dc/plugins/ComponentHandler* | *dc/plugins/PluginEditor* | \
                *dc/plugins/PluginInstance* | *dc/plugins/VST3Host*)
                    if [ -n "$VST3_SDK_DIR" ]; then
                        EXTRA_FLAGS="-I $VST3_SDK_DIR"
                    else
                        NEEDS_SYSTEM_LIB="vst3sdk (run cmake --preset release first)"
                    fi
                    ;;
            esac

            # Skip if system library is not available
            if [ -n "$NEEDS_SYSTEM_LIB" ] && [ -z "$EXTRA_FLAGS" ]; then
                # Try without flags anyway — header might just forward-declare
                tmpfile=$(mktemp /tmp/dc_header_check_XXXXXX.cpp)
                echo "#include \"$header\"" > "$tmpfile"

                if ! c++ -std=c++17 -fsyntax-only -I . -I src/ -I build/generated/ \
                         -x c++ "$tmpfile" 2>/dev/null; then
                    HEADER_SKIP="$HEADER_SKIP
  $header (missing $NEEDS_SYSTEM_LIB)"
                    rm -f "$tmpfile"
                    continue
                fi
                rm -f "$tmpfile"
            fi

            # Create a temp file that just includes the header
            tmpfile=$(mktemp /tmp/dc_header_check_XXXXXX.cpp)
            echo "#include \"$header\"" > "$tmpfile"

            # shellcheck disable=SC2086
            if ! c++ -std=c++17 -fsyntax-only -I . -I src/ -I build/generated/ \
                     $EXTRA_FLAGS -x c++ "$tmpfile" 2>/dev/null; then
                HEADER_FAIL="$HEADER_FAIL
  $header"
            fi

            rm -f "$tmpfile"
        done

        if [ -n "$HEADER_FAIL" ]; then
            echo "FAIL"
            echo "The following dc:: headers are not self-contained:$HEADER_FAIL"
            if [ -n "$HEADER_SKIP" ]; then
                echo ""
                echo "Skipped (missing system library):$HEADER_SKIP"
            fi
            FAIL=1
        else
            echo "OK"
            if [ -n "$HEADER_SKIP" ]; then
                echo "  (skipped due to missing system libraries:$HEADER_SKIP)"
            fi
        fi
    else
        echo "SKIP (no C++ compiler found)"
    fi
fi

#==============================================================================
# Summary
#==============================================================================

echo ""
if [ "$FAIL" -ne 0 ]; then
    echo "Architecture check FAILED"
    exit 1
else
    echo "All architecture checks passed."
    exit 0
fi

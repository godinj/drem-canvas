#!/bin/sh
# Architecture enforcement script for Drem Canvas
#
# Checks that dc:: libraries remain JUCE-free, boundary types stay in
# documented boundary files, ColourBridge is used correctly, and engine
# processors are real-time safe.
#
# Usage:
#   scripts/check_architecture.sh          # run all checks
#   scripts/check_architecture.sh --check N  # run only check N (1-5)
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
# Check 1: JUCE-free dc:: libraries
#
# No dc:: source file may contain JUCE includes or JUCE type references.
# Comments that mention juce:: for documentation purposes (e.g., "Replaces
# juce::MidiInput") are excluded — only non-comment references matter.
#==============================================================================

if should_run 1; then
    printf "Check 1: JUCE-free dc:: libraries ... "

    JUCE_PATTERNS='#include.*Juce|#include.*juce_|JuceHeader'
    DC_DIRS="src/dc/foundation src/dc/model src/dc/midi src/dc/audio"

    FOUND=""
    for dir in $DC_DIRS; do
        if [ -d "$dir" ]; then
            # Check for JUCE includes (these are never OK, even in comments)
            hits=$(grep -rn -E "$JUCE_PATTERNS" "$dir" \
                       --include='*.h' --include='*.cpp' 2>/dev/null || true)
            if [ -n "$hits" ]; then
                FOUND="$FOUND
$hits"
            fi

            # Check for juce:: type references, excluding comments
            # Lines starting with // or /// or containing juce:: only inside a comment
            hits=$(grep -rn -E 'juce::' "$dir" \
                       --include='*.h' --include='*.cpp' 2>/dev/null \
                   | grep -v -E '^\s*//' \
                   | grep -v -E ':\s*//' \
                   | grep -v -E ':\s*\*' \
                   | grep -v -E ':\s*/\*' \
                   || true)
            if [ -n "$hits" ]; then
                FOUND="$FOUND
$hits"
            fi
        fi
    done

    if [ -n "$FOUND" ]; then
        echo "FAIL"
        echo "$FOUND"
        echo ""
        echo "ERROR: JUCE reference found in dc:: library sources."
        echo "dc:: libraries must have zero JUCE dependencies."
        FAIL=1
    else
        echo "OK"
    fi
fi

#==============================================================================
# Check 2: JUCE boundary type enforcement outside dc::
#
# Outside dc:: libraries, juce::String, juce::File, juce::Array, and
# juce::Colour should only appear in documented boundary files:
# src/gui/, src/ui/, src/plugins/, src/Main.cpp, *Processor.h
#==============================================================================

if should_run 2; then
    printf "Check 2: JUCE boundary types ... "

    # juce::Colour[^BGI] avoids matching juce::ColourGradient, juce::ColourId (legitimate JUCE API)
    BOUNDARY_TYPES='juce::String|juce::File|juce::Array|juce::Colour[^BGI]'
    BOUNDARY_EXCLUDES='src/gui/|src/ui/|src/plugins/|src/Main\.cpp|Processor\.h'

    hits=$(grep -rn -E "$BOUNDARY_TYPES" src/ \
               --include='*.h' --include='*.cpp' --include='*.mm' 2>/dev/null \
           | grep -v -E "$BOUNDARY_EXCLUDES" \
           | grep -v '// JUCE API boundary' \
           || true)

    if [ -n "$hits" ]; then
        echo "FAIL"
        echo "$hits"
        echo ""
        echo "ERROR: JUCE boundary types found outside allowed boundary files."
        echo "Use dc:: equivalents or mark with '// JUCE API boundary' comment."
        FAIL=1
    else
        echo "OK"
    fi
fi

#==============================================================================
# Check 3: ColourBridge enforcement
#
# GUI files must not construct juce::Colour directly — they must use
# dc::bridge::toJuce().
#==============================================================================

if should_run 3; then
    printf "Check 3: ColourBridge enforcement ... "

    hits=""
    for dir in src/gui src/ui; do
        if [ -d "$dir" ]; then
            found=$(grep -rn 'juce::Colour(' "$dir" \
                        --include='*.h' --include='*.cpp' 2>/dev/null \
                    | grep -v 'ColourBridge' \
                    | grep -v '// JUCE API boundary' \
                    || true)
            if [ -n "$found" ]; then
                hits="$hits
$found"
            fi
        fi
    done

    if [ -n "$hits" ]; then
        echo "FAIL"
        echo "$hits"
        echo ""
        echo "ERROR: Direct juce::Colour() construction in GUI code."
        echo "Use dc::bridge::toJuce() from ColourBridge.h instead."
        FAIL=1
    else
        echo "OK"
    fi
fi

#==============================================================================
# Check 4: Real-time safety in engine Processor files
#
# Engine *Processor.cpp files must not contain heap allocation, blocking,
# or I/O. Override comments: '// RT-safe:' and '// not on audio thread'.
#==============================================================================

if should_run 4; then
    printf "Check 4: Real-time safety (engine processors) ... "

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
# Check 5: dc:: header self-containment
#
# Each dc:: .h file must compile independently (no missing includes).
# Headers that depend on external system libraries (portaudio, RtMidi)
# are tested with the appropriate pkg-config flags when available,
# and skipped when the library is not installed.
#==============================================================================

if should_run 5; then
    printf "Check 5: dc:: header self-containment ... "

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

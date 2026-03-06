# Drem Canvas — Benchmarking Strategy

How to measure Drem Canvas against elite engineering standards. Organized into
five phases, from zero-cost analysis (git history, grep) through automated
tooling to continuous enforcement.

**Reference**: [Code Analysis Criteria](code-analysis-criteria.md) — the
thresholds and metrics used throughout this document.

---

## Table of Contents

1. [Codebase Profile](#codebase-profile)
2. [Phase 1: Static Inventory (no tools required)](#phase-1-static-inventory)
3. [Phase 2: Structural Analysis (script-based)](#phase-2-structural-analysis)
4. [Phase 3: Behavioral Analysis (git history)](#phase-3-behavioral-analysis)
5. [Phase 4: Dynamic Analysis (runtime)](#phase-4-dynamic-analysis)
6. [Phase 5: Continuous Enforcement](#phase-5-continuous-enforcement)
7. [Scorecard](#scorecard)
8. [Priority Order](#priority-order)

---

## Codebase Profile

| Property | Value |
|----------|-------|
| Language | C++17 |
| Source LOC | ~39,600 |
| Test LOC | ~11,725 |
| Source files | ~291 (.h, .cpp, .mm) |
| Test files | 52 unit/integration + 12 e2e |
| Modules | 21 directories under src/ |
| Build system | CMake + Ninja |
| Test framework | Catch2 v3.8.1 + trompeloeil |
| Existing static analysis | .clang-tidy (bugprone, cppcoreguidelines, modernize, performance, readability) |
| Existing arch checks | RT-safety grep (engine processors), header self-containment |
| Coverage tooling | LLVM source-based (preset exists, not routinely run) |

### Known Hotspot Candidates

| File | Lines | Concern |
|------|-------|---------|
| `src/vim/VimEngine.cpp` | 3,778 | Largest file; likely high cyclomatic complexity |
| `src/ui/AppController.cpp` | 2,383 | God class candidate — owns AudioEngine, Project, VimEngine, all UI |
| `src/dc/plugins/PluginInstance.cpp` | 1,225 | Complex VST3 setup logic |
| `src/Main.cpp` | 1,040 | Entry point + E2E test harness mixed in one file |

---

## Phase 1: Static Inventory (no tools required)

Zero-cost measurements using grep, wc, and awk on the source tree.

### 1.1 File Size Census

**Metric**: Lines per file (Warning > 400, Critical > 750)

```bash
# Script: scripts/benchmark/file-sizes.sh
find src/ -name '*.cpp' -o -name '*.h' | while read f; do
    wc -l < "$f" | xargs printf "%6d %s\n" "$1" "$f"
done | sort -rn | head -30
```

**Pass criteria**: No file > 750 LOC. Fewer than 10% of files > 400 LOC.

### 1.2 Function Length Census

**Metric**: Lines per function (Warning > 40, Critical > 60)

```bash
# Approximate: count lines between function-opening { and closing }
# More accurate: use clang-tidy readability-function-size check
clang-tidy -checks='-*,readability-function-size' \
  -config='{CheckOptions: [{key: readability-function-size.LineThreshold, value: 40}]}' \
  src/**/*.cpp -p build-debug/ 2>&1 | grep 'warning:' | sort
```

**Pass criteria**: No function > 60 LOC. Fewer than 5% of functions > 40 LOC.

### 1.3 Parameter Count Census

**Metric**: Parameters per function (Warning > 3, Critical > 5)

```bash
clang-tidy -checks='-*,readability-function-size' \
  -config='{CheckOptions: [{key: readability-function-size.ParameterThreshold, value: 4}]}' \
  src/**/*.cpp -p build-debug/ 2>&1 | grep 'warning:'
```

**Pass criteria**: No public API function > 5 parameters.

### 1.4 Include Depth Survey

**Metric**: Transitive include count per TU (Healthy < 100, Problematic > 450)

```bash
# For each .cpp file, count transitive includes
for f in src/**/*.cpp; do
    count=$(c++ -std=c++17 -I src/ -I . -H "$f" 2>&1 | grep '^\.' | wc -l)
    printf "%4d %s\n" "$count" "$f"
done | sort -rn | head -20
```

**Pass criteria**: No TU > 200 transitive includes. Median < 80.

### 1.5 Test-to-Code Ratio

**Metric**: Test LOC / Source LOC (Target 1:1 to 3:1, Poor < 0.5:1)

```bash
SRC=$(find src/ -name '*.cpp' -o -name '*.h' | xargs wc -l | tail -1 | awk '{print $1}')
TEST=$(find tests/ -name '*.cpp' | xargs wc -l | tail -1 | awk '{print $1}')
echo "Source: $SRC, Tests: $TEST, Ratio: $(echo "scale=2; $TEST / $SRC" | bc)"
```

**Current estimate**: ~11,725 / ~39,600 = **0.30:1** (below target).

### 1.6 Raw RT-Safety Audit

**Metric**: Forbidden patterns in audio-path code (Target: 0 violations)

Expand existing `check_architecture.sh` check 1 to cover:
- `src/engine/*.cpp` (all engine files, not just `*Processor.cpp`)
- `src/dc/engine/*.cpp` (the AudioGraph and node system)
- `src/dc/audio/*.cpp` (audio file I/O — verify which functions run on audio thread)

```bash
# Extended forbidden pattern list
RT_FORBIDDEN='(^|[^a-zA-Z_])(new |delete |malloc|free|realloc|calloc)[^a-zA-Z_]'
RT_FORBIDDEN="$RT_FORBIDDEN|std::mutex|lock_guard|unique_lock|condition_variable"
RT_FORBIDDEN="$RT_FORBIDDEN|std::cout|std::cerr|printf|fprintf|fopen|fwrite|fread"
RT_FORBIDDEN="$RT_FORBIDDEN|std::thread|pthread_create|pthread_mutex"
RT_FORBIDDEN="$RT_FORBIDDEN|std::vector|std::map|std::string"  # may allocate
RT_FORBIDDEN="$RT_FORBIDDEN|throw "
```

**Pass criteria**: 0 unexcused violations in audio-thread code paths.

### 1.7 Duplicate Code Detection

**Metric**: Duplicated blocks (Warning: any block >= 10 lines / 100 tokens)

```bash
# PMD CPD (Copy-Paste Detector) — works on C++
pmd cpd --minimum-tokens 100 --language cpp --dir src/
```

Alternative: SonarQube's duplication engine or jscpd.

**Pass criteria**: < 3% duplication density.

---

## Phase 2: Structural Analysis (script-based)

Custom analysis scripts that parse or instrument the source.

### 2.1 Complexity Analysis

**Metrics**: Cyclomatic complexity, Cognitive complexity per function

**Tool**: clang-tidy with `readability-function-cognitive-complexity`

Current `.clang-tidy` threshold is **25** (above the industry-standard **15**).

```bash
# Run cognitive complexity check at industry threshold
clang-tidy -checks='-*,readability-function-cognitive-complexity' \
  -config='{CheckOptions: [{key: readability-function-cognitive-complexity.Threshold, value: 15}]}' \
  src/**/*.cpp -p build-debug/ 2>&1 | grep 'warning:' | \
  sed 's/.*cognitive complexity of \([0-9]*\).*/\1/' | sort -rn
```

**Benchmark tiers**:
| Tier | Cognitive Complexity Threshold | Interpretation |
|------|-------------------------------|----------------|
| Elite | <= 15 (all functions) | Industry standard |
| Good | <= 25 (current .clang-tidy) | Acceptable for complex domain |
| Needs work | > 25 | Refactoring candidates |

**Pass criteria**: 0 functions > 25. Fewer than 10 functions > 15.

### 2.2 Class Size and Cohesion

**Metrics**: Methods per class, Fields per class, LOC per class

Script to extract from headers:

```bash
# scripts/benchmark/class-metrics.sh
# For each .h file, count:
#   - public/protected/private method declarations
#   - member variable declarations
#   - total lines in class body
```

**God Class candidates**: Check AppController, VimEngine, PluginInstance against:
- Methods > 30? (likely yes for AppController)
- Fields > 20? (likely yes for AppController)
- Lines > 750? (AppController is 2,383 lines in .cpp alone)

**Pass criteria**: No class with methods > 30 AND fields > 20 AND LOC > 750.

### 2.3 Dependency Direction Enforcement

**Metric**: Layer violations (Target: 0)

Define the allowed dependency graph for Drem Canvas:

```
Layer 0 (foundation):  src/dc/foundation/     → (nothing internal)
Layer 1 (primitives):  src/dc/model/           → Layer 0
                       src/dc/midi/            → Layer 0
                       src/dc/audio/           → Layer 0
Layer 2 (engine):      src/dc/engine/          → Layers 0-1
                       src/dc/plugins/         → Layers 0-2
Layer 3 (app model):   src/model/              → Layers 0-2
                       src/engine/             → Layers 0-3
                       src/plugins/            → Layers 0-3
                       src/vim/                → Layers 0-3
Layer 4 (graphics):    src/graphics/           → Layers 0-1 (no engine!)
Layer 5 (ui):          src/ui/                 → Layers 0-4
                       src/platform/           → Layers 0-4
```

Enforcement script:

```bash
# scripts/benchmark/check-layers.sh
# For each .cpp/.h file, extract #include directives
# Verify each include respects the layer ordering above
# Flag any upward dependency (e.g., dc/foundation including dc/model)
```

**Specific rules to check**:
- `src/dc/foundation/` includes NOTHING from `src/dc/model/`, `src/dc/engine/`, etc.
- `src/dc/model/` includes NOTHING from `src/dc/engine/` or `src/dc/plugins/`
- `src/engine/` includes NOTHING from `src/ui/` or `src/graphics/`
- `src/graphics/core/` includes NOTHING from `src/engine/` or `src/model/`
- `src/dc/*` includes NOTHING from `src/engine/`, `src/model/`, `src/ui/`, `src/plugins/`, `src/vim/`

**Pass criteria**: 0 layer violations.

### 2.4 Circular Dependency Detection

**Metric**: Circular include dependencies (Target: 0)

```bash
# scripts/benchmark/circular-deps.sh
# Build include graph: for each file, extract includes
# Run topological sort; any cycle = failure
# Tools: include-what-you-use --graph, or custom Python script
```

**Pass criteria**: 0 circular dependencies between modules.

### 2.5 Header Self-Containment

**Metric**: Every .h file compiles independently (Target: 100%)

Already implemented in `scripts/check_architecture.sh` check 2, but only for
`src/dc/` headers. Extend to ALL headers:

```bash
# Extend check_architecture.sh to cover src/**/*.h, not just src/dc/*.h
```

**Pass criteria**: 100% of headers compile independently.

### 2.6 Bool Parameter Audit

**Metric**: Bool parameters in public API functions (Target: 0)

```bash
# Grep for bool parameters in public method declarations
grep -rn 'bool [a-z]' src/**/*.h | grep -v '//' | grep -v 'return' | \
  grep -v 'const bool' | grep -v 'private:' -A 100
```

Better approach: clang-tidy custom check or AST-based script.

**Pass criteria**: 0 bool parameters in dc:: public API. Document exceptions.

### 2.7 Raw new/delete Audit

**Metric**: Raw `new`/`delete` outside RAII wrappers (Target: 0 in dc::)

```bash
grep -rn '\bnew \|delete ' src/dc/ | grep -v '// RT-safe:' | \
  grep -v 'unique_ptr\|shared_ptr\|make_unique\|make_shared'
```

**Pass criteria**: 0 raw new/delete in `src/dc/`. Document exceptions in app layer.

---

## Phase 3: Behavioral Analysis (git history)

Analysis of version control history to identify risk patterns.

### 3.1 Hotspot Analysis

**Metric**: Change frequency x file size (proxy for complexity)

```bash
# scripts/benchmark/hotspots.sh
# Top 20 most-changed files weighted by current size
git log --format=format: --name-only --since="6 months ago" | \
  grep -E '\.(cpp|h)$' | sort | uniq -c | sort -rn | head -30
```

Cross-reference with file size to find hotspots (high churn + large files).

**Output**: Ranked list of files needing attention. Files in both "most changed"
and "largest" lists are priority refactoring targets.

### 3.2 Change Coupling

**Metric**: Files that change together > 20% of commits

```bash
# scripts/benchmark/change-coupling.sh
# For each commit, record the set of files changed
# For each file pair, count co-change frequency
# Flag pairs crossing module boundaries
```

**Pass criteria**: No cross-module file pairs with > 20% co-change rate.

### 3.3 Code Age Analysis

**Metric**: Time since last modification per file

```bash
# scripts/benchmark/code-age.sh
git log -1 --format="%ai" -- "$file"  # for each source file
```

**Flag**: Files not touched in > 12 months that are in hotspot areas (frequently
included by actively-changing code).

### 3.4 Churn Rate

**Metric**: Relative code churn (Microsoft Research predictor)

```bash
# scripts/benchmark/churn.sh
# For each file: (lines added + lines deleted) / total lines
# Over last 6 months
git log --numstat --since="6 months ago" -- src/ | \
  awk '/^[0-9]/ {add[$3]+=$1; del[$3]+=$2} END {for(f in add) print add[f]+del[f], f}' | \
  sort -rn | head -20
```

**Pass criteria**: No file with churn ratio > 3x its current size (suggests
instability or design issues).

### 3.5 Knowledge Distribution

**Metric**: Bus factor per module

```bash
# scripts/benchmark/knowledge.sh
# For each directory under src/, count distinct authors
for dir in src/dc/foundation src/dc/model src/dc/engine src/dc/plugins \
           src/engine src/model src/ui src/graphics src/vim src/plugins; do
    authors=$(git log --format='%aN' -- "$dir/" | sort -u | wc -l)
    echo "$authors authors: $dir"
done
```

**Note**: For a solo/small-team project, bus factor will inherently be low. This
metric becomes relevant as the team grows.

---

## Phase 4: Dynamic Analysis (runtime)

Requires building and running the application or tests with special
instrumentation.

### 4.1 Code Coverage

**Metric**: Line and branch coverage (Target: > 80% branch)

The `coverage` CMake preset already exists:

```bash
cmake --preset coverage
cmake --build --preset coverage
ctest --test-dir build-coverage --output-on-failure
# Generate report
llvm-profdata merge -sparse build-coverage/*.profraw -o coverage.profdata
llvm-cov report build-coverage/dc_unit_tests -instr-profile=coverage.profdata \
  --ignore-filename-regex='tests/|_deps/'
llvm-cov show build-coverage/dc_unit_tests -instr-profile=coverage.profdata \
  --format=html --output-dir=coverage-report/ \
  --ignore-filename-regex='tests/|_deps/'
```

**Benchmark tiers**:
| Module | Line Target | Branch Target |
|--------|-------------|---------------|
| `dc/foundation` | > 90% | > 85% |
| `dc/model` | > 90% | > 85% |
| `dc/midi` | > 85% | > 80% |
| `dc/audio` | > 80% | > 75% |
| `dc/engine` | > 80% | > 75% |
| `dc/plugins` | > 70% | > 65% |
| `engine/` (app) | > 70% | > 65% |
| `model/` (app) | > 80% | > 75% |

### 4.2 AddressSanitizer

**Metric**: Memory errors (Target: 0)

```bash
# Add to CMake:
# -fsanitize=address -fno-omit-frame-pointer
cmake -B build-asan -DCMAKE_CXX_FLAGS="-fsanitize=address -fno-omit-frame-pointer" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-asan
ASAN_OPTIONS=detect_leaks=1 ctest --test-dir build-asan
```

**Pass criteria**: 0 ASan findings in unit/integration tests.

### 4.3 UndefinedBehaviorSanitizer

**Metric**: UB violations (Target: 0)

```bash
cmake -B build-ubsan -DCMAKE_CXX_FLAGS="-fsanitize=undefined" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-ubsan
ctest --test-dir build-ubsan
```

**Pass criteria**: 0 UBSan findings.

### 4.4 ThreadSanitizer

**Metric**: Data races and deadlocks (Target: 0)

```bash
cmake -B build-tsan -DCMAKE_CXX_FLAGS="-fsanitize=thread" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-tsan
ctest --test-dir build-tsan
```

**Pass criteria**: 0 TSan findings. Critical for audio engine correctness.

### 4.5 Build Time Benchmarks

**Metric**: Clean build time, incremental rebuild time

```bash
# Clean build
cmake --preset release
time cmake --build --preset release --clean-first

# Incremental: touch one file and rebuild
touch src/model/Project.cpp
time cmake --build --preset release

# Preprocessed line count for worst-case TU
c++ -std=c++17 -I src/ -I . -E src/ui/AppController.cpp 2>/dev/null | wc -l
```

**Pass criteria**: Clean build < 5 min. Incremental < 10 sec.

### 4.6 RealtimeSanitizer (if LLVM 20+ available)

**Metric**: Real-time violations in audio callback (Target: 0)

```bash
# Requires Clang 20+
cmake -B build-rtsan -DCMAKE_CXX_FLAGS="-fsanitize=realtime" \
  -DCMAKE_BUILD_TYPE=Debug
cmake --build build-rtsan
# Run with audio processing active
./build-rtsan/DremCanvas --smoke
```

**Prerequisite**: Annotate audio callbacks with `[[clang::nonblocking]]`.

**Pass criteria**: 0 RTSan violations during audio processing tests.

### 4.7 Test Reliability

**Metric**: Flaky test rate (Target: < 2%)

```bash
# Run tests 10 times, track failures
for i in $(seq 1 10); do
    ctest --test-dir build-debug --output-on-failure -j$(nproc) 2>&1 | \
      tail -5 >> test-reliability.log
done
# Count tests that fail inconsistently
```

**Pass criteria**: 0 flaky tests across 10 runs.

---

## Phase 5: Continuous Enforcement

Integrate benchmarks into the development workflow so scores can only improve.

### 5.1 Tighten .clang-tidy

Current cognitive complexity threshold is 25. Strategy to ratchet down:

1. **Baseline**: Run at threshold 15, record all violations
2. **Grandfather**: Create `// NOLINT` exemptions for existing violations with
   tracking comments
3. **Enforce**: Set threshold to 15 for new code
4. **Reduce**: Remove one `NOLINT` per sprint as functions are refactored

### 5.2 Add Architecture Fitness Functions to verify.sh

Extend `scripts/verify.sh` with new checks:

```bash
# Check 3: Layer dependency enforcement
scripts/benchmark/check-layers.sh

# Check 4: File size limits (no file > 750 LOC)
oversized=$(find src/ -name '*.cpp' -o -name '*.h' | xargs wc -l | \
  awk '$1 > 750 {print}' | grep -v total)
if [ -n "$oversized" ]; then
    echo "FAIL: Files exceeding 750 LOC:"
    echo "$oversized"
    ERRORS=$((ERRORS + 1))
fi

# Check 5: No raw new/delete in dc::
raw_alloc=$(grep -rn '\bnew \|\bdelete ' src/dc/ | \
  grep -v '// RT-safe:' | grep -v 'unique_ptr\|shared_ptr\|make_' | \
  grep -v '\.md:' || true)
if [ -n "$raw_alloc" ]; then
    echo "FAIL: Raw new/delete in dc:: namespace:"
    echo "$raw_alloc"
    ERRORS=$((ERRORS + 1))
fi
```

### 5.3 Coverage Gate

Add minimum coverage enforcement:

```bash
# In CI or verify.sh
MIN_COVERAGE=70
actual=$(llvm-cov report ... | grep TOTAL | awk '{print $NF}' | tr -d '%')
if [ "$actual" -lt "$MIN_COVERAGE" ]; then
    echo "FAIL: Coverage $actual% < minimum $MIN_COVERAGE%"
fi
```

Ratchet: increase minimum by 2% per quarter.

### 5.4 Sanitizer CI Targets

Add CMake presets for sanitizer builds:

```json
{
  "name": "asan",
  "inherits": "test",
  "cacheVariables": {
    "CMAKE_CXX_FLAGS": "-fsanitize=address -fno-omit-frame-pointer"
  }
},
{
  "name": "tsan",
  "inherits": "test",
  "cacheVariables": {
    "CMAKE_CXX_FLAGS": "-fsanitize=thread"
  }
},
{
  "name": "ubsan",
  "inherits": "test",
  "cacheVariables": {
    "CMAKE_CXX_FLAGS": "-fsanitize=undefined"
  }
}
```

### 5.5 Benchmark Dashboard Script

Single script that runs all measurements and produces a scorecard:

```bash
# scripts/benchmark/scorecard.sh
# Runs all Phase 1 + Phase 2 checks
# Outputs results in a structured format (JSON or markdown table)
# Can be diffed against previous runs to track progress
```

---

## Scorecard

Summary of all dimensions with current assessment methodology and targets.

| # | Dimension | Metric | Tool | Target | Drem Canvas Status |
|---|-----------|--------|------|--------|--------------------|
| 1 | Function complexity | Cognitive complexity <= 15 | clang-tidy | 0 violations | TBD (baseline needed) |
| 2 | Function size | LOC <= 40 | clang-tidy | < 5% over | TBD |
| 3 | Class size | LOC <= 750 | wc -l | 0 violations | Known: 4 files exceed |
| 4 | Class cohesion | God class detection | Manual / script | 0 God classes | AppController is candidate |
| 5 | Layer enforcement | Dependency direction | check-layers.sh | 0 violations | TBD |
| 6 | Circular deps | Cycle detection | Include graph | 0 cycles | TBD |
| 7 | Header self-containment | Independent compilation | check_architecture.sh | 100% | Partial (dc:: only) |
| 8 | RT-safety (static) | Forbidden patterns | quick-check.sh | 0 violations | Enforced for Processor files |
| 9 | RT-safety (runtime) | RTSan | -fsanitize=realtime | 0 violations | Not yet available |
| 10 | RAII compliance | Raw new/delete | grep | 0 in dc:: | TBD |
| 11 | Memory safety | ASan | -fsanitize=address | 0 findings | TBD |
| 12 | Thread safety | TSan | -fsanitize=thread | 0 findings | TBD |
| 13 | Undefined behavior | UBSan | -fsanitize=undefined | 0 findings | TBD |
| 14 | Test coverage | Branch >= 80% | llvm-cov | By module | TBD |
| 15 | Test ratio | Test/Source LOC | wc -l | >= 1:1 | ~0.30:1 (gap) |
| 16 | Test reliability | Flaky rate | Repeated runs | 0 flaky | TBD |
| 17 | Duplication | < 3% density | CPD / jscpd | < 3% | TBD |
| 18 | Build time | Clean < 5 min | time | < 5 min | TBD |
| 19 | Include bloat | Transitive < 100 | -H flag | Median < 80 | TBD |
| 20 | Hotspots | Churn x size | git log | Identified + addressed | TBD |
| 21 | Change coupling | Cross-module < 20% | git log | 0 violations | TBD |
| 22 | Bool params | 0 in public API | grep / clang-tidy | 0 | TBD |
| 23 | API surface | Public methods <= 20/class | Script | 0 violations | AppController is candidate |
| 24 | Clang-tidy clean | 0 warnings | clang-tidy | 0 warnings | Partially enforced |

---

## Priority Order

Based on risk, effort, and value. Do the high-impact, low-effort measurements
first to establish a baseline.

### Immediate (this sprint — baseline establishment)

1. **File size census** (1.1) — 5 minutes. Identify the worst offenders.
2. **Complexity baseline** (2.1) — 15 minutes. Run clang-tidy at threshold 15.
3. **RT-safety audit** (1.6) — 10 minutes. Expand existing checks.
4. **Coverage report** (4.1) — 30 minutes. Run the existing coverage preset.
5. **Test-to-code ratio** (1.5) — 2 minutes. Single command.
6. **Build time measurement** (4.5) — 5 minutes. Time the existing build.

### Near-term (next 2-4 sprints — fill critical gaps)

7. **Layer enforcement script** (2.3) — 2-4 hours. Write check-layers.sh.
8. **ASan test run** (4.2) — 1 hour. Add preset + run tests.
9. **UBSan test run** (4.3) — 1 hour. Same pattern.
10. **TSan test run** (4.4) — 1 hour. Critical for audio engine.
11. **Hotspot analysis** (3.1) — 1 hour. Script + manual review.
12. **Duplicate detection** (1.7) — 30 minutes. Install and run CPD.

### Medium-term (1-3 months — improve scores)

13. **Tighten .clang-tidy** (5.1) — ongoing. Ratchet threshold 25 → 15.
14. **Refactor AppController** — multi-sprint. Break God class into components.
15. **Refactor VimEngine** — multi-sprint. Extract command handlers.
16. **Increase test coverage** — ongoing. Target 80% branch for dc:: libraries.
17. **Header self-containment** (2.5) — extend to all headers.
18. **Change coupling analysis** (3.2) — 2 hours. Detect architectural drift.

### Long-term (3-6 months — continuous enforcement)

19. **Scorecard dashboard** (5.5) — 4 hours. Unified benchmark script.
20. **Sanitizer CI presets** (5.4) — 2 hours. Run on every verify.sh.
21. **RTSan adoption** (4.6) — when LLVM 20+ is available.
22. **Mutation testing** — evaluate Mull for dc:: libraries.
23. **Coverage ratchet** (5.3) — increase minimum 2% per quarter.
24. **Bool parameter elimination** — systematic cleanup of public APIs.

---

## Deliverables

Each phase produces concrete artifacts:

| Phase | Artifact | Location |
|-------|----------|----------|
| 1 | Baseline measurements | `docs/baseline-report.md` |
| 2 | Analysis scripts | `scripts/benchmark/*.sh` |
| 3 | Hotspot + coupling report | `docs/behavioral-analysis.md` |
| 4 | Sanitizer results + coverage report | `coverage-report/`, build logs |
| 5 | Enforced checks in verify.sh | `scripts/verify.sh` |
| All | Scorecard (updated per sprint) | `docs/scorecard.md` |

# Code Analysis Criteria Reference

Research synthesis of metrics, thresholds, and practices used by Google, Meta,
Microsoft Research, Stripe, Shopify, Netflix, and the academic/tooling ecosystem.

Organized into 15 dimensions spanning function-level through organizational-level
quality. Each section includes specific metrics with numeric thresholds,
detection strategies, and source attribution.

---

## Table of Contents

1. [Function-Level Metrics](#1-function-level-metrics)
2. [Class-Level Metrics](#2-class-level-metrics)
3. [Module/Package-Level Metrics](#3-modulepackage-level-metrics)
4. [Dependency Graph Analysis](#4-dependency-graph-analysis)
5. [Code Smells Taxonomy](#5-code-smells-taxonomy)
6. [C++ Language Safety](#6-c-language-safety)
7. [Real-Time Audio Thread Safety](#7-real-time-audio-thread-safety)
8. [API Design Quality](#8-api-design-quality)
9. [Testing Quality](#9-testing-quality)
10. [Build System Health](#10-build-system-health)
11. [Behavioral Code Analysis](#11-behavioral-code-analysis)
12. [Technical Debt Quantification](#12-technical-debt-quantification)
13. [ISO 25010 Software Quality Model](#13-iso-25010-software-quality-model)
14. [Google Code Review Dimensions](#14-google-code-review-dimensions)
15. [DORA Delivery Metrics](#15-dora-delivery-metrics)

---

## 1. Function-Level Metrics

| Metric | Warning | Critical | Source |
|--------|---------|----------|--------|
| Cyclomatic complexity | > 10 | > 30 | McCabe / NIST SP 500-235 |
| Cognitive complexity | > 15 | > 25 | SonarSource whitepaper |
| Lines of code | > 40 | > 60 | Google C++ Style Guide / CppDepend |
| Parameters | > 3 | > 5 | Clean Code / CppDepend |
| Local variables | > 8 | > 15 | CppDepend |
| Nesting depth | > 3 | > 5 | Industry consensus |
| Return statements | > 4 | — | CodeClimate |

### Cyclomatic Complexity (McCabe, 1976)

Count of linearly independent paths through a function. For a single function:
count each `if`, `else if`, `while`, `for`, `case`, `catch`, `&&`, `||`, `?:`
and add 1.

| CC | Risk | Testability |
|----|------|-------------|
| 1-10 | Low | Simple, easily testable |
| 11-20 | Moderate | More complex, moderate effort |
| 21-50 | High | Hard to test thoroughly |
| > 50 | Very high | Untestable, extremely error-prone |

### Cognitive Complexity (SonarSource, 2016)

Addresses cyclomatic complexity's blind spots (e.g., a `switch` with 10 cases
scores 10 in CC but is easy to read). Three rules:

1. **+1** for each break in linear flow: `if`, `else if`, `for`, `while`,
   `catch`, `goto`, ternary, sequences of mixed boolean operators
2. **+1 per nesting level** for nested control structures and lambdas
3. **No penalty** for shorthand that improves readability (null-coalescing,
   early returns)

Example:
```
for (...)            // +1 (nesting = 0)
  for (...)          // +2 (1 base + 1 nesting)
    if (...)         // +3 (1 base + 2 nesting)
// Total cognitive complexity: 6
```

---

## 2. Class-Level Metrics

| Metric | Warning | Critical | Source |
|--------|---------|----------|--------|
| WMC (Weighted Methods per Class) | > 30 | >= 47 | Lanza & Marinescu |
| Methods per class | > 20 | > 30 | CppDepend / Rule of 30 |
| Fields per class | > 20 | — | CppDepend |
| Lines per class | > 400 | > 750 | Fitness functions / industry |
| Efferent coupling (Ce) | > 20 | > 50 | CppDepend |
| LCOM4 (cohesion) | >= 2 | — | Class should split into N parts |
| TCC (Tight Class Cohesion) | < 0.5 | < 0.33 | Lanza & Marinescu |
| Depth of inheritance | >= 4 | >= 6 | CppDepend |
| Instance size (C++) | > 64 bytes | — | CppDepend (cache-line) |

### God Class Detection

All three conditions must hold simultaneously (Lanza & Marinescu):

| Metric | Full Name | Threshold |
|--------|-----------|-----------|
| ATFD | Access to Foreign Data | > 5 |
| WMC | Weighted Methods per Class | >= 47 |
| TCC | Tight Class Cohesion | < 0.33 |

### Feature Envy Detection

A method accesses more data from other classes than its own:
- ATFD > few (2-5) AND LAA < 1/3 AND FDP <= few

### Shotgun Surgery Detection

A single conceptual change requires modifying many classes:
- Touching > 5 classes for one logical change = flag
- Measured via commit-level cross-file coupling in version control

### Divergent Change Detection

A single class is modified for many unrelated reasons:
- Modified for > 3 distinct feature areas = candidate

---

## 3. Module/Package-Level Metrics

Robert C. Martin's package coupling metrics:

| Metric | Formula | Ideal | Danger |
|--------|---------|-------|--------|
| Instability (I) | Ce / (Ca + Ce) | 0 or 1 | 0.3-0.7 without matching abstractness |
| Abstractness (A) | Abstract types / Total types | Balanced with I | — |
| Distance from Main Sequence (D) | \|A + I - 1\| | 0 | > 0.3 suspicious, > 0.7 problematic |
| Files per module | — | — | > 50 |
| Dependencies per module | — | — | > 15 |

Where:
- **Ca** (Afferent Coupling) = external classes depending on this module
- **Ce** (Efferent Coupling) = external classes this module depends on

### Zones to Avoid

- **Zone of Pain** (A=0, I=0): Concrete and stable — painful to modify. Example:
  a utility class depended on by everything but containing no abstractions.
- **Zone of Uselessness** (A=1, I=1): Abstract and unstable — unused abstractions
  that nobody depends on.

### Cohesion Metrics

| Metric | Good | Bad |
|--------|------|-----|
| LCOM4 | = 1 (single connected component) | >= 2 (split into N classes) |
| LCOM-HS (Henderson-Sellers) | 0-0.5 | > 1.0 |
| TCC (Tight Class Cohesion) | > 0.5 | < 0.33 |
| Relational Cohesion | 1.5-4.0 | Outside this range |

---

## 4. Dependency Graph Analysis

| Metric | Threshold | Interpretation |
|--------|-----------|----------------|
| Circular dependencies | 0 | Any cycle is a defect |
| NCCD | < 1.0 (well-layered), < 2.0 (acceptable) | > 2.0 = probably cyclic, > 15 = severe |
| CBO (Coupling Between Objects) | 1-4 (good), 5-9 (acceptable) | > 9 = problematic |
| Fan-in (incoming) | High = widely used, should be stable | — |
| Fan-out (outgoing) | Low preferred | High = fragile |

### Dependency Direction Rules

Stable modules (low I, high Ca) should not depend on unstable modules (high I,
low Ca). Dependencies should flow toward stability.

### Layer Violations

Define allowed import paths per module. Example rules:
- `src/engine/` may NOT include from `src/ui/` or `src/graphics/`
- `src/dc/foundation/` may NOT include from `src/dc/model/` or above
- `src/dc/model/` may include from `src/dc/foundation/` only

---

## 5. Code Smells Taxonomy (Fowler & Beck)

### Bloaters (code grown too large)
- **Long Method**: Function > 40 LOC (Google) or > 20 LOC (strict)
- **Large Class**: > 750 LOC or > 30 methods
- **Primitive Obsession**: Using built-in types where domain types would add safety
- **Long Parameter List**: > 3-5 parameters
- **Data Clumps**: Groups of parameters/fields that always appear together

### Object-Orientation Abusers
- **Switch Statements**: Conditional logic better handled via polymorphism
- **Temporary Field**: Fields used only in specific circumstances
- **Refused Bequest**: Subclasses rejecting inherited functionality
- **Alternative Classes with Different Interfaces**: Same concept, different API

### Change Preventers
- **Divergent Change**: One class modified for multiple unrelated reasons
- **Shotgun Surgery**: Single change scattered across many classes
- **Parallel Inheritance Hierarchies**: Adding a subclass requires adding another

### Dispensables
- **Duplicate Code**: >= 100 successive tokens across >= 10 lines (SonarQube)
- **Lazy Class**: Performs minimal function, doesn't justify its existence
- **Data Class**: Only holds data with no behavior
- **Dead Code**: Unreachable code paths, unused declarations
- **Speculative Generality**: Unused "just in case" abstractions

### Couplers
- **Feature Envy**: Method uses another class's data excessively
- **Inappropriate Intimacy**: Classes know internal details of each other
- **Message Chains**: Long chains of `.method().method().method()` calls
- **Middle Man**: Class primarily delegates to others without adding value

---

## 6. C++ Language Safety

### RAII Compliance (C++ Core Guidelines)

| Rule | Requirement |
|------|-------------|
| R.1 | Manage resources automatically using RAII |
| R.5 | Prefer scoped objects; don't heap-allocate unnecessarily |
| R.10 | Avoid `malloc()`/`free()` |
| R.11 | Avoid calling `new` and `delete` explicitly |
| R.12 | Immediately give explicit allocation results to a manager object |
| R.13 | Perform at most one explicit resource allocation per statement |

### Exception Safety Levels

| Level | Guarantee | Required For |
|-------|-----------|-------------|
| Nothrow | Operation will not throw | Destructors, swap, move operations |
| Strong | State unchanged on exception | Transaction-like operations |
| Basic | No resource leaks, objects in valid state | Minimum acceptable |
| None | Anything can happen | Unacceptable in production |

### Const-Correctness

- Variables `const` whenever possible
- `const` member functions for non-mutating methods
- `const` references for unmodified function parameters
- Non-const globals explicitly flagged as violations

### Smart Pointer Rules

- `std::unique_ptr` for exclusive ownership (default choice)
- `std::shared_ptr` only when ownership is genuinely shared
- Raw `T*` means non-owning observer only
- Raw `T&` means non-owning, non-null reference

### Memory Safety Toolchain

| Tool | Category | Detects |
|------|----------|---------|
| Cppcheck | Static | Memory leaks, UB, null deref, buffer overflows |
| Clang-Tidy | Static | Style, bug patterns, modernization; auto-fixable |
| Clang Thread Safety | Static | Mutex annotations, lock ordering (compile-time) |
| ASan | Runtime | Buffer overflow, use-after-free, stack-buffer-overflow |
| TSan | Runtime | Data races, deadlocks |
| UBSan | Runtime | Signed overflow, null deref, misaligned access |
| RTSan (LLVM 20+) | Runtime | Real-time safety violations |

### MISRA C++:2023 — Desktop-Relevant Subset

179 guidelines (4 directives + 175 rules). Most relevant for non-automotive C++:

1. Eliminate undefined behavior — no signed overflow, no dangling pointers, no
   uninitialized reads
2. Rule of Zero — prefer compiler-generated special member functions
3. No implicit narrowing conversions — especially `double` to `float`, `size_t`
   to `int`
4. Exception safety — every function provides at least the basic guarantee;
   destructors must be `noexcept`
5. No unchecked casts — `reinterpret_cast` must be justified
6. Avoid raw `new`/`delete` — use smart pointers and containers

### SEI CERT C++ (143 rules, all HIGH severity)

Key categories: Memory Management (MEM50-57), Concurrency (CON50-56),
Exceptions (ERR50-62), Object-Oriented Programming (OOP50-58).

---

## 7. Real-Time Audio Thread Safety

### The Golden Rule

No operation with unbounded or unpredictable execution time on the audio thread.

### Timing Constraints

| Buffer Size | Sample Rate | Callback Deadline | Usable Budget (70%) |
|-------------|-------------|-------------------|---------------------|
| 64 samples | 44.1 kHz | 1.45 ms | 1.01 ms |
| 128 samples | 44.1 kHz | 2.90 ms | 2.03 ms |
| 256 samples | 48 kHz | 5.33 ms | 3.73 ms |
| 512 samples | 48 kHz | 10.67 ms | 7.47 ms |

Every single buffer must complete within deadline. No exceptions. Zero xruns
under normal operation.

### Forbidden Operations on Audio Thread

| Category | Forbidden Operations |
|----------|---------------------|
| Memory | `malloc`, `free`, `new`, `delete`, `calloc`, `realloc`, any allocator |
| Locking | `pthread_mutex_lock`, `std::mutex::lock`, `try_lock`, `unlock` |
| I/O | `open`, `read`, `write`, `fopen`, `printf`, `NSLog`, GUI API calls |
| System | `mmap`, `syslog`, signal handlers, any blocking OS call |
| Threading | `pthread_create`, `pthread_join`, `std::thread` constructor |
| Exceptions | `throw` (allocates exception object; unwind is unbounded) |
| Virtual memory | Accessing non-resident pages (triggers page fault = disk I/O) |
| Logging | `std::cout`, `std::cerr`, `fprintf(stderr, ...)` |

### Safe Alternatives

| Need | Solution |
|------|----------|
| Numeric values | `std::atomic<T>` (load/store only, no CAS loops) |
| Data streams | Lock-free SPSC FIFO (e.g., `dc::spsc_queue`) |
| Complex state | Immutable snapshots with atomic pointer swap |
| Memory | Pre-allocate all resources before audio processing starts |
| Try-lock pattern | `SpinLock::ScopedTryLockType` — skip work if unavailable |

### RTSan (RealtimeSanitizer — LLVM 20+)

Runtime enforcement of real-time safety:
- Compiler flag: `-fsanitize=realtime`
- `[[clang::nonblocking]]` on audio callback entry points
- `[[clang::blocking]]` on intentionally unsafe functions
- Intercepts: `malloc`, `free`, `pthread_mutex_lock`, and ~50 other functions
- Companion: Clang compile-time Function Effect Analysis

---

## 8. API Design Quality (C++)

| Check | Target | Flag If |
|-------|--------|---------|
| Public methods per class | <= 20 | > 30 |
| Bool parameters in public API | 0 | Any (use `enum class` instead) |
| Raw owning pointers in API | 0 | Any (use smart pointers) |
| Friend functions per class | <= 2 | > 2 (leaky abstraction) |
| Include depth from public header | <= 3 transitive levels | > 5 |
| Parameters per function | <= 4 | > 6 (consider parameter object) |
| Non-template header-only class | Templates/inline only | > 100 lines without .cpp |

### Design Principles (Meyers / Reddy)

- Easy to use correctly, hard to use incorrectly
- Minimal public surface area
- Prefer non-member non-friend functions
- Consistent naming — same concept always uses same verb
- Consistent parameter ordering — output last (or first), subject before modifier
- Type safety — prefer `std::string_view`, `std::span`, `std::optional` over raw
  pointers
- Orthogonality — features independent, combinations produce no surprises

---

## 9. Testing Quality

| Metric | Target | Poor |
|--------|--------|------|
| Branch coverage | > 80% | < 60% |
| Mutation score (critical paths) | > 95% | < 60% |
| Mutation score (general) | > 80% | < 60% |
| Flaky test rate | < 2% | > 5% |
| Per-test flaky rate (1 retry) | < 0.7% | > 5% (quarantine) |
| Unit test suite time | < 10 min | > 30 min |
| Test isolation | 100% order-independent | Any order-dependent = defect |
| Test-to-code ratio (LOC) | 1:1 to 3:1 | < 0.5:1 |
| Mocking ratio | < 30% of tests use mocks | > 70% = testing mocks not code |
| Assertion density | >= 2 per test | 0 (smoke-only) |
| Setup complexity | < 10 lines | > 50 lines |

### Mutation Testing

Inject code mutations (operator replacement, statement deletion, constant
modification) and verify tests catch them. Tools for C++: Mull (LLVM-based).

| Module Criticality | Mutation Score Target |
|--------------------|----------------------|
| Safety-critical (audio engine, data integrity) | >= 95% |
| Core business logic | >= 80% |
| Utility/logging | >= 60-70% |

### Flaky Test Management (Google Data)

16% of Google's tests exhibit flaky behavior. 1.5% of all test runs are flaky.

- No retries: average per-test flakiness must be < 0.005%
- With 1 retry: per-test flakiness can be up to 0.7%
- Tests flaking > 5%: quarantine (disable, document, assign owner)

---

## 10. Build System Health

| Metric | Healthy | Problematic |
|--------|---------|-------------|
| Full clean build | < 5 min (medium project) | > 15 min |
| Incremental rebuild (1 file) | < 10 sec | > 60 sec |
| Preprocessed lines per TU | < 100K | > 500K |
| TU bloat ratio (preprocessed/source) | < 100x | > 200x |
| Header parse count per header | 1-2 times | > 50 times |
| Include depth | <= 5 levels | > 10 levels |
| Transitive includes per TU | < 100 headers | > 450 headers |

### Optimization Techniques and Impact

| Technique | Typical Impact | When to Use |
|-----------|---------------|-------------|
| Forward declarations | 10-30% reduction | Always, especially in headers |
| Fwd.h per directory | 50% reduction (Figma) | Large codebases |
| Precompiled headers | 20-50% reduction | Stable, widely-included headers |
| Unity/jumbo builds | 2-5x faster clean | CI servers |
| include-what-you-use | Variable | CI enforcement |
| C++ modules (C++20) | 5-10x potential | When toolchain matures |

### Build Dependency Analysis

- **Critical path length**: longest sequential compilation chain = minimum build time
- **Parallelism factor**: actual build time / (total CPU time / cores) — closer to
  1.0 = better
- **Circular dependencies**: must be 0 between modules

---

## 11. Behavioral Code Analysis

Based on Adam Tornhill's "Your Code as a Crime Scene" and CodeScene methodology.

### Hotspot Analysis

Hotspots = files with high change frequency AND low code health.

Key finding: **1.2% of a codebase can contain 45% of bugs.** Prioritize
refactoring investment at the intersection of frequent change and poor quality.

### Change Coupling (Temporal Coupling)

- Files changing together > 20% of commits are temporally coupled
- Cross-module temporal coupling = architectural violation
- **Sum of Coupling**: aggregate coupling of a file across all its co-changing partners

### Knowledge Distribution

| Metric | Healthy | At Risk |
|--------|---------|---------|
| Bus factor per module | >= 3 developers | 1 (single point of failure) |
| Knowledge silos | < 20% of codebase | > 50% |
| System mastery | Well-distributed | Concentrated in 1-2 people |

### Code Churn Defect Prediction (Microsoft Research)

Relative code churn measures predict fault-prone code with **89% accuracy**
(Nagappan & Ball, ICSE 2005). Key relative measures:

- Churned LOC / Total LOC
- Files churned / File count
- Churn count / File count

The number of **minor (low-expertise) contributors** to a component has the
highest correlation with defects of any metric Microsoft collects (Bird,
Nagappan et al.).

### Code Age

- Recently maintained code in actively-changing areas = healthy
- Stale code in hotspots = high risk (knowledge has decayed)

---

## 12. Technical Debt Quantification

### SonarQube Ratings

**Technical Debt Ratio** = remediation cost / development cost (at 30 min/LOC)

| Rating | Debt Ratio |
|--------|-----------|
| A | <= 5% |
| B | 6-10% |
| C | 11-20% |
| D | 21-50% |
| E | > 50% |

**Reliability Rating**: A = 0 bugs, B = minor, C = major, D = critical, E = blocker

**Security Rating**: Same scale as reliability, for vulnerabilities.

**Duplication**: >= 100 successive tokens across >= 10 lines flagged as duplicate.

### CodeScene Code Health Score

Scale 1-10, derived from 25+ factors:

| Factor | What It Detects |
|--------|----------------|
| Brain Method | Single function centralizing too much behavior |
| Nested Complexity | Deep nesting increasing defect risk |
| Bumpy Road | Function with multiple distinct logical chunks |
| DRY Violations | Duplicated logic changing together |
| Primitive Obsession | Overuse of built-in types |
| Developer Congestion | Code becoming coordination bottleneck |
| Knowledge Loss | Risk from departing developers |

**Target**: >= 8 for active development, >= 5 for maintenance phase.

### CodeClimate 10-Point Assessment

Organized into four categories:
1. **Size**: File length, method length, argument count, method count
2. **Control Flow**: Return statement count, nested control flow depth
3. **Complexity**: Boolean logic complexity, method cognitive complexity
4. **Duplication**: Identical blocks, similar structural duplicates

---

## 13. ISO/IEC 25010:2023 Software Quality Model

### Product Quality (9 characteristics)

| Characteristic | Sub-characteristics |
|---------------|-------------------|
| **Functional Suitability** | Completeness, Correctness, Appropriateness |
| **Performance Efficiency** | Time Behavior, Resource Utilization, Capacity |
| **Compatibility** | Co-existence, Interoperability |
| **Interaction Capability** | Recognizability, Learnability, Operability, Error Protection |
| **Reliability** | Maturity, Availability, Fault Tolerance, Recoverability |
| **Security** | Confidentiality, Integrity, Authenticity, Resistance |
| **Maintainability** | Modularity, Reusability, Analysability, Modifiability, Testability |
| **Flexibility** | Adaptability, Installability, Replaceability, Scalability |
| **Safety** | Operational Constraint, Risk Identification, Fail Safe |

### Maintainability (most relevant for code analysis)

- **Modularity**: Degree to which components are separable and replaceable
- **Reusability**: Degree to which components can be used in other systems
- **Analysability**: How easily the impact of changes can be assessed
- **Modifiability**: How easily code can be changed without introducing defects
- **Testability**: How easily test criteria can be established and tests executed

---

## 14. Google Code Review Dimensions

Eight dimensions evaluated on every CL:

1. **Design** — Overall architecture sensible? Change belongs in codebase?
   Integrates well with existing system?
2. **Functionality** — Correct behavior? Edge cases handled? No race conditions
   or deadlocks?
3. **Complexity** — Understandable quickly by readers? No over-engineering?
   Solves current problems only?
4. **Tests** — Present, correct, fail when code is broken? Assertions clear?
5. **Naming** — Descriptive yet readable? Length balanced for clarity?
6. **Comments** — Explain "why" not "what"? Clear English?
7. **Style** — Follows style guide? No major style changes mixed with functional?
8. **Documentation** — READMEs, references updated?

Overarching standard: **Approve when the CL definitely improves overall code
health, even if not perfect.**

### Google Readability Certification

Required for at least one reviewer per language per CL. Earned by submitting
code to randomly-assigned mentors until mastery of idioms, patterns, library
ecosystem, and conventions is demonstrated. ~33-50% of Googlers hold readability
in their primary language.

---

## 15. DORA Delivery Metrics

From the DevOps Research and Assessment team (Google Cloud), studying thousands
of organizations over 10+ years:

| Metric | Elite | High | Medium | Low |
|--------|-------|------|--------|-----|
| Deployment frequency | On-demand (multiple/day) | Daily-weekly | Weekly-monthly | Monthly+ |
| Lead time for changes | < 1 hour | < 1 week | 1 week - 1 month | > 1 month |
| Change failure rate | < 5% | < 10% | < 15% | > 64% |
| Recovery time | < 1 hour | < 1 day | 1 day - 1 week | > 1 month |

Key finding: Organizations with high DORA maturity are **2x more likely to
exceed profitability targets**.

Code-level practices that correlate with elite performance:
- **Trunk-based development**: branches last hours, not days
- **Small batch size**: small, frequent integrations
- **Continuous integration**: automated build + test on every commit
- **High-quality documentation**: teams with quality docs achieve higher SLO compliance

---

## Sources

### Engineering Practices
- Google Engineering Practices: https://google.github.io/eng-practices/
- Google Testing Blog (Code Health): https://testing.googleblog.com/2017/04/code-health-googles-internal-code.html
- Google Testing Blog (Flaky Tests): https://testing.googleblog.com/2016/05/flaky-tests-at-google-and-how-we.html
- Google C++ Style Guide: https://google.github.io/styleguide/cppguide.html
- Meta/Infer: https://fbinfer.com/docs/all-issue-types/
- Stripe/Sorbet: https://stripe.com/blog/sorbet-stripes-type-checker-for-ruby
- Shopify/Packwerk: https://shopify.engineering/deconstructing-monolith-designing-software-maximizes-developer-productivity

### Research
- McCabe (1976): Cyclomatic complexity
- NIST SP 500-235: Structured testing methodology
- Nagappan & Ball (ICSE 2005): Relative code churn defect prediction (89% accuracy)
- Bird, Nagappan et al.: Code ownership and software quality
- Nagappan, Murphy, Basili (2008): Organizational metrics predict failures (85%)
- Lanza & Marinescu: God Class / Feature Envy detection thresholds
- SonarSource Cognitive Complexity: https://www.sonarsource.com/docs/CognitiveComplexity.pdf
- DORA / Accelerate: https://dora.dev/

### Standards
- C++ Core Guidelines: https://isocpp.github.io/CppCoreGuidelines/
- SEI CERT C++: https://wiki.sei.cmu.edu/confluence/pages/viewpage.action?pageId=88046682
- MISRA C++:2023: 179 guidelines for C++17
- ISO/IEC 25010:2023: Software quality model
- Robert C. Martin: Package coupling metrics (Instability, Abstractness, D)

### Audio-Specific
- Ross Bencina: http://www.rossbencina.com/code/real-time-audio-programming-101-time-waits-for-nothing
- RTSan: https://clang.llvm.org/docs/RealtimeSanitizer.html
- ADC 2024: LLVM's Real-Time Safety Revolution
- timur.audio: https://timur.audio/using-locks-in-real-time-audio-processing-safely

### Tooling
- CodeScene: https://codescene.com/product/code-health
- CppDepend: https://www.cppdepend.com/documentation/code-metrics
- Mull (mutation testing): https://github.com/mull-project/mull
- Figma build optimization: https://www.figma.com/blog/speeding-up-build-times/
- Tornhill: "Your Code as a Crime Scene" (Pragmatic Programmers)

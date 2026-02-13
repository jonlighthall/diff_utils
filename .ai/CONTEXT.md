# Context

**Purpose:** Facts, decisions, and history for AI agents working on this project.

---

## Topics

- C++ Engine: [cpp_engine/CONTEXT.md](cpp_engine/CONTEXT.md) and [cpp_engine/INSTRUCTIONS.md](cpp_engine/INSTRUCTIONS.md)

## Table of Contents

- [Author](#author)
- [Project Overview](#project-overview)
- [Directory Structure](#directory-structure)
- [Key Components](#key-components)
- [Data Flow](#data-flow)
- [Key Decisions](#key-decisions)
- [Critical Paradigm Question (PENDING)](#critical-paradigm-question-pending)
- [Test Categories](#test-categories)
- [Testing Infrastructure](#testing-infrastructure)
- [Superseded Decisions](#superseded-decisions)
- [Scripting Utilities: Batch Processing Drivers](#scripting-utilities-batch-processing-drivers)

## Author

**Name:** J. Lighthall
**Domain:** Underwater acoustics, numerical comparison, cross-platform validation
**Primary Tools:** C++, Fortran, Make, VS Code

**General preferences:**
- Impersonal voice for expository writing
- Mathematical "we" acceptable in derivations ("we substitute...", "we obtain...")
- Avoid editorial "we" ("we believe...", "we recommend...")
- Prefer minimal changes over extensive rewrites

**Writing style:**
- Target the style appropriate for the project (technical docs, code comments)
- Target audience: Developers working on acoustic propagation model validation
- Level of detail: Appropriate for the audience

**What NOT to do:**
- **No meta-commentary:** Don't create summary markdown files after edits. If context is worth preserving, put it in `.ai/` files.
- **No hyper-literal headers:** If asked to "add a clarifying statement," don't create a section titled "Clarifying Statement." Integrate naturally.
- **No AI self-narration:** Don't describe what you're doing in the document itself. Just do it.

**Productivity guardrail:**
The author has a tendency to fixate on formatting minutiae beyond what's productive. If you notice a pattern of multiple iterations on cosmetic details that don't affect functionality or clarity, it's appropriate to gently note: *"This is getting into diminishing returns—want to call it good enough and move on?"*

---

## Project Overview

### What This Project Does

`tl_diff` solves a specific problem: **comparing numerical results that are visually identical but bitwise different across platforms**.

When two acoustic propagation model implementations produce results that look the same but differ due to:
- Floating-point rounding differences
- Platform/compiler variations
- Precision formatting differences

...traditional tools like `diff` fail completely. `tl_diff` recognizes these formatting artifacts as trivial (informationally equivalent) and filters them out, enabling application of peer-reviewed quantitative comparison methods.

### The Novel Contribution

**Hierarchical Six-Level Discrimination (Core Organizing Principle):**
- Six levels of progressively refined difference classification
- Early-exit architecture: program continues to next level only upon match or critical failure
- Physics-based thresholds from IEEE 754 and peer-reviewed standards
- Not merely a taxonomy, but an operational decision framework

**Sub-LSB Detection (Information-Theoretic):**
- Recognizes printed value as interval, not point
- Enables robust cross-precision comparison
- Example: `30.8` vs `30.85` treated as equivalent when one file has 1 decimal place

**Rigorous Accounting Invariants:**
- Structural and semantic consistency throughout classification
- Ensures reproducibility
- Validated through semantic invariant tests

**Enablement of Fabre's Method:**
- Precision filtering to allow peer-reviewed quantitative comparison on bitwise-different results
- Not a replacement or alternative to Fabre's method—enables it

---

## Directory Structure

```
diff_utils/
├── src/
│   ├── cpp/                          # Modern C++ comparison engine (primary)
│   │   ├── include/                  # Headers
│   │   ├── src/                      # Implementation
│   │   ├── tests/                    # Unit tests
│   │   ├── main/                     # Main executables
│   │   ├── reference/                # Staging: code for future tl_metric/tl_analysis
│   │   └── CHANGELOG.md              # C++ version history (v2.0.0+)
│   ├── fortran/                      # Delivered Fortran programs (v1.0.0, sealed)
│   │   ├── main/                     # tldiff, cpddiff, prsdiff, tsdiff
│   │   ├── modules/                  # Shared modules
│   │   └── CHANGELOG.md              # Fortran version history (v1.0.0)
│   └── java/                         # Java utilities
│
├── build/                            # Compiled artifacts
│   ├── bin/                          # Executables + tests/
│   ├── obj/                          # Object files
│   ├── mod/                          # Fortran modules
│   └── deps/                         # Dependency files
│
├── data/                             # Test data files
│
├── docs/                             # Documentation
│   ├── report/                       # LaTeX paper (UBAND_DIFF_MEMO.tex)
│   ├── guide/                        # Markdown guides
│   └── future-work.md                # Research roadmap
│
├── scripts/                          # Utility scripts
│   └── pi_gen/                       # Pi precision test generators
│
├── makefile                          # Main build
├── README.md                         # Project overview
└── .ai/                              # This folder
```

---

## Key Components
Engine details moved to topic file: see [.ai/cpp_engine/CONTEXT.md](cpp_engine/CONTEXT.md).

### ErrorAccumulationAnalyzer (Staged for tl_analysis)

**Purpose:** Cross-file statistics and exploratory diagnostics

**Status:** ⚠️ **Staged** — Moved to `src/cpp/reference/` (Feb 2026). Not compiled
into tl_diff. Preserved as seed code for future tl_analysis program.

**Files:**
- `src/cpp/reference/error_accumulation_analyzer.h` (227 lines)
- `src/cpp/reference/error_accumulation_analyzer.cpp` (421 lines)
- `src/cpp/reference/tl_metrics.cpp` (342 lines, standalone Fabre M1-M5)

**Responsibilities (future tl_analysis):**
- Accumulate differences across range
- Diagnostic pattern analysis (run tests, autocorrelation, regression)
- Phase error estimation (curve stretching)
- 2% installation verification discriminator

See `docs/future-work.md` for research roadmap.

---

## Data Flow
See engine data flow in topic file: [.ai/cpp_engine/CONTEXT.md](cpp_engine/CONTEXT.md).

---

## Key Decisions
Engine decisions moved to: [.ai/cpp_engine/CONTEXT.md](cpp_engine/CONTEXT.md).

---

## Critical Paradigm Question (RESOLVED)

### The Three Questions

Comparing numerical output from scientific codes raises three distinct questions
that early versions of this tool conflated into a single algorithm:

1. **Installation Verification (Q1):** *Is the difference simply due to trivial
   differences in computing, or is it a failure of the installation?*
   Identical algorithms on different machines should produce nearly identical
   output. Differences arise from compiler flags (fast math, uninitialized
   variables), CPU architectures, language reimplementations, and floating-point
   rounding. The question is whether observed differences are computing noise
   or indicate a real problem.

2. **Tactical Equivalence (Q2):** *Would different battlefield decisions be made
   based on the difference between two curves?* This goes beyond checking if an
   installation is correct. Two curves may differ computationally but be
   operationally indistinguishable for decision-making purposes.

3. **Computational Equivalence (Q3):** *Did these two models produce the same
   output? Are they computationally "close enough"?* Examples in Fabre's paper
   show TL curves with similar gross structure but apparent range-offset
   differences (possible accumulated phase error). This may point to real
   modeling differences but might have less tactical effect.

### Program Architecture (Trio Separation)

| Question | Program | Scope |
|----------|---------|-------|
| Q1 | **tl_diff** | Element-by-element, precision-aware comparison. Evaluates differences based on printed precision, single-precision limits, and operational domain thresholds. Stops there. |
| Q2 | **tl_metric** | Curve-level metrics per Fabre et al. (2009). M1/M2/M3/M_curve/M_total. Nothing beyond the paper. |
| Q3 | **tl_analysis** | Exploratory diagnostics — error accumulation, autocorrelation, regression, phase stretch. All ad hoc attempts at quantifying "close enough." May be further subdivided. |

**Decision:** One repository, three programs. Shared infrastructure (file
parsing, line parsing) supports all three. Each program has its own validation
standard appropriate to the question it answers.

### The 2% Threshold (Removed)

The former `tl_diff` failure threshold (2% of non-marginal, non-critical
significant differences) was an ad hoc value derived from visual inspection of
real output file differences during early development. It has been **removed**.
tl_diff now uses strict pass/fail: any non-marginal, non-critical significant
difference causes failure (exit code 1), aligning with Fortran's `nerr3 > 0`
behavior. Aggregate/curve-level "close enough" assessment belongs in
**tl_metric** and **tl_analysis**.

### TRANSIENT_SPIKES Override (Removed)

The TRANSIENT_SPIKES pass relaxation (allowing PASS up to 10% when transient
spike pattern detected) has been removed from `tl_diff`'s pass/fail logic.
This was experimental code that contaminated the core pass/fail decision with
exploratory diagnostics. Error pattern analysis belongs in `tl_analysis`, not
in the installation verification tool.

### Aggregate Analysis (Moved out of tl_diff)

The following features have been moved out of `tl_diff` to their future homes:
- **RMSE statistics** (RMSEStats, weighted RMSE) → `tl_metric`
- **TL curve metrics** (TLMetrics, M1/M2/M3/M_curve) → `tl_metric`
- **Error accumulation analysis** (ErrorAccumulationAnalyzer) → `tl_analysis`
- **Weighted diff display column** → removed from diff table

The `ErrorAccumulationData` struct and `ErrorAccumulationAnalyzer` class have
been moved to `src/cpp/reference/` and are no longer compiled into or linked by
the `tl_diff` executable. The standalone `tl_metrics.cpp` (Fabre M1-M5) is also
in `src/cpp/reference/`.

**Source:** Sessions 2026-02-12 and 2026-02-13

### Versioning (Decided)

Separate version tracks for each implementation:

| Implementation | Version | Changelog |
|---------------|---------|-----------|
| Fortran tl_diff | v1.0.0 (sealed) | `src/fortran/CHANGELOG.md` |
| C++ tl_diff | v2.0.0 + unreleased | `src/cpp/CHANGELOG.md` |

Root `CHANGELOG.md` is a pointer to both. Fortran v1.0.0 was delivered with
NSPE v6.2 extras. C++ v2.0.0 is a complete rewrite, not a port. Future
tl_metric and tl_analysis will share the C++ version track.

**Source:** Session 2026-02-13

### Critical Threshold Decoupled from error_found (Decided)

`error_found` is now reserved for actual errors (file access failures, parse
errors). Critical threshold exceedances no longer set `error_found`. Critical
diffs still cause failure because they are classified as "significant" at
Level 3, which sets `files_are_close_enough = false`.

The critical threshold has three effects:
1. **Print truncation** — `has_critical_diff` stops table printing (kept)
2. ~~**error_found**~~ — removed (was a redundant parallel failure path)
3. **Counting** — `diff_critical++` increments (kept)

Exit code semantics:
- `error_found = true` → exit 1 with "Error found." (something broke)
- `files_are_close_enough = false` → exit 1 with "Files differ significantly."
- Both false → exit 0

**Source:** Session 2026-02-13

### files_are_close_enough Is the Pass/Fail Mechanism (Decided)

The `files_are_close_enough` flag is NOT deprecated. It is THE proper mechanism
for point-by-point threshold-based pass/fail in tl_diff. It is set to `false`
at Level 3 whenever any significant difference is found.

This embodies the separation of function and evaluation:
- **tl_diff** performs the **function**: point-by-point comparison against a
  user-supplied threshold. `files_are_close_enough` answers "does every point
  pass the threshold?"
- **tl_analysis** (future) performs the **evaluation**: interpreting what the
  pattern of differences *means*.

**Source:** Session 2026-02-13

### Fortran CLI Compatibility (Decided)

The C++ tl_diff accepts the same 3 positional arguments as the Fortran version
(FILE1 FILE2 THRESH). Additional C++ arguments are optional with sensible
defaults. The threshold semantics differ intentionally:

- **Fortran:** effective error = user_thresh + comp_diff (0.05)
- **C++:** threshold applied directly; sub-LSB detection replaces comp_diff

The Fortran `comp_diff` addition was an ad hoc forerunner of the C++ sub-LSB
detection algorithm. The C++ approach is strictly better — it adapts to the
actual precision of each comparison point rather than adding a fixed offset.
Existing Fortran callers' inputs are valid; the C++ behavior is an intentional
improvement, not a compatibility bug.

**Source:** Session 2026-02-13

### Four Use Cases (Decided)

| ID | Use Case | Description | Program |
|----|----------|-------------|---------|
| UC1 | Same model, same input | Installation verification | tl_diff (+ tl_analysis --verify) |
| UC2 | Different models, same input | Model comparison | tl_metric (Fabre) |
| UC3 | Same model, different inputs | Sensitivity analysis | tl_analysis |
| Agnostic | Any two TL curves | Quantitative comparison | tl_metric (Fabre) |

The 2% threshold belongs to UC1 (installation verification) and lives in
tl_analysis, not tl_diff. The Fabre metric is agnostic of use case.

**Source:** Session 2026-02-13

### Restructuring Plan (Planned)

Sequence for extracting the three-program architecture:

1. **Extract shared library** — FileReader, LineParser, FormatTracker,
   threshold constants, calculate_tl_weight → shared linkable code
2. **Scaffold tl_metric** — Fabre M1-M5 only, using shared library and
   `src/cpp/reference/tl_metrics.cpp` as seed
3. **Scaffold tl_analysis** — ErrorAccumulationAnalyzer, 2% threshold
   (--verify mode), RMSE, phase error estimation, using
   `src/cpp/reference/error_accumulation_analyzer.*` as seed

Reference code is staged in `src/cpp/reference/`. No tl_metric or tl_analysis
main programs exist yet.

**Source:** Session 2026-02-13

---

## Test Categories
Moved to topic: [cpp_engine/CONTEXT.md](cpp_engine/CONTEXT.md)

---

## Testing Infrastructure

### Unit Testing Framework: Google Test

**Framework:** Google Test (gtest) — industry standard for C++ unit testing

**Current Status:** ~1,400 lines of unit tests across 7 test files

**Why Google Test:**
- Industry standard — nearly universal in C++ projects
- Well-documented with extensive examples
- Feature-rich: assertions, fixtures, parameterized tests, death tests
- Active development and maintenance by Google
- Cross-platform (Linux, Windows, macOS)
- Good IDE support

**Installation Requirement:**
```bash
sudo apt-get install libgtest-dev
```

**Makefile Behavior (Conditional Compilation):**
- **If gtest available:** Tests are compiled and `make test` runs all tests
- **If gtest NOT available:**
  - `make all` succeeds (builds only non-test targets)
  - `make test` fails with clear error message and installation instructions
  - `make tests` shows warning but doesn't fail the build

**Detection:** The makefile automatically detects gtest availability at build time:
```makefile
GTEST_AVAILABLE := $(shell echo '\#include <gtest/gtest.h>' | $(CXX) -x c++ -E - > /dev/null 2>&1 && echo yes || echo no)
```

**Alternative Frameworks (Not Currently Used):**

1. **Catch2** — Header-only (no installation required), very similar syntax
   - Pro: Can be bundled with project, no system dependency
   - Con: Would require converting ~1,400 lines of existing tests

2. **Bundled Google Test** — Include gtest source in repository
   - Pro: Works everywhere (no sudo required), keeps existing tests
   - Con: Adds ~5MB to repository, needs makefile modifications to build from source

3. **Boost.Test** — Part of Boost library
4. **doctest** — Lightweight, similar to Catch2
5. **Custom framework** — Simple but limited

**Decision Rationale:** Keeping Google Test with conditional compilation provides:
- Industry-standard testing framework
- No code changes required (keep existing 1,400 lines of tests)
- Graceful degradation on systems without gtest
- Clear error messages guide users on how to install

**Future Options:** If cross-platform portability without installation becomes critical, consider bundling gtest source or switching to Catch2 (header-only).

---

## Superseded Decisions

### .ai/ File Structure
**Current:** 3 files (README.md, CONTEXT.md, INSTRUCTIONS.md)
**Previously:** 8 files (00-START-HERE.md, codebase.md, dev-workflow.md, patterns.md, quick-reference.md, instructions.md, IMPLEMENTATION.md, README.md)
**Why changed:** 8-file structure had overlapping content, ambiguous placement rules, and maintenance burden. 3-file structure follows BOOTSTRAP.md standard—clear separation of facts (CONTEXT) vs procedures (INSTRUCTIONS).
**Source:** Session 2026-01-13

### Pi Generator Locations
**Current:** All pi generators consolidated in `scripts/pi_gen/`
**Previously:** Scattered across `src/cpp/main/`, `src/fortran/main/`, `src/java/main/`, `scripts/`
**Why changed:** Consolidation for discoverability; they're test utilities, not core components
**Source:** Session 2026-01-13

### LaTeX Report Structure
**Current:** Single comprehensive `docs/report/UBAND_DIFF_MEMO.tex` (~1140 lines)
**Previously:** Modular structure with 11 separate section files in `docs/report/sections/`
**Why changed:** Merged for self-contained paper suitable for distribution
**Source:** Session 2026-01-13

### Documentation Restructure
**Current:** Three clean, purpose-specific documentation files:
- `docs/DISCRIMINATION_HIERARCHY.md` — Standalone technical reference (no external dependencies)
- `data/README.md` — Test case descriptions for data directory (NEW)
- `data/notes.md` — Development history and algorithmic insights only

**Previously:**
- DISCRIMINATION_HIERARCHY.md referenced notes.md extensively
- notes.md mixed test descriptions, hierarchy definitions, and development notes
- No README in data directory

**Why changed:** Clean separation of concerns:
- DISCRIMINATION_HIERARCHY.md now works as standalone technical reference
- Test case info moved to data/README.md where it belongs
- notes.md cleaned to contain only unique algorithmic/historical content

**Backup files:** `DISCRIMINATION_HIERARCHY_OLD.md`, `notes_old.md` preserved
**Source:** Session 2026-01-13

### Range Data Detection for Column 0
**Current:** Column 0 detected as range data bypasses TL-specific thresholds (110 dB marginal, 138.47 dB ignore)

**Detection criteria:**
1. Monotonically increasing values
2. Fixed delta between consecutive values (±1% tolerance)
3. Starting value < 100 (typical for range in meters/kilometers)
4. Non-zero delta (excludes constant sequences)

**Implementation details:** See engine topic: [cpp_engine/CONTEXT.md](cpp_engine/CONTEXT.md)

**Rationale:** TL thresholds are meaningless for distance/range measurements. Automatically detecting range data prevents spurious threshold application.
**Source:** Session 2026-01-13

### Columns 76–80 Override Mechanism (process_nspe_in_files.sh)

**Current:** process_nspe_in_files.sh (Jan 2026) supports optional override of required-marker checks by examining columns 76–80 of the first line.

**Previously:** Always required one of: tl, rtl, hrfa, hfra, hari in the file's first few lines.

**Why changed:** Some workflows may use column 76–80 as a metadata/label field (e.g., case ID). When blank, the file should be processable without content markers. When non-blank, assume it's metadata and enforce normal marker checks.

**Behavior:**
- Columns 76–80 blank → Skip marker checks, proceed with processing
- Columns 76–80 contain text → Apply normal marker checks (require tl/rtl/etc.)
- Prints message for each file indicating which case applied

**Implementation:** Checks `${first_line:75:5}` (bash substring extraction, 0-based). If empty after trimming whitespace, overrides marker checks. Otherwise applies normal checks.

**Rationale:** Enables flexibility for files using fixed-column metadata while maintaining safety checks for unmarked files. Provides visibility into decision logic via printed messages.

**Source:** Session 2026-01-13

### Maximum Difference Display Enhancement
**Current:** All testing scenarios print maximum difference underlined in purple, followed immediately by maximum percent error

**Locations updated:**
- `print_maximum_difference_analysis()` — raw differences
- `print_rounded_summary()` — rounded differences
- `print_maximum_significant_difference_details()` — significant differences

### Output File Naming Convention (Jan 2026)

**Current:** Pi precision test generators output files with descriptive .txt names:
- Pattern: `<base>-<program>-<order>.txt`
- Example: `pi_output-pi-precision-test-ascending.txt`, `nspe01-pi-precision-test-descending.txt`

**What changed:**
- Old naming (`.asc` / `.desc` suffixes) replaced with descriptive `.txt` filenames
- Generators updated: C++ (`pi_precision_test.cpp`), Python (`pi_precision_test.py`), Fortran (`pi_precision_test.f90`), Java (`PiPrecisionTest.java`)
- All four languages now produce identical naming scheme
- Output .txt files created to preserve legacy .asc/.desc content; old files deleted
- Example docs updated: `example_data/notes.md`, `example_data/plot_bbpp_freq.m`

**Program Name Unification (Hyphenated Wrappers):**
- Added wrapper scripts in `bin/` using hyphenated names: `tl-diff`, `pi-precision-test`, `pi-comparison`
- Wrappers exec the original underscore binaries (non-destructive; originals unchanged)
- Wrappers require `chmod +x` locally (use `bin/tl-diff` or `./bin/pi-precision-test`)

**Remaining Legacy References:**
- `process_nspe_in_files.sh` still references `.asc` in code (lines 448–450, 555–562, 622–625, 682–683)
- `prsdiff.f90` and `tldiff_refactored.f90` hardcode `nspe02.asc`
- These are safe to leave (legacy code for backward compatibility with existing workflows)
- New workflows can emit descriptive .txt names directly from generators

**Rationale:** Descriptive filenames make it obvious which tool generated the file and in what order (ascending vs descending precision). Hyphenated wrappers unify bin naming without destructive renames.

**Source:** Session 2026-01-14
- Special case for trivial-only scenario

**Format:** `Maximum [type] difference: [value]` followed by `Maximum percent error: [percentage]%`
**Source:** Session 2026-01-13

### PercentThresholdTest Fix
**Current:** Test files use two columns (range + data)
**Previously:** Single-column test files (e.g., `write_file(f1, "100.5")`)
**Why changed:** Column 0 is always skipped as range column, so single-column files had no comparison happening. Tests appeared to fail but were actually not executing any comparison logic.
**Solution:** Add range column so data becomes column 1: `write_file(f1, "101.5 100.5")`
**Also fixed:** Missing `debug_level` parameter in FileComparator constructor calls
**Source:** Session 2025-11-20

### Root README Implementation Summary
**Current:** Root `README.md` includes a short "Implementation summary" subsection describing the sub‑LSB fix, FP tolerance, decimal-place limit increase (17), and added tests. It links to `docs/IMPLEMENTATION_SUMMARY.md` for full details.
**Previously:** Detailed implementation notes were only in `docs/IMPLEMENTATION_SUMMARY.md` and `docs/SUB_LSB_DETECTION.md` without a high-visibility summary in the root README.
**Why changed:** Provide user-facing visibility in the root README while keeping long-form technical details in `docs/` to avoid clutter.
**Source:** Session 2025-10-27

---

## Cross-Language Testing (Pi Precision Generators)

### Purpose

**Goal:** Validate that `tl_diff`'s sub-LSB detection works correctly across files generated by different programming languages, which may use different rounding methods when formatting floating-point output.

**Critical for:**
- Cross-platform scientific validation
- Ensuring robustness across different compiler/interpreter implementations
- Confirming sub-LSB detection works regardless of source language

### Rounding Behaviors to Test

Different languages/libraries may implement different rounding strategies when formatting floating-point values:

1. **Round half away from zero** — C++ `std::round`, most common
2. **Banker's rounding (round half to even)** — Python 3 default, IEEE 754 recommendation
3. **Truncation** — Some older systems
4. **Round down (floor)** — Mathematical floor function
5. **Round up (ceiling)** — Mathematical ceiling function

### Test Generator Implementations

**Location:** `scripts/pi_gen/` (consolidated Jan 2026)

**Generated Files:**
- Pattern: `<base>-<program>-<order>.txt`
- Example: `pi_output-pi-precision-test-ascending.txt`
- Content: One number per line, varying precision from 0 to 14 decimal places

| Language | File | Status | Notes |
|----------|------|--------|-------|
| Fortran | `pi_gen_fortran.f90` | ✅ Complete | Uses Machin's formula |
| C++ | `pi_gen_cpp.cpp` | ✅ Complete | Uses `<iomanip>`, `std::fixed` |
| Python | `pi_gen_python.py` | ✅ Complete | f-strings, banker's rounding |
| Java | `pi_gen_java.java` | ✅ Complete | DecimalFormat |

**Batch Runner:** `run_all.sh` - executes all four generators and compares outputs

**Validation Method:**
1. Each language generates ascending + descending files
2. Run `tl_diff` on cross-language comparisons
3. All should report as equivalent (sub-LSB detection should work)
4. Document any discovered rounding behavior differences

**Output Requirements:**
- Files contain line number + value (e.g., `1  3`, `2  3.1`)
- Console output shows metadata (filenames, counts, progress)
- Format compatible with `tl_diff` column parsing

### Unit Test Integration (Added Jan 2026)

**Test File:** `src/cpp/tests/test_cross_language_precision.cpp`

**Tests:**
- `PiCppVsFortranIdentical` — C++ vs Fortran pi output
- `PiCppVsPythonIdentical` — C++ vs Python pi output
- `PiCppVsJavaIdentical` — C++ vs Java pi output
- `PiFortranVsPythonIdentical` — Fortran vs Python pi output
- `AllLanguagesProduceIdenticalPi` — All 6 pairwise comparisons

**Key Assertion:** All four languages produce IDENTICAL pi output. Any difference = 0 (not just "within tolerance").

**Data Location:** `data/pi_<language>_asc.txt` (accessed via `../../data/` from test directory)

**Regeneration:** `make pi_gen_data` to regenerate test files from source generators.

**Historical Context:**
- Original Fortran version used complex Machin's formula implementation
- Simplified approach: `4.0d0 * atan(1.0d0)` for clarity (earlier version)
- Removed headers and index numbers from output at user request (Jan 2026)
- Evolved from `.asc`/`.desc` suffixes to descriptive `.txt` naming (Jan 2026)
- **Status Update (Jan 2026):** All four language implementations exist and are documented in `scripts/pi_gen/README.md`
- **Unit Tests Added (Jan 2026):** 5 new tests validating cross-language equivalence

**Source:** Session 2026-01-14

---

## process_nspe_in_files.sh — NSPE Input Processing Script

### Overview

Bash script for batch processing NSPE input files: copies `.in` files to `nspe.in`, executes the NSPE executable, renames all output files, compares against references, and produces categorized diff results.

**Location:** `/home/jlighthall/utils/diff_utils/process_nspe_in_files.sh` (1242+ lines)

**Modes:**
- `make` — Generate reference outputs (no comparison)
- `copy` — Copy generated outputs to reference directory
- `test` — Execute + compare against references
- `diff` — Compare existing files without execution

### Key Features (Jan 2026)

**1. nspe.in Compatibility**
- Copies input file to `nspe.in` (executables expect this default name)
- Passes `nspe.in` as argument to executable: `(cd "$dir" && "$prog_path" nspe.in)`
- Cleans up `nspe.in` after execution

**2. Comprehensive Output File Renaming**
- Main outputs: `nspe01.asc` → `basename_01.asc`, `nspe_01.asc` → `basename_01.asc`
- Binary files: `nspe09.bin` → `basename_09.bin` (zero-padded)
- Special files: `nspe.prs` → `basename.prs`, `nspe.pulse` → `basename.pulse`
- Fortran unit files: `for[0-9][0-9][0-9].*` → `basename_##.ext`
  - Special case: `for003.dat` → `basename.003` (not `_003.dat`)
  - Leading zero handling: `for042.dat` → `basename_42.dat`, `for009.bin` → `basename_09.bin`
- Extra files: `angles.asc` → `basename_angles.asc`, `ram.in` → `basename_ram.in` (copied, not moved)

**3. Binary File Management**
- Default: Only keeps ASCII files with corresponding `.tl` reference
- Option: `--keep-bin` flag preserves binary files (for042.dat, for003.dat, for009.bin, for045.bin, etc.)
- Detection: Uses `file` command with regex to identify ASCII vs binary
- Cleanup: Lists files being removed, distinguishing binary from other temporary files

**4. Diff Failure Categorization**
- Tracks which diff tool failed for each file (hierarchical: diff → tldiff → tl_diff)
- Files categorized even when passing (identifies "pass with advanced tool" vs "exact match")
- Arrays:
  - `simple_diff_fail_files` — Passed tldiff/tl_diff but failed diff
  - `tldiff_fail_files` — Passed tl_diff but failed tldiff
  - `tl_diff_fail_files` — Failed all diff tools
- **Diff Details section appears FIRST in output** (before pass/fail lists) to emphasize tool hierarchy

**5. Summary Output Structure**
```
Diff Results:
=============
Diff Details:
  [Shows which tool failed for each file]
Passed files: N
Failed files: N
Skipped files: N
[Additional status sections]
```

### Implementation Details

**Arithmetic Handling (set -e safe):**
```bash
count=$((count + 1))  # Safe with set -e
# Avoid: ((count++)) which exits when count=0
```

**Exit Code Capture from Pipes (PIPESTATUS):**
```bash
"$prog_path" nspe.in | tee "$diff_output" > /dev/null
diff_exit=${PIPESTATUS[0]}  # Capture pipe exit code, not tee's
```

**File Type Detection:**
```bash
if file "$file" | grep -qi "ASCII text"; then
    # ASCII file — keep if has reference
else
    # Binary file — remove if --keep-bin not set
fi
```

**Fortran Unit File Extraction:**
```bash
for unit_file in for[0-9][0-9][0-9].*; do
    unit_num="${unit_file:3:3}"  # Extract 003, 042, 009, etc.
    # Special handling for 003
    # Leading zero removal: 042→42, 009→09
done
```

### Empty Files and Missing Outputs

**Empty File Handling:**
- Empty files are NOT reclassified as execution failures
- Tracked separately: `empty_output_files` array
- Counted in output summary for visibility
- Allows execution to succeed even if output is empty

**Missing Output Handling (test mode):**
- Files that produce no output are skipped (not failed)
- Tracked: `missing_exec_output_files` array
- Reported in summary

### Performance Considerations

**Optimizations:**
- Binary file detection only runs if `--keep-bin` not set
- Reference checking only for files with references
- Cleanup skips files that don't exist
- File operations ordered by size (largest first)

### Dependencies

- `lib_diff_utils.sh` — Provides `diff_files()` function (returns 0 if any diff tool succeeds)
- `diff`, `tldiff`, `tl_diff` — Diff tools in hierarchy
- Standard utilities: `find`, `grep`, `cp`, `rm`, `file`, `tee`

### Known Limitations

- `--keep-bin` is a boolean flag (no granular per-file control)
- Reference detection based on file extension (`.tl`, `.rtl`, `.ftl`)
- No support for custom diff tool chains
- Cleanup list printed to stdout (can't be easily captured)

### Testing Locations

Current reference data:
- `v6.1.2` — 8 files fail simple diff but pass with tldiff
- `v6.2-b2` — Additional test suite

### Source

Session 2026-01-14 — Consolidated nspe.in processing, output renaming, binary file management, and diff failure categorization.

---

## Scripting Utilities: Batch Processing Drivers

### Overview

Two batch-processing drivers exist for running executables over collections of input files. They have similar goals but different design philosophies and feature sets.

### `run_pf_ram_batch.sh` (Simpler, Focused)

**Purpose:** Simple batch runner for invoking a single executable (pf_ram) across input files with automatic output renaming/relocation.

**Key features:**
- `--dry-run`, `--force`, `--rebuild`, `--skip-existing`, `--exe`, `--pattern`, `--max-depth`, `--input-dir`, `--debug`
- Derives `exe_tag` from executable basename (e.g., `pf_ram.exe` → `pf_ram`)
- **Exe-tagged output renaming**: renames produced files to `<inputbase>_<exe_tag>_<originalname>` (e.g., `case1_pf_ram_tl.line`)
- Per-run stdout/stderr capture with exe-tag in filename
- Moves renamed outputs into the input file's directory
- Default executable: `PF_RAM_EXE=./bin/pf_ram.exe`
- Expects outputs: `tl.grid`, `tl.line`, `p.vert`

**Use case:** Quick, repeatable batch runs of a single executable; output naming avoids collisions when running multiple executable versions.

### `process_ram_in_files.sh` (Complex, Feature-Rich)

**Purpose:** Advanced driver supporting multiple operational modes (make, copy, test, diff) with rich filtering, skip logic, and reporting.

**Key features:**
- **Four modes**: `make` (execute only), `copy` (copy outputs to reference), `test` (execute + compare), `diff` (compare existing files)
- Multiple `--pattern` and `--exclude` glob filters with ordered application
- `--skip-existing`, `--skip-newer`, `--force` with fine-grained control
- Project root auto-detection (searches for `bin/` directory)
- Ordered input file collection (sorted by size)
- Colored console output with detailed categorized summaries
- Structured logging with support for multiple output file types (`.line`, `.grid`, `.vert`, `.check`)
- Sources `lib_diff_utils.sh` for shared utilities
- Default executable: `PROG=bin/ram1.5.x` (or `RAM_EXE` override)
- Exit codes reflect success/failure state

**Generalization (Session 2026-01-14):**
- Documentation generalized from "NSPE executable" to "executable" for broader applicability
- Function renamed: `detect_nspe_program()` → `detect_program()`
- Executable detection now searches for both `executable.x/exe` and `nspe.x/exe` (backward compatible)
- Environment variable `NSPE_EXE` preserved for backward compatibility (marked as legacy)
- Comments and examples updated to use generic executable terminology
- Makefile target detection updated to search for both "nspe" and "executable" patterns

**Use case:** Comprehensive test workflows (generate outputs → create references → run + compare), with fine-grained filtering and detailed reporting.

### Feature Comparison

| Feature | `run_pf_ram_batch.sh` | `process_ram_in_files.sh` |
|---------|----------------------|--------------------------|
| Modes (make/copy/test/diff) | No (execute only) | Yes |
| Multiple `--pattern` filters | No (single `--pattern`) | Yes (`--pattern` × N) |
| `--exclude` filtering | No | Yes |
| `--max-depth` for recursive depth | Yes | No (uses directory arg only) |
| Exe-tag output renaming | Yes (default) | No (fixed names) |
| Per-run stdout/stderr capture | Yes (with exe-tag) | Yes (via `LOG_FILE`) |
| Skip-newer logic | No | Yes |
| Colored summary output | No | Yes |
| Project root detection | No | Yes |
| `--rebuild` flag | Yes (explicit) | No (tries build if missing) |
| Source library dependencies | No | Yes (`lib_diff_utils.sh`) |
| File ordering | No | Yes (sorted by size) |
| Detailed categorized reporting | No | Yes (exec/diff/copy/file status) |

### Recommendation

**`process_ram_in_files.sh` is the canonical modern driver** for multi-stage test workflows and validation. Use it for:
- Reference generation + testing
- Filtered batch runs with precise control
- Comprehensive reporting and audit trails

**`run_pf_ram_batch.sh` remains useful for:**
- Simple one-off batch execution
- Output renaming that preserves exe-tag (prevents file collision when running multiple executables)
- Quick dry-run previews

### `process_in_files.sh`

**Purpose:** Sibling script to `process_ram_in_files.sh`, designed for a different executable workflow but with similar operational philosophy.

**Key differences from `process_ram_in_files.sh`:**
- Designed for processing `.in` files with different executable (originally NSPE, now generalized)
- Requires files to contain specific strings (`tl`, `rtl`, or `hrfa/hfra/hari`) to be processed
- Handles different output file types (`.tl`, `.rtl`, `.ftl`)
- Different default executable detection (searches for `nspe.x/exe` patterns)

**Generalization (Session 2026-01-14):**
- All documentation references to "NSPE" changed to "executable" or "specified executable"
- Function renamed: `detect_nspe_program()` → `detect_program()`
- Executable detection searches for `executable.x/exe` and `nspe.x/exe` (backward compatible)
- Usage examples updated to use generic executable names
- Environment variable `NSPE_EXE` preserved for backward compatibility (marked as legacy)
- Comments indicate legacy support for `nspe` pattern matching

**Shared features with `process_ram_in_files.sh`:**
- Four modes (make/copy/test/diff)
- `--pattern`, `--exclude`, `--skip-existing`, `--skip-newer`, `--force`, `--dry-run`, `--debug`
- `--exe` option to specify executable path
- Colored output and detailed summaries
- Sources `lib_diff_utils.sh`

**Use case:** Same workflow philosophy as `process_ram_in_files.sh` but for different executable/file types.

---

## Acoustic Propagation Utilities

### earth_acoustic — Spherical Earth Ray Calculator

**Location:** `earth_acoustic.f90`, executable in `bin/earth_acoustic`

**Purpose:** Calculate where an acoustic beam intersects with Earth's surface accounting for spherical curvature. Tests spherical Earth corrections for underwater acoustic propagation models.

**Problem addressed:** Flat-Earth acoustic propagation models assume a beam shot horizontally continues parallel to the surface forever. In reality, Earth's curvature causes the beam to eventually intersect the surface. This tool provides ballpark estimates of where this occurs.

**Key Parameters:**
- Earth radius: 6,371 km (hard-coded)
- Source depth: positive downward (meters)
- Beam angle: from horizontal (0° = horizontal, positive = upward, -90° to +90°)

**Theory:**
- Geometric ray theory (straight-line propagation)
- Law of sines solution for triangle: Earth center → Source → Surface intersection
- Assumes constant sound speed (range/depth-independent)

**Usage:**
```bash
./earth_acoustic [depth_m] [angle_deg]
# Or interactive mode: ./earth_acoustic
```

**Key Findings from Testing:**
- **Horizontal beams** (0°) from 100m depth: ~10,000 km range (1/4 Earth circumference)
- **Small upward angles dramatically reduce range**: 1000m/1° → 9,849 km vs 57.3 km flat-Earth
- **Curvature effects are enormous**: flat-Earth models underestimate by factors of 100-1000×
- **Practical steep angles**: 100m/89° → 111 km vs 0.002 km flat-Earth

**Output:**
- Horizontal range to surface intersection (km and m)
- Central angle subtended by ray path (degrees and radians)
- Context classification (local/coastal/regional/basin/global scale)
- Flat-Earth comparison showing curvature effects

**Limitations:**
- Straight-line propagation (no ray bending due to sound speed gradients)
- Perfect spherical Earth (no topography)
- No absorption or scattering effects

**Documentation:** `EARTH_ACOUSTIC_README.md`, test script: `test_earth_acoustic.sh`

**Related tools:**
- Python version with visualization: `scripts/spherical_earth_acoustic.py` (plots ray paths and generates figures)

**Source:** Created January 2026 for validating spherical Earth corrections in acoustic propagation codes

---

## Visualization Tools

### Python Plotting Scripts

**Location:** `scripts/sphere/`, `scripts/genpi/`

**Key Constraint (Linux/GUI):**
- **Cannot use `os.fork()` to detach matplotlib windows** — GUI toolkits are multi-threaded and fork causes deadlocks/crashes
- **Current approach:** Blocking `plt.show()` — script waits until window is closed before returning to CLI
- **Alternative attempted:** Double-fork daemonization → X connection breaks due to threading issues
- **Workaround if needed:** Use subprocess to launch a separate Python interpreter (not yet implemented)

**Implication:** Python plotting scripts must either block (current) or require more complex subprocess architecture to achieve non-blocking behavior.

---

## Scripting: RAM Processing Pipeline

### Empty Output File Detection

**Added:** January 2026

**Problem:** Execution could return success (exit code 0) but produce an empty output file, leading to false positives in test results.

**Solution implemented in `process_ram_in_files.sh`:**
- Check output files with `[[ ! -s "$file" ]]` after moving from parent directory
- Track empty outputs in `empty_output_files=()` array
- Print warnings with yellow color (`\e[33m`)
- Reclassify execution from success to failure when empty output detected
- Prevent double-counting: Check if file is already in `exec_fail_files` before reclassifying (execution can fail AND produce empty output)
- Report empty output counts in summary for all modes (make, test, copy, diff)
- Exit with code 1 if empty outputs detected

**Why this matters:** An empty output file indicates the program ran but produced no results—a silent failure that should be caught and reported, not counted as success.

### Color Reset Robustness

**Added:** January 2026

**Problem:** `PROG_OUTPUT_COLOR` (light green highlighting for executable output) sometimes wasn't reset after program execution, leaving terminal in colored state.

**Solution:**
- Added `trap 'echo -en "\x1B[0m"' EXIT ERR` before setting color
- Trap ensures color reset even if program is interrupted, fails, or script exits unexpectedly
- Color reset happens immediately after program execution
- Trap is cleared after successful reset with `trap - EXIT ERR`

**Guarantees color reset in all scenarios:** normal completion, program failure, script interruption (Ctrl+C), unexpected errors.

---

*Version history is tracked by git, not by timestamps in this file.*

## 6-Level Hierarchy Implementation (January 2026 Session)

### Current Status: DEBUGGING IN PROGRESS

### Implementation Status: COMPLETE (Verified January 2026)

The 6-level hierarchy implementation is **complete and fully functional**. All 50 unit tests pass.

**6-Level Hierarchy Structure:**
1. **LEVEL 1:** zero vs non-zero (calculated from `elem_number` and `diff_non_zero`)
2. **LEVEL 2:** trivial vs non-trivial (based on format precision `big_zero`)
3. **LEVEL 3:** insignificant vs significant (based on ignore threshold ~138.47)
4. **LEVEL 4:** marginal vs non-marginal (based on marginal threshold 110)
5. **LEVEL 5:** critical vs non-critical (based on critical threshold argument)
6. **LEVEL 6:** error vs non_error (based on user threshold argument)

**Implementation:**
- `CountStats` struct: `diff_error` and `diff_non_error` counters
- `Flags` struct: `has_error_diff` and `has_non_error_diff` flags
- Complete hierarchy logic in `DifferenceAnalyzer::process_hierarchy()` (lines 72-250)
- `SixLevelHierarchyValidation` test validates all 6 levels with mathematical check

**Verification:** 50/50 tests pass including `SixLevelHierarchyValidation` which outputs:
```
Total elements: 16
Zero: 7, Non-zero: 9
Trivial: 0, Non-trivial: 9
Insignificant: 1, Significant: 8
Marginal: 3, Critical: 1
Error: 4, Non-error: 0
Mathematical check: 16 = 7 + 0 + 1 + 3 + 1 + 4 + 0
```


# Context

**Purpose:** Facts, decisions, and history for AI agents working on this project.

---

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

`uband_diff` solves a specific problem: **comparing numerical results that are visually identical but bitwise different across platforms**.

When two acoustic propagation model implementations produce results that look the same but differ due to:
- Floating-point rounding differences
- Platform/compiler variations
- Precision formatting differences

...traditional tools like `diff` fail completely. `uband_diff` recognizes these formatting artifacts as trivial (informationally equivalent) and filters them out, enabling application of peer-reviewed quantitative comparison methods.

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
│   │   └── main/                     # Main executables
│   ├── fortran/                      # Legacy Fortran programs
│   │   ├── programs/                 # cpddiff, prsdiff, tldiff, tsdiff
│   │   └── modules/                  # Shared modules
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

### DifferenceAnalyzer

**Purpose:** Core numeric comparison logic

**Key Responsibilities:**
- Classify differences across 6 levels
- Apply sub-LSB detection with floating-point tolerance
- Enforce immutable Level 2 filtering (trivial differences)
- Maintain semantic invariants (especially zero-threshold behavior)
- Track marginal, critical, error counts

**Critical Fields:**
```cpp
const double FP_TOLERANCE = 1e-12;
const double MARGINAL_TL = 110.0;
const double IGNORE_TL = 138.47;
int max_decimal_places = 17;
```

### FileComparator

**Purpose:** File I/O and validation

**Responsibilities:**
- Check column count consistency
- Detect decimal places per column
- Parse numeric values
- Validate format before processing

**Critical Behavior: Column 0 is Always Skipped**

Column index 0 is unconditionally treated as the **range column** (e.g., distance, time) and is excluded from numerical comparison. Data columns start at index 1.

**Implication for Test Files:**
- Single-column test files will have their only value skipped (column 0)
- Test files must have at least 2 columns: `range_value data_value`
- Example: `"100.5"` → column 0 skipped, no comparison happens
- Correct: `"101.5 100.5"` → column 0 (101.5) skipped, column 1 (100.5) compared

**Constructor Parameters:**
```cpp
FileComparator(
    double user_thresh,           // User significance threshold
    double hard_thresh,           // Critical threshold  
    double table_thresh,          // Print threshold for table output
    int verbosity_level = 0,      // Verbosity (0=quiet, 1=normal, 2=verbose)
    int debug_level = 0,          // Debug level (0=none, 1-3=increasing)
    bool significant_is_percent = false,  // Use percent mode
    double significant_percent = 0.0      // Percent threshold (e.g., 0.01 = 1%)
);
```

**Common Mistake:** Omitting `debug_level` causes `significant_is_percent` and `significant_percent` to be misinterpreted as positional arguments.

### LineParser

**Purpose:** Parse individual lines, track precision

**Responsibilities:**
- Extract numbers from lines
- Track decimal places per value
- Compute shared minimum precision

### ErrorAccumulationAnalyzer (Experimental)

**Purpose:** Cross-file statistics and exploratory diagnostics

**Status:** ⚠️ **Experimental** — Use for investigation only, not for authoritative pass/fail decisions.

**Responsibilities:**
- Accumulate differences across multiple comparisons
- Diagnostic pattern analysis (run tests, autocorrelation, regression)
- Identify transient spikes or systematic biases

See `docs/future-work.md` for research roadmap.

---

## Data Flow

```
Input File
    ↓
FileComparator::validate()
    ├─ Check column count consistency
    ├─ Detect decimal places per column
    └─ Parse numeric values
    ↓
LineParser::analyze_line()
    ├─ Extract numbers
    ├─ Track precision (decimal places)
    └─ Compute shared minimum precision
    ↓
DifferenceAnalyzer::classify_difference()
    ├─ Level 1: Check if raw_diff != 0
    ├─ Level 2: Sub-LSB detection (with FP tolerance)
    ├─ Level 3: Significance threshold
    ├─ Level 4: Marginal band (110 dB TL)
    ├─ Level 5: Critical threshold check
    └─ Level 6: Error classification
    ↓
Output
    ├─ Difference table
    ├─ Summary report
    └─ Consistency checks (invariants)
```

---

## Key Decisions

### Level 2 Filtering is Immutable

Differences classified as trivial at Level 2 are **permanently excluded** from later semantic buckets. This is intentional—formatting artifacts cannot be reintroduced by threshold tuning.

**Rationale:** Prevents the "zero-threshold paradox" where asking for maximum sensitivity could accidentally promote formatting differences to significant.

### FP_TOLERANCE = 1e-12

Chosen to be well below double-precision epsilon (~2.2e-16) while allowing for accumulated floating-point error in calculations.

### Maximum Decimal Places = 17

Increased from 15 to fully support double-precision representation. Beyond 17 digits, all precision is noise.

### Fabre's Method is Authoritative

The IEEE weighted TL difference algorithm from Fabre et al. (2009) is the only peer-reviewed quantitative comparison method. This tool *enables* that method by pre-filtering formatting artifacts.

**Citation:** Fabre & Zingarelli, "A synthesis of transmission loss comparison methods," IEEE OCEANS 2009, doi:10.23919/OCEANS.2009.5422312

---

## Critical Paradigm Question (PENDING)

**Fabre's Method Optimization Paradigm:** Requires careful reading of Fabre et al. to determine:

1. **Tactical Equivalence:** Curves sufficiently similar for operational decision-making
2. **Theoretical/Computational Equivalence:** Curves match at numerical/phase error analysis level

**Observations from Fabre Figures 2–6:** Curves with similar gross structure but apparent range-offset differences (possible accumulated phase error). Whether these score high or low in Fabre's method determines the paradigm.

**Why it matters:** This distinction affects interpretation of all subsequent phase-error and horizontal-stretch comparison work. See `docs/future-work.md` for investigation plan.

---

## Test Categories

### Semantic Invariant Tests (`test_semantic_invariants.cpp`)

**Purpose:** Verify zero-threshold contracts and domain rules

**Key Tests:**
- Zero-threshold mode enables maximum sensitivity
- Trivial filtering is immutable (Level 2 cannot be revisited)
- Ignore TL region filters correctly
- Non-trivial count = significant + insignificant (in normal mode)

### Sub-LSB Boundary Tests (`test_sub_lsb_boundary.cpp`)

**Purpose:** Edge cases in floating-point precision

**Key Tests:**
- `30.8` vs `30.85` classified as TRIVIAL (not SIGNIFICANT)
- Rounding to shared minimum precision works correctly
- FP tolerance handles binary representation edge cases

### Trivial Exclusion Tests (`test_trivial_exclusion.cpp`)

**Purpose:** Ensure Level 2 filtering persists through pipeline

**Key Tests:**
- Trivial differences never counted as significant
- Trivial differences never counted as insignificant
- Total = trivial + nontrivial

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

**Implementation:** `FileReader::is_first_column_fixed_delta()` and `is_first_column_monotonic()` in `src/cpp/src/file_reader.cpp`, wired through `FileComparator` to `DifferenceAnalyzer`

**Rationale:** TL thresholds are meaningless for distance/range measurements. Automatically detecting range data prevents spurious threshold application.
**Source:** Session 2026-01-13

### Maximum Difference Display Enhancement
**Current:** All testing scenarios print maximum difference underlined in purple, followed immediately by maximum percent error

**Locations updated:**
- `print_maximum_difference_analysis()` — raw differences
- `print_rounded_summary()` — rounded differences
- `print_maximum_significant_difference_details()` — significant differences
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

---

*Version history is tracked by git, not by timestamps in this file.*

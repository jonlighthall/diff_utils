# Context — C++ Engine (DifferenceAnalyzer)

Purpose: Focused context for the numeric comparison engine used by tl_diff and related tools. Use this when working on core semantics, precision handling, and classification logic.

---

## Table of Contents

- [Key Components](#key-components)
- [Data Flow](#data-flow)
- [Key Decisions](#key-decisions)
- [Critical Paradigm Question (PENDING)](#critical-paradigm-question-pending)
- [Test Categories](#test-categories)
- [Links](#links)

## Key Components

### DifferenceAnalyzer

Purpose: Core numeric comparison logic

Responsibilities:
- Classify differences across 6 levels
- Apply sub-LSB detection with floating-point tolerance
- Enforce immutable Level 2 filtering (trivial differences)
- Maintain semantic invariants (especially zero-threshold behavior)
- Track marginal, critical, error counts
- Set `files_are_close_enough = false` at Level 3 for any significant difference (THE pass/fail mechanism)
- Set `has_critical_diff = true` at Level 5 (controls print truncation, NOT pass/fail)
- `error_found` is reserved for file access/parse errors only — never set by comparison logic

Critical Fields:
```cpp
const double FP_TOLERANCE = 1e-12;
const double MARGINAL_TL = 110.0;
const double IGNORE_TL = 138.47;
int max_decimal_places = 17;
```

### FileComparator

Purpose: File I/O and validation

Responsibilities:
- Check column count consistency
- Detect decimal places per column
- Parse numeric values
- Validate format before processing
- Error handling for file access failures
- Summary output generation

Critical Behavior: Column 0 is Always Skipped
- Column index 0 is unconditionally treated as the range column (e.g., distance, time) and is excluded from numerical comparison. Data columns start at index 1.

Implications:
- Single-column test files will have their only value skipped (column 0)
- Test files must have at least 2 columns: `range_value data_value`

### LineParser

Purpose: Parse individual lines, track precision

Responsibilities:
- Extract numbers from lines
- Track decimal places per value
- Compute shared minimum precision

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
Differences classified as trivial at Level 2 are permanently excluded from later semantic buckets.

Rationale: Prevents the zero-threshold paradox where formatting artifacts are promoted to significant by threshold tuning.

### FP_TOLERANCE = 1e-12
Well below double-precision epsilon (~2.2e-16) while allowing for accumulated floating-point error.

### Maximum Decimal Places = 17
Supports full double-precision representation. Beyond 17 digits, precision is noise.

### Fabre's Method is Authoritative
Peer-reviewed, IEEE-weighted TL curve comparison is the only authoritative quantitative method. This engine enables Fabre by filtering formatting artifacts.

Citation: Fabre & Zingarelli, "A synthesis of transmission loss comparison methods," IEEE OCEANS 2009, doi:10.23919/OCEANS.2009.5422312

---

## Critical Paradigm Question (RESOLVED)

Resolved in parent context: see [../../CONTEXT.md](../../CONTEXT.md#critical-paradigm-question-resolved).

**Summary:** Three distinct questions identified — installation verification
(Q1, tl_diff), tactical equivalence (Q2, tl_metric), computational equivalence
(Q3, tl_analysis). The paradigm question is answered by separating the programs
rather than trying to optimize one algorithm for all three purposes.

**tl_diff scope:** Element-by-element precision-aware comparison with
physics-based thresholds. Answers Q1 only. No curve-level metrics, no
exploratory diagnostics, no TRANSIENT_SPIKES pattern analysis in pass/fail.

**Source:** Session 2026-02-12

---

## Test Categories

### Output Message Quality Tests

**Purpose:** Ensure summary output messages are accurate and non-misleading

**Key Tests & Fixes (Session 2026-01-14):**

1. **Print Threshold Confusion (Fixed)**
   - **Problem:** When significant differences existed but all were below print threshold, output showed "Files are identical within print threshold" in green, which was misleading
   - **Example:** 1211 significant differences (20.18%) but message said "identical"
   - **Solution:**
     - Removed misleading "identical" message when significant differences exist
     - Added warning: "All significant differences are below the print threshold" (yellow)
     - Only show "identical within print threshold" when truly no significant differences
   - **Location:** `src/file_comparator.cpp`, `print_additional_diff_info()`

2. **File Access Error Handling (Fixed)**
   - **Problem:** When input files couldn't be opened, program still printed:
     - Misleading SETTINGS section (thresholds irrelevant if no comparison happened)
     - Misleading STATISTICS section (0 lines compared)
     - **Dangerous:** "Files are identical" in green when files didn't exist
   - **Solution:**
     - Check `flag.error_found` early in summary output
     - Skip SETTINGS and STATISTICS when file access fails
     - Print clear error message: "Cannot perform comparison due to file access errors" (red)
     - No misleading "identical" claim
   - **Location:** `src/file_comparator.cpp`, `print_summary()` and `print_diff_like_summary()`
   - **Rationale:** Prevents dangerous misinterpretation where missing files appear to pass validation

**Test Status:** Manual testing confirms correct behavior

**Source:** Session 2026-01-14 (output quality improvements)

### Semantic Invariant Tests (test_semantic_invariants.cpp)

Purpose: Verify zero-threshold contracts and domain rules

Key Tests:
- Zero-threshold mode enables maximum sensitivity
- Trivial filtering is immutable (Level 2 cannot be revisited)
- Ignore TL region filters correctly
- Non-trivial count = significant + insignificant (in normal mode)

### Sub-LSB Boundary Tests (test_sub_lsb_boundary.cpp)

Purpose: Edge cases in floating-point precision

Key Tests:
- 30.8 vs 30.85 classified as TRIVIAL (not SIGNIFICANT)
- Rounding to shared minimum precision works correctly
- FP tolerance handles binary representation edge cases

**Historical Context:**
- Original bug: Checked `rounded_diff` instead of `raw_diff` against `big_zero`
- Edge case discovered: `raw_diff = 0.05000000000000071054` vs `big_zero = 0.05000000000000000278`
- Without FP_TOLERANCE: Would fail (raw_diff > big_zero by ~7e-16)
- With FP_TOLERANCE: Correctly classified as equivalent (sub-LSB difference)

**Implementation (src/difference_analyzer.cpp, lines 98-113):**
```cpp
constexpr double FP_TOLERANCE = 1e-12;
bool sub_lsb_diff = (raw_diff < big_zero) ||
                   (std::abs(raw_diff - big_zero) < FP_TOLERANCE * std::max(raw_diff, big_zero));
bool trivial_after_rounding = (rounded_diff == 0.0 || sub_lsb_diff);
```

**Test Suite Status:** 55/55 tests passing (verified 2026-02-13)

**Pi Precision Validation:**
- Automated test script: `scripts/pi_gen/test_pi_precision.sh`
- Tests ascending vs descending π files (all cross-precision comparisons)
- Validates sub-LSB detection across precision boundaries

**Source:** Session 2025-10-27 (bug fix), Session 2026-01-14 (pi validation)

### Trivial Exclusion Tests (test_trivial_exclusion.cpp)

Purpose: Ensure Level 2 filtering persists through pipeline

Key Tests:
- Trivial differences never counted as significant
- Trivial differences never counted as insignificant
- Total = trivial + nontrivial

---

## 6-Level Hierarchy Implementation (COMPLETE)

**Status:** ✅ Fully implemented and validated. All 55 unit tests pass (verified 2026-02-13).

**Implementation Location:** `DifferenceAnalyzer::process_hierarchy()` in `src/cpp/src/difference_analyzer.cpp`

**Test Validation:** `SixLevelHierarchyValidation` test confirms all levels work correctly with mathematical consistency checks.

**Historical Fixes (for reference):**
1. Trivial difference counting — modified to correctly detect sub-LSB differences
2. Critical differences no longer cause premature abort — table truncated but processing continues
3. Summary no longer suppressed when errors found
4. Hierarchy invariant validation added via `print_consistency_checks()`
5. Critical threshold decoupled from `error_found` — sets `has_critical_diff` only (2026-02-13)

**Test Coverage:** 55 tests across 15 test suites including:
- `FileComparatorSummationTest.SixLevelHierarchyValidation` — validates all 6 levels
- `SubLSBBoundaryTest` — 6 tests for sub-LSB detection
- `SemanticInvariants` — 4 tests for zero-threshold contracts
- `PercentThresholdTest` — 3 tests for percent-mode thresholds
- `PiCrossLanguagePrecisionTests` — 5 tests for cross-language π precision validation

## Links
- Project-wide context: ../../CONTEXT.md
- Project-wide instructions: ../../INSTRUCTIONS.md

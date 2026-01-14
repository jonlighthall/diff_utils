# Context — C++ Engine (DifferenceAnalyzer)

Purpose: Focused context for the numeric comparison engine used by uband_diff and related tools. Use this when working on core semantics, precision handling, and classification logic.

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

## Critical Paradigm Question (PENDING)

Fabre's Method Optimization Paradigm: Requires careful reading of Fabre et al. to determine:

1. Tactical Equivalence: Curves sufficiently similar for operational decision-making
2. Theoretical/Computational Equivalence: Curves match at numerical/phase error analysis level

Observations from Fabre Figures 2–6: Curves with similar gross structure but apparent range-offset differences (possible accumulated phase error). Whether these score high or low in Fabre's method determines the paradigm.

Why it matters: This distinction affects interpretation of all subsequent phase-error and horizontal-stretch comparison work. See docs/future-work.md for investigation plan.

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

**Test Suite Status:** 43/43 tests passing (37 original + 6 sub-LSB boundary tests)

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

## Recent Work (Session 2026-01-14: 6-Level Hierarchy Implementation)

### Problem Context
User implementing and validating 6-level difference classification hierarchy:
- Level 0: zero vs non-zero
- Level 1: non-zero = trivial + non_trivial
- Level 2: non_trivial = insignificant + significant
- Level 3: significant = marginal + non_marginal
- Level 4: non_marginal = critical + non_critical
- Level 5: non_critical = error + non_error

### Fixes Completed

**1. Trivial Difference Counting (Fixed)**
- **Problem:** diff_trivial always 0; raw differences that rounded to zero weren't being counted
- **Solution:** Modified process_rounded_values() to compute raw_diff, lsb, big_zero; check if raw_non_zero && trivial_after_rounding → increment diff_trivial
- **Location:** difference_analyzer.cpp
- **Result:** Verified with example data: diff_trivial=9, diff_non_trivial=39, diff_non_zero=48 (identity holds)

**2. Critical Differences Causing Premature Abort (Fixed)**
- **Problem:** Critical differences triggered early return, truncating table and preventing full processing
- **Solution:** Changed process_difference to always return true; added flag.has_critical_diff guard in print_table to suppress printing but continue processing
- **Location:** file_comparator.cpp, difference_analyzer.cpp

**3. Summary Suppressed When Errors Found (Fixed)**
- **Problem:** print_summary had early return when error_found=true, hiding counter consistency checks
- **Solution:** Removed "if (flag.error_found) return;" early exit
- **Location:** file_comparator.cpp

**4. FileReader Debug Print Access (Fixed)**
- **Problem:** FileReader::compare_column_structures couldn't access print.debug2 (incomplete type error)
- **Solution:**
  - Added PrintLevel& parameter to compare_column_structures
  - Forward-declared PrintLevel in file_reader.h
  - Included uband_diff.h in file_reader.cpp for full definition
  - Gated all std::cout on if(print_level.debug2)
  - Added backward-compatible overload creating silent PrintLevel{0,false,false,false,false}
- **Location:** file_reader.h, file_reader.cpp

**5. Hierarchy Invariant Validation (Added)**
- **Solution:** Added print_consistency_checks() function validating all 17 level sum identities
- **Location:** file_comparator.cpp
- **Output:** CONSISTENCY CHECKS section in summary showing all level equations

### Open Issues

**CompareDifferentFilesWithinTolerance Test Failing**
- **Test:** Creates "1.000 2.000" vs "1.001 2.001", expects no significant diff with user_thresh=0.05
- **Symptom:** has_significant_diff=true when expected false
- **Hypothesis:** FormatTracker::calculate_threshold may return dp_threshold (0.001 for 3 decimal places) instead of user_thresh (0.05)
- **Status:** Needs diagnostic logging to verify threshold calculation
- **Location:** test_file_comparator.cpp lines 110-140

### Recommended Additional Tests

User asked "are there any additional unit tests to add for checking the file hierarchy?"

**Priority Tests to Implement:**
1. OnlyTrivialDifferences - verify raw non-zero that rounds to zero counted correctly
2. NonTrivialButInsignificant - both_above_ignore || !exceeds_significance cases
3. NonMarginalNonCriticalSplit_ErrorAndNonError - Level 6 partition
4. CriticalDifferenceCounts - only count if both values <= ignore threshold
5. MixedComprehensiveCase - all levels present in single test
6. Column structure tests - SingleHeaderThenDataGroups, StructureMismatchBetweenFiles
7. Hierarchy invariants in ALL tests - validateCounterInvariants() helper asserting all 17 identities

**Test Suite Helper:**
- Existing: validateCounterInvariants() in FileComparatorSummationTest checks 10 invariants
- Needed: Expand to all 17 level sum identities, apply to all tests

## Links
- Project-wide context: ../../CONTEXT.md
- Project-wide instructions: ../../INSTRUCTIONS.md

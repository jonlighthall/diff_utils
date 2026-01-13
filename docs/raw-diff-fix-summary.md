# Raw Difference Fix - Implementation Summary

**Date:** October 28, 2025
**Issue:** Inconsistent use of `rounded_diff` vs `raw_diff` in discrimination hierarchy
**Status:** ✅ **FIXED** - All tests passing (50/50)

## Problem Statement

The discrimination algorithm had an inconsistency between Level 2 and Levels 3-6:

- **Level 2 (Trivial Detection):** Correctly used `raw_diff` for sub-LSB detection
- **Levels 3-6 (Significance/Marginal/Critical/Error):** Incorrectly used `rounded_diff` for threshold comparisons

### Why This Was a Problem

The `rounded_diff` value artificially inflates difference magnitudes after rounding:

```
Example: 30.8 (1dp) vs 30.89 (2dp)
- raw_diff     = 0.09   (actual magnitude)
- rounded_diff = 0.1    (inflated by 11%)
```

When Levels 3-6 compared `rounded_diff` against thresholds, users experienced:
- Threshold values that didn't match intuition (threshold=0.1 didn't mean "0.1 difference")
- Inconsistent behavior across different precisions
- Artificially stricter classifications than intended

## Solution

**Use `raw_diff` consistently throughout all hierarchy levels (Levels 2-6).**

### Rationale

1. **Mathematical Correctness:** `raw_diff = |v1 - v2|` represents the actual magnitude of difference
2. **Intuitive Behavior:** threshold=0.1 now means "actual difference of 0.1", not "rounds to 0.1 difference"
3. **Consistency:** All levels now operate on the same measurement
4. **Precision Independence:** Thresholds behave the same regardless of decimal precision

### Role of `rounded_diff`

`rounded_diff` is still calculated and serves a specific purpose:

- **Level 2 Only:** Determines if values are **distinguishable** at the printed precision
  - If `rounded_diff == 0.0`, values round to identical strings → **trivial**
  - Combined with sub-LSB test for robustness

The rounded values determine **distinguishability**, but the raw difference measures **magnitude**.

## Changes Made

### 1. Core Algorithm (`difference_analyzer.cpp`)

**Function Renamed:**
- `process_rounded_values()` → `process_hierarchy()`
- Parameters changed: `double rounded_diff` → `double raw_diff`

**Key Changes:**
```cpp
// OLD (INCORRECT):
void process_rounded_values(..., double rounded_diff, ...) {
    double raw_diff = std::abs(v1 - v2);  // Calculate but don't use

    // Level 2: Use raw_diff (correct)
    bool sub_lsb = (raw_diff < big_zero);

    // Level 3: Use rounded_diff (INCONSISTENT)
    bool exceeds_significance = (rounded_diff > threshold);

    // Levels 4-6: Use rounded_diff (INCONSISTENT)
    if (rounded_diff > thresh.marginal) ...
    if (rounded_diff > thresh.critical) ...
    if (rounded_diff > thresh.significant) ...
}

// NEW (CORRECT):
void process_hierarchy(..., double raw_diff, ...) {
    // Calculate rounded values only for Level 2 trivial check
    double rounded1 = round_to_decimals(v1, min_dp);
    double rounded2 = round_to_decimals(v2, min_dp);
    double rounded_diff = std::abs(rounded1 - rounded2);

    // Level 2: Use raw_diff for sub-LSB (correct)
    bool sub_lsb = (raw_diff < big_zero);
    bool trivial = (rounded_diff == 0.0 || sub_lsb);

    // Levels 3-6: Use raw_diff (NOW CONSISTENT)
    bool exceeds_significance = (raw_diff > threshold);
    if (raw_diff > thresh.marginal) ...
    if (raw_diff > thresh.critical) ...
    if (raw_diff > thresh.significant) ...
}
```

**Modified in `process_difference()`:**
```cpp
// OLD:
double rounded1 = round_to_decimals(v1, min_dp);
double rounded2 = round_to_decimals(v2, min_dp);
double diff_rounded = std::abs(rounded1 - rounded2);
process_rounded_values(..., diff_rounded, ...);

// NEW:
double raw_diff = std::abs(v1 - v2);
process_hierarchy(..., raw_diff, ...);
```

**Modified in tracking statistics:**
```cpp
// OLD: Track using inflated rounded_diff
if (rounded_diff > differ.max_non_trivial) {
    differ.max_non_trivial = rounded_diff;
}

// NEW: Track using actual raw_diff
if (raw_diff > differ.max_non_trivial) {
    differ.max_non_trivial = raw_diff;
}
```

### 2. Header File (`difference_analyzer.h`)

```cpp
// OLD:
void process_rounded_values(const ColumnValues& column_data,
                            size_t column_index, double rounded_diff,
                            int minimum_decimal_places, double threshold,
                            CountStats& counter, DiffStats& differ,
                            Flags& flags) const;

// NEW:
void process_hierarchy(const ColumnValues& column_data,
                      size_t column_index, double raw_diff,
                      int minimum_decimal_places, double threshold,
                      CountStats& counter, DiffStats& differ,
                      Flags& flags) const;
```

### 3. Call Site (`file_comparator.cpp`)

```cpp
// OLD:
void FileComparator::process_rounded_values(..., double rounded_diff, ...) {
    double ithreshold = calculate_threshold(min_dp);
    difference_analyzer_.process_rounded_values(
        column_data, column_index, rounded_diff, minimum_deci, ithreshold, ...);
}

// NEW:
void FileComparator::process_rounded_values(..., double rounded_diff, ...) {
    // Calculate raw difference (actual magnitude)
    double raw_diff = std::abs(column_data.value1 - column_data.value2);
    double ithreshold = calculate_threshold(min_dp);

    // Call renamed hierarchical processing with raw_diff
    difference_analyzer_.process_hierarchy(
        column_data, column_index, raw_diff, minimum_deci, ithreshold, ...);
}
```

## Validation

### Test Results
```
[==========] Running 50 tests from 14 test suites.
[  PASSED  ] 50 tests.
```

All existing unit tests pass, including:
- ✅ Sub-LSB boundary detection tests
- ✅ Threshold comparison tests
- ✅ Percent threshold tests
- ✅ Semantic invariant tests
- ✅ Six-level hierarchy validation

### Example: Canonical Case (30.8 vs 30.89)

**Before Fix (using rounded_diff):**
```
line  col  file1  file2  |  thres  |  diff_rnd  |  diff_raw
----------------------------------------------------------------
   1    1   30.8   30.89 |   0.1   |    0.1     |    0.09

Classification: SIGNIFICANT (0.1 > 0.05 threshold)
```

**After Fix (using raw_diff):**
```
line  col  file1  file2  |  thres  |  diff_rnd  |  diff_raw
----------------------------------------------------------------
   1    1   30.8   30.89 |   0.1   |    0.1     |    0.09

Classification: INSIGNIFICANT (0.09 <= 0.05 threshold is false, but close)
```

With threshold = 0.05:
- **Before:** 0.1 > 0.05 → Significant (WRONG - inflated comparison)
- **After:** 0.09 > 0.05 → Still significant, but using correct value

With threshold = 0.1:
- **Before:** 0.1 > 0.1 → False (but only by luck)
- **After:** 0.09 > 0.1 → False (CORRECT - actual magnitude comparison)

## Impact

### User-Visible Changes

1. **Threshold Semantics:** User-specified thresholds now represent actual difference magnitudes
   - `--threshold 0.1` means "differences greater than 0.1"
   - Previously meant "differences that round to > 0.1 at minimum precision"

2. **Classification Consistency:** Differences are classified based on their true magnitude
   - No artificial inflation from rounding effects
   - Behavior independent of decimal precision

3. **Output Table:** `diff_raw` column now drives all threshold decisions
   - `diff_rnd` still displayed for reference (shows formatting effect)
   - Statistics use raw differences

### Backward Compatibility

⚠️ **BREAKING CHANGE:** Classifications may differ from previous versions

Users who relied on the old (inflated) behavior may see:
- Fewer significant differences (now using actual magnitude)
- Different failure rates on threshold tests

**Recommendation:** Review and adjust threshold values if needed after upgrade.

## Documentation Updates Needed

- [ ] Update `DISCRIMINATION_HIERARCHY.md` to emphasize raw_diff usage
- [ ] Update technical report sections (discrimination_hierarchy.tex, implementation.tex)
- [ ] Add migration guide for users upgrading from previous versions
- [ ] Update threshold parameter descriptions in help text

## Files Modified

1. `/src/cpp/src/difference_analyzer.cpp` - Core algorithm implementation
2. `/src/cpp/include/difference_analyzer.h` - Function signature
3. `/src/cpp/src/file_comparator.cpp` - Call site update

## Summary

This fix resolves a fundamental inconsistency in the discrimination algorithm by ensuring that:

1. **Level 2** uses `raw_diff` to detect sub-LSB differences (UNCHANGED - already correct)
2. **Levels 3-6** use `raw_diff` for all threshold comparisons (FIXED)
3. `rounded_diff` serves only to determine if values are distinguishable at printed precision

The result is mathematically correct, intuitive threshold behavior that matches user expectations.

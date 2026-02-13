# Discrimination Hierarchy: Algorithm Design and Implementation

## Overview

This document describes the six-level hierarchical discrimination algorithm used for numerical file comparison in `tl_diff`. **This hierarchy is the author's novel organizing principle** for progressive refinement pipeline that classifies numeric differences into hierarchical categories, enabling domain-specific and precision-aware comparison.

The program implements **early-exit logic**: it continues through subsequent levels only when a match is found or critical failure occurs, not as a mere taxonomy but as an operational decision-making framework.

## Design Philosophy

The comparison algorithm is designed around three core principles:

1. **Progressive Refinement with Early Exit**: Each level partitions one set into exactly two subsets. Upon match or critical failure, the program halts; otherwise, the unmatched subset undergoes further subdivision at the next level. This is the organizing principle.
2. **Precision Awareness**: Differences are evaluated in the context of printed precision, machine epsilon, and floating-point representation
3. **Domain Specificity**: Special handling for transmission loss (TL) data, including physics-based thresholds derived from acoustic pressure limits and single-precision arithmetic constraints

The algorithm implements a six-level hierarchy where each level applies specific numerical criteria to progressively categorize differences from raw comparison through domain-specific significance assessment.

## Implementation Architecture

**Key source files:**
- `src/cpp/include/tl_diff.h` — Threshold definitions, data structures (`Thresholds`, `CountStats`, `DiffStats`, `Flags`)
- `src/cpp/include/difference_analyzer.h` — Core analyzer interface
- `src/cpp/src/difference_analyzer.cpp` — Discrimination logic implementation (early-exit control flow)
- `src/cpp/src/file_comparator.cpp` — File parsing, orchestration, summary generation
- `src/cpp/main/tl_diff.cpp` — Command-line interface

**Runtime data structures:**
- **`ColumnValues`**: Per-element structure containing `value1`, `value2`, `min_dp` (minimum decimal places), `max_dp`, range information
- **`Thresholds`**: Configuration including `zero`, `significant`, `critical`, `marginal`, `ignore`, plus percent-mode fields
- **`CountStats`**: Counters for each discrimination level
- **`DiffStats`**: Maximum differences and precision tracking
- **`Flags`**: Boolean state tracking for comparison outcomes

## Mathematical Foundation

### Machine Precision Constants

```cpp
SINGLE_PRECISION_EPSILON = 2^-23 ≈ 1.19e-07
```

This value represents the smallest resolvable difference in single-precision floating-point arithmetic and forms the basis for domain-specific thresholds.

### Domain-Specific Thresholds

**Ignore Threshold (TL Domain)**:
```cpp
thresh.ignore = -20 × log₁₀(SINGLE_PRECISION_EPSILON) ≈ 138.47 dB
```

This represents the transmission loss value corresponding to the smallest representable pressure in single-precision arithmetic. Values above this threshold are numerically unreliable.

**Marginal Threshold (TL Domain)**:
```cpp
thresh.marginal = 110 dB
```

Based on operational research (https://doi.org/10.23919/OCEANS.2009.5422312), transmission loss values above 110 dB are weighted to zero in acoustic propagation analysis. This threshold delineates the boundary of operational significance.

### Sub-LSB Detection

The algorithm uses half-ULP (Unit in the Last Place) criterion for detecting trivial differences:

```cpp
LSB = 10^(-minimum_decimal_places)
big_zero = LSB / 2.0
trivial = (raw_diff < big_zero) OR (|raw_diff - big_zero| < FP_TOLERANCE × max(raw_diff, big_zero))
```

Where `FP_TOLERANCE = 1e-12` provides floating-point comparison robustness.

## Six-Level Discrimination Hierarchy

Each level implements a binary partition, with one subset undergoing further subdivision.

---

### LEVEL 0: Structure Validation

**Purpose**: Verify file structure compatibility and count total elements

**Partition**: `total_elements = countable_elements + structural_mismatches`

**Decision Rule**:
```
total_elements = Σ(lines_per_group × columns_per_group)
```

**Implementation**: `FileComparator::compare_files()` in `src/cpp/src/file_comparator.cpp`

**Behavior**: If element counts differ between files, comparison continues up to the structural divergence point but is marked as failed. Functions involved: `parse_line()`, `process_line()`, `extract_column_values()`.

---

### LEVEL 1: Raw Difference Detection

**Purpose**: Distinguish identical elements from those with any measurable difference

**Partition**: `total = zero_diff + non_zero_diff`

**Decision Rule**:
```cpp
raw_diff = |value1 - value2|
non_zero = (raw_diff > thresh.zero)  // thresh.zero = SINGLE_PRECISION_EPSILON
```

**Implementation**: `DifferenceAnalyzer::process_raw_values()` in `src/cpp/src/difference_analyzer.cpp`

**Counters**: `counter.diff_non_zero`, `differ.max_non_zero`

**Significance**: If `non_zero == 0`, files are bitwise equivalent (within epsilon). This provides similar functionality to `diff` but with epsilon tolerance for floating-point comparison.

---

### LEVEL 2: Precision-Based Trivial Detection

**Purpose**: Separate format-driven differences from substantive numerical differences

**Partition**: `non_zero = trivial + non_trivial`

**Decision Rule**:
```cpp
LSB = 10^(-min_dp)  // Least Significant Bit based on format
big_zero = LSB / 2.0  // Half-ULP criterion
rounded_diff = round_to_decimals(raw_diff, min_dp)

trivial = (rounded_diff == 0.0) OR
          (raw_diff < big_zero) OR
          (|raw_diff - big_zero| < FP_TOLERANCE × max(raw_diff, big_zero))
```

**Implementation**: `DifferenceAnalyzer::process_hierarchy()` in `src/cpp/src/difference_analyzer.cpp`

**Counters**: `counter.diff_trivial`, `counter.diff_non_trivial`, `differ.max_non_trivial`

**Robustness**: FP_TOLERANCE (1e-12) handles floating-point representation errors. Example: comparing 30.8 (1dp) vs 30.85 (2dp) yields raw_diff = 0.05, which equals big_zero = 0.05, triggering the tolerance check for consistent classification.

**Percent Error Tracking**: For non-trivial differences:
```cpp
percent_error = 100 × raw_diff / |value2|  // value2 is reference
// If |value2| ≤ thresh.zero, percent_error = ∞ (sentinel)
```

---

### LEVEL 3: Subnormal vs Normal (Machine Precision Limit)

**Purpose**: Among non-trivial differences, separate those that are numerically reliable from those in the subnormal range where values are dominated by machine precision limitations

**Partition**: `non_trivial = subnormal + normal`

**Physical Context**: In transmission loss (TL) calculations, TL = -20×log₁₀(pressure). When TL > 138.47 dB, the corresponding pressure is less than single-precision epsilon (ε_single ≈ 1.19×10⁻⁷). These values are in the subnormal/denormal range and are numerically unreliable.

**Decision Rule**:
```cpp
// Check if both values exceed ignore threshold (subnormal boundary)
both_above_ignore = (value1 > thresh.ignore) AND (value2 > thresh.ignore)

// For range data detection: skip TL thresholds for column 0 if monotonic with fixed delta
skip_tl_check = (column_index == 0) AND (flags.column1_is_range_data)
if (skip_tl_check) {
    both_above_ignore = false
}

if (both_above_ignore) {
    subnormal  // Both values > 138.47 dB: pressures < ε_single, unreliable
} else {
    // Apply user significance threshold to normal-range differences
    if (thresh.significant_is_percent) {
        // Percent mode: compare fractional difference
        ref = |value2|
        if (ref <= thresh.zero) {
            exceeds_user_threshold = (raw_diff > thresh.zero)  // Conservative
        } else {
            exceeds_user_threshold = (raw_diff / ref) > thresh.significant_percent
        }
    } else if (thresh.significant == 0.0) {
        // Sensitive mode: all non-trivial below ignore are normal
        exceeds_user_threshold = true
    } else {
        // Standard mode: absolute threshold
        exceeds_user_threshold = (raw_diff > thresh.significant)
    }

    if (NOT both_above_ignore AND exceeds_user_threshold) {
        normal  // At least one value ≤ 138.47 dB: numerically reliable
    } else {
        subnormal  // Does not exceed user threshold or both values subnormal
    }
}
```

**Implementation**: `DifferenceAnalyzer::process_hierarchy()` in `src/cpp/src/difference_analyzer.cpp`

**Counters**: `counter.diff_insignificant` (subnormal), `counter.diff_significant` (normal), `counter.diff_high_ignore`

**Terminology Note**: Code variables retain historical names (`diff_insignificant`, `diff_significant`) but output now uses clearer subnormal/normal terminology to reflect physical meaning.

**Special Cases**:
- **Percent Mode**: Enabled when `thresh.significant < 0` (interpreted as percentage). Uses value2 as reference.
- **Sensitive Mode**: When `thresh.significant == 0.0`, all non-trivial differences below ignore threshold (138.47 dB) are normal.
- **Range Data**: Column 0 data detected as range (monotonic + fixed delta + starting value < 100) bypasses TL-specific thresholds.

---

### LEVEL 4: Zero-Weighted vs Non-Zero-Weighted (Operational Significance)

**Purpose**: Within normal differences, distinguish between those in the operationally weighted-to-zero range and those in the non-zero-weighted operational range

**Partition**: `normal = zero_weighted + non_zero_weighted`

**Physical Context**: Research (https://doi.org/10.23919/OCEANS.2009.5422312) shows transmission loss values above 110 dB are weighted to zero in acoustic propagation phase-space analysis. While numerically valid (unlike subnormal), these differences have no operational significance.

**Decision Rule**:
```cpp
// Skip TL threshold check for range data in column 0
skip_tl_check = (column_index == 0) AND (flags.column1_is_range_data)

if (NOT skip_tl_check AND
    value1 > thresh.marginal AND value1 < thresh.ignore AND
    value2 > thresh.marginal AND value2 < thresh.ignore) {
    zero_weighted  // Both values in range (110, 138.47] dB: weighted to zero operationally
} else {
    non_zero_weighted  // At least one value ≤ 110 dB: operationally significant
}
```

**Implementation**: `DifferenceAnalyzer::process_hierarchy()` in `src/cpp/src/difference_analyzer.cpp`

**Counters**: `counter.diff_marginal` (zero-weighted)

**Terminology Note**: Code variable `diff_marginal` represents zero-weighted differences. Output uses zero-weighted/non-zero-weighted terminology to clarify the operational weighting.

**Domain Context**: For transmission loss data, values between 110 and 138.47 dB represent the "operationally negligible zone"—numerically valid but outside the range of operational interest due to phase-space weighting.

---

### LEVEL 5: Critical vs Non-Critical (Model Failure Detection)

**Purpose**: Among non-zero-weighted differences, detect catastrophically large differences that may indicate model failure or discontinuous behavior

**Partition**: `non_zero_weighted = critical + non_critical`

**Decision Rule**:
```cpp
// Skip TL threshold check for range data in column 0
skip_tl_check = (column_index == 0) AND (flags.column1_is_range_data)

if (NOT skip_tl_check AND
    raw_diff > thresh.critical AND
    value1 <= thresh.ignore AND value2 <= thresh.ignore) {
    critical  // Large difference in numerically valid range
    flags.has_critical_diff = true
    // NOTE: error_found is NOT set — critical threshold controls
    // print truncation only. Pass/fail comes from Level 3.
} else {
    non_critical
}
```

**Implementation**:
- Early check: `DifferenceAnalyzer::process_difference()` (top-level)
- Per-element: `DifferenceAnalyzer::process_hierarchy()`

**Counters**: `counter.diff_critical`

**Behavior**: Upon detecting the first critical difference, `print_hard_threshold_error()` is called to report it. The `has_critical_diff` flag truncates further table printing. Processing continues to maintain consistent statistics. Critical differences are already classified as "significant" at Level 3, which sets `files_are_close_enough = false` and ensures non-zero exit code through the normal pass/fail path.

---

### LEVEL 6: Pass/Fail Assessment (Strict Mode)

**Purpose**: Evaluate overall file comparison success based on whether any non-marginal, non-critical significant differences exist

**Decision Rule**:
```cpp
// Count non-marginal, non-critical, significant differences
non_marginal_non_critical = counter.diff_significant -
                             counter.diff_marginal -
                             counter.diff_critical

if (non_marginal_non_critical > 0) {
    FAIL  // Any significant operational difference = failure
} else {
    PASS  // All differences are marginal, critical (handled), or absent
}
```

**Implementation**: `FileComparator::print_significant_percentage()` in `src/cpp/src/file_comparator.cpp`

**Rationale**: This strict assessment aligns with Fortran's `nerr3 > 0`
behavior. Any non-marginal significant difference means the files are
not point-by-point equivalent. Aggregate/curve-level evaluation of
"close enough" belongs in `tl_metric` and `tl_analysis`.

> **Historical note.** Previous versions used an ad hoc 2% tolerance
> rule that permitted a small proportion of significant differences.
> This was removed to maintain clean separation between point-by-point
> comparison (tl_diff) and aggregate analysis (tl_metric/tl_analysis).

---

## Hierarchical Relationships

At each level, the following invariants hold:

```
LEVEL 0: total = Σ(lines × columns)

LEVEL 1: total = zero + non_zero

LEVEL 2: non_zero = trivial + non_trivial
         total = zero + trivial + non_trivial

LEVEL 3: non_trivial = subnormal + normal
         non_zero = trivial + subnormal + normal
         total = zero + trivial + subnormal + normal

LEVEL 4: normal = zero_weighted + non_zero_weighted
         non_trivial = subnormal + zero_weighted + non_zero_weighted
         non_zero = trivial + subnormal + zero_weighted + non_zero_weighted
         total = zero + trivial + subnormal + zero_weighted + non_zero_weighted

LEVEL 5: non_zero_weighted = critical + non_critical
         normal = zero_weighted + critical + non_critical
         non_trivial = subnormal + zero_weighted + critical + non_critical
         non_zero = trivial + subnormal + zero_weighted + critical + non_critical
         total = zero + trivial + subnormal + zero_weighted + critical + non_critical

LEVEL 6: Assessment only (no binary partition)
         Evaluates: non_zero_weighted_non_critical_count / total < 0.02
         Where: non_zero_weighted_non_critical = normal - zero_weighted - critical
```
         non_zero = trivial + insignificant + marginal + critical + error + non_error
         total = zero + trivial + insignificant + marginal + critical + error + non_error
```

These invariants are verified in unit tests to ensure accounting consistency.

---

## Special Features and Edge Cases

### Percent-Mode Threshold

When the user specifies a negative significant threshold (e.g., `-10` for 10%), the system enters percent mode:

```cpp
thresh.significant_is_percent = true
thresh.significant_percent = |specified_value| / 100.0  // e.g., 0.10 for 10%
```

**Reference Choice**: Uses `value2` (second file) as reference. This is asymmetric but consistent.

**Near-Zero Reference Handling**: When `|value2| <= thresh.zero`, the percent comparison is undefined. The implementation conservatively treats any non-trivial difference as exceeding the percent threshold and displays `INF` as a sentinel value for the percentage.

**Alternative Approaches** (not currently implemented):
- Symmetric relative difference: `|v1 - v2| / max(|v1|, |v2|)`
- Combined absolute + relative: significant when `abs_diff > abs_tol OR rel_diff > rel_tol`
- ULP-based: `meaningful_threshold = max(|v1|, |v2|) × machine_epsilon × safety_factor`

### Range Data Detection

For column 0 (typically range/distance), the algorithm detects when data represents range information:

**Criteria**:
1. Monotonically increasing values
2. Fixed delta between consecutive values (±1% tolerance)
3. Starting value < 100 (typical for range in meters/kilometers)
4. Non-zero delta (excludes constant sequences)

**Implementation**: `FileReader::is_first_column_fixed_delta()` and `is_first_column_monotonic()` in `src/cpp/src/file_reader.cpp`

**Effect**: When range data is detected, TL-specific thresholds (ignore and marginal) are bypassed for column 0, as these thresholds are meaningless for distance measurements.

### Sensitive/Zero-Threshold Mode

When `thresh.significant == 0.0`, the algorithm operates in maximum sensitivity mode:

- All non-trivial differences below the ignore threshold are classified as significant
- Useful for detecting any substantive numerical change
- Distinct from raw `diff` by still applying format precision awareness

### Sub-LSB Boundary Handling

The half-ULP criterion with floating-point tolerance ensures cross-platform consistency:

```cpp
// Example: 30.8 vs 30.85 with min_dp=1
LSB = 0.1
big_zero = 0.05
raw_diff = 0.05
// Without tolerance: raw_diff == big_zero might fail due to FP representation
// With tolerance: |0.05 - 0.05| < 1e-12 × 0.05 → classified as trivial
```

### Maximum Difference and Percent Error Reporting

The system tracks and reports:

- `differ.max_non_zero`: Maximum raw difference
- `differ.max_non_trivial`: Maximum rounded difference
- `differ.max_significant`: Maximum significant difference
- `differ.max_percent_error`: Maximum percent error (finite values only)

These are displayed in the summary output with purple underlining for visibility.

---

## Failure Criteria

The comparison fails (non-zero exit code) under these conditions:

1. **Structural Mismatch**: Files have incompatible column structures
2. **File Access Error**: Cannot open or read one or both files
3. **Significant Difference**: Any non-marginal, non-critical significant difference (strict mode)
4. **Critical Difference**: Any difference exceeding `thresh.critical` in the valid numerical range (these are also classified as significant at Level 3)

> **Note:** Previous versions had a "pass-with-warning" scenario where
> files with < 2% significant differences were considered "close enough."
> This has been removed in favor of strict pass/fail semantics.

---

## Code Organization

**Analysis Flow**:
1. `FileComparator::compare_files()` orchestrates file reading
2. `FileComparator::process_line()` parses each line
3. `FileComparator::process_column()` extracts column values
4. `FileComparator::process_difference()` → `DifferenceAnalyzer::process_difference()`
5. `DifferenceAnalyzer::process_raw_values()` implements LEVEL 1
6. `DifferenceAnalyzer::process_hierarchy()` implements LEVELS 2-6

**Summary Generation**:
- `FileComparator::print_diff_like_summary()` — LEVEL 1 summary
- `FileComparator::print_rounded_summary()` — LEVEL 2 summary
- `FileComparator::print_significant_summary()` — LEVELS 3-6 summary

**Unit Tests**: Located in `src/cpp/tests/`, verifying:
- Counter summation invariants
- Edge case handling (sub-LSB, percent mode, sensitive mode)
- Threshold boundary conditions
- File structure validation

---

## Quick Code Reference

For developers working on the discrimination algorithm:

**Core Implementation Files**:
- `src/cpp/include/tl_diff.h` — Threshold definitions, data structures (`Thresholds`, `CountStats`, `DiffStats`, `Flags`)
- `src/cpp/include/difference_analyzer.h` — `DifferenceAnalyzer` class declaration
- `src/cpp/src/difference_analyzer.cpp` — Core logic: `process_difference()`, `process_raw_values()`, `process_hierarchy()`, `round_to_decimals()`
- `src/cpp/src/file_comparator.cpp` — Orchestration: `compare_files()`, `process_line()`, `process_column()`, summary printing
- `src/cpp/main/tl_diff.cpp` — CLI parsing and threshold initialization

**Key Functions by Level**:
- LEVEL 1: `DifferenceAnalyzer::process_raw_values()`
- LEVELS 2-6: `DifferenceAnalyzer::process_hierarchy()`
- Summary: `FileComparator::print_diff_like_summary()`, `print_rounded_summary()`, `print_significant_summary()`

---

## Alternative Comparison Approaches

The current implementation uses specific choices for comparison logic. For future consideration, here are alternative approaches:

### Symmetric Relative Difference
Instead of using `value2` as reference:
```cpp
symmetric_relative = |value1 - value2| / max(|value1|, |value2|)
```
**Pros**: Avoids asymmetry when reference may be noisy
**Cons**: Still undefined when both values near zero

### Combined Absolute + Relative Rule
```cpp
significant = (abs_diff > abs_threshold) OR (abs_diff / max(|v1|,|v2|) > rel_threshold)
```
**Pros**: Handles both large absolute differences and relative precision requirements
**Cons**: Two thresholds to configure

### ULP-Based Comparisons
```cpp
meaningful_threshold = max(|value1|, |value2|) × machine_epsilon × safety_factor
```
**Pros**: Scale-aware, based on floating-point representation
**Cons**: May be too lenient for high-precision requirements

These alternatives are not currently implemented but could be added with corresponding command-line flags if needed.

# Discrimination Hierarchy: Algorithm Design and Implementation

## Overview

This document describes the multi-level binary discrimination algorithm used for numerical file comparison in the `uband_diff` utility. The algorithm implements a progressive refinement pipeline that classifies numeric differences between two files into hierarchical categories, enabling domain-specific and precision-aware comparison.

## Design Philosophy

The comparison algorithm is designed around three core principles:

1. **Progressive Refinement**: Each level partitions one set into exactly two subsets, with one subset undergoing further subdivision at the next level
2. **Precision Awareness**: Differences are evaluated in the context of printed precision, machine epsilon, and floating-point representation
3. **Domain Specificity**: Special handling for transmission loss (TL) data, including physics-based thresholds derived from acoustic pressure limits and single-precision arithmetic constraints

The algorithm implements a six-level hierarchy where each level applies specific numerical criteria to progressively categorize differences from raw comparison through domain-specific significance assessment.

## Implementation Architecture

**Key source files:**
- `src/cpp/include/uband_diff.h` — Threshold definitions, data structures (`Thresholds`, `CountStats`, `DiffStats`, `Flags`)
- `src/cpp/include/difference_analyzer.h` — Core analyzer interface
- `src/cpp/src/difference_analyzer.cpp` — Discrimination logic implementation
- `src/cpp/src/file_comparator.cpp` — File parsing, orchestration, summary generation
- `src/cpp/main/uband_diff.cpp` — Command-line interface

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

**Implementation**: `DifferenceAnalyzer::process_rounded_values()` in `src/cpp/src/difference_analyzer.cpp`

**Counters**: `counter.diff_trivial`, `counter.diff_non_trivial`, `differ.max_non_trivial`

**Robustness**: FP_TOLERANCE (1e-12) handles floating-point representation errors. Example: comparing 30.8 (1dp) vs 30.85 (2dp) yields raw_diff = 0.05, which equals big_zero = 0.05, triggering the tolerance check for consistent classification.

**Percent Error Tracking**: For non-trivial differences:
```cpp
percent_error = 100 × raw_diff / |value2|  // value2 is reference
// If |value2| ≤ thresh.zero, percent_error = ∞ (sentinel)
```

---

### LEVEL 3: Significance Assessment (Machine Precision)

**Purpose**: Among non-trivial differences, separate those that are numerically meaningful from those dominated by machine precision limitations

**Partition**: `non_trivial = insignificant + significant`

**Decision Rule**:
```cpp
// Check if both values exceed ignore threshold (domain-specific)
both_above_ignore = (value1 > thresh.ignore) AND (value2 > thresh.ignore)

// For range data detection: skip TL thresholds for column 0 if monotonic with fixed delta
skip_tl_check = (column_index == 0) AND (flags.column1_is_range_data)
if (skip_tl_check) {
    both_above_ignore = false
}

if (both_above_ignore) {
    insignificant  // Values too large to be reliable in single precision
} else {
    // Apply significance threshold
    if (thresh.significant_is_percent) {
        // Percent mode: compare fractional difference
        ref = |value2|
        if (ref <= thresh.zero) {
            exceeds_significance = (rounded_diff > thresh.zero)  // Conservative
        } else {
            exceeds_significance = (rounded_diff / ref) > thresh.significant_percent
        }
    } else if (thresh.significant == 0.0) {
        // Sensitive mode: all non-trivial below ignore are significant
        exceeds_significance = true
    } else {
        // Standard mode: absolute threshold
        exceeds_significance = (rounded_diff > thresh.significant)
    }
    
    if (NOT both_above_ignore AND exceeds_significance) {
        significant
    } else {
        insignificant
    }
}
```

**Implementation**: `DifferenceAnalyzer::process_rounded_values()` in `src/cpp/src/difference_analyzer.cpp`

**Counters**: `counter.diff_insignificant`, `counter.diff_significant`, `counter.diff_high_ignore`

**Special Cases**:
- **Percent Mode**: Enabled when `thresh.significant < 0` (interpreted as percentage). Uses value2 as reference.
- **Sensitive Mode**: When `thresh.significant == 0.0`, all non-trivial differences below ignore threshold are significant.
- **Range Data**: Column 0 data detected as range (monotonic + fixed delta + starting value < 100) bypasses TL-specific thresholds.

---

### LEVEL 4: Marginal vs Non-Marginal (Operational Significance)

**Purpose**: Within significant differences, distinguish between operationally marginal cases and truly non-marginal differences

**Partition**: `significant = marginal + non_marginal`

**Decision Rule**:
```cpp
// Skip TL threshold check for range data in column 0
skip_tl_check = (column_index == 0) AND (flags.column1_is_range_data)

if (NOT skip_tl_check AND
    value1 > thresh.marginal AND value1 < thresh.ignore AND
    value2 > thresh.marginal AND value2 < thresh.ignore) {
    marginal  // Both values in marginal operational band [110, 138.47] dB
} else {
    non_marginal
}
```

**Implementation**: `DifferenceAnalyzer::process_rounded_values()` in `src/cpp/src/difference_analyzer.cpp`

**Counters**: `counter.diff_marginal`

**Domain Context**: For transmission loss data, values between 110 and 138.47 dB represent the "warning zone"—numerically valid but outside the range of primary operational interest.

---

### LEVEL 5: Critical vs Non-Critical (Model Failure Detection)

**Purpose**: Detect catastrophically large differences that may indicate model failure

**Partition**: `non_marginal = critical + non_critical`

**Decision Rule**:
```cpp
// Skip TL threshold check for range data in column 0
skip_tl_check = (column_index == 0) AND (flags.column1_is_range_data)

if (NOT skip_tl_check AND
    rounded_diff > thresh.critical AND
    value1 <= thresh.ignore AND value2 <= thresh.ignore) {
    critical  // Large difference in numerically valid range
    flags.has_critical_diff = true
    flags.error_found = true
} else {
    non_critical
}
```

**Implementation**: 
- Early check: `DifferenceAnalyzer::process_difference()` (top-level)
- Per-element: `DifferenceAnalyzer::process_rounded_values()`

**Counters**: `counter.diff_critical`

**Behavior**: Upon detecting the first critical difference, `print_hard_threshold_error()` is called to report it. The `error_found` flag ensures non-zero exit code. Processing continues to maintain consistent statistics, but table printing may be abbreviated.

---

### LEVEL 6: Error vs Non-Error (User Threshold)

**Purpose**: Among non-critical, non-marginal significant differences, apply user-specified threshold to determine failure criteria

**Partition**: `non_critical = error + non_error`

**Decision Rule**:
```cpp
if (thresh.significant_is_percent) {
    // Percent mode
    ref = |value2|
    if (ref <= thresh.zero) {
        percent_exceeds = (rounded_diff > thresh.zero)
    } else {
        percent_exceeds = (rounded_diff / ref) > thresh.significant_percent
    }
    
    if (percent_exceeds) {
        error
    } else {
        non_error
    }
} else {
    // Standard mode: already classified at LEVEL 3
    // This level uses same threshold for consistency
    if (rounded_diff > thresh.significant) {
        error
    } else {
        non_error
    }
}
```

**Implementation**: `DifferenceAnalyzer::process_rounded_values()` in `src/cpp/src/difference_analyzer.cpp`

**Counters**: `counter.diff_error`, `counter.diff_non_error`

**Note**: This level primarily serves to subdivide the non-critical, non-marginal differences for detailed reporting. The user threshold was already applied at LEVEL 3 for initial significance assessment.

---

## Hierarchical Relationships

At each level, the following invariants hold:

```
LEVEL 0: total = Σ(lines × columns)

LEVEL 1: total = zero + non_zero

LEVEL 2: non_zero = trivial + non_trivial
         total = zero + trivial + non_trivial

LEVEL 3: non_trivial = insignificant + significant
         non_zero = trivial + insignificant + significant
         total = zero + trivial + insignificant + significant

LEVEL 4: significant = marginal + non_marginal
         non_trivial = insignificant + marginal + non_marginal
         non_zero = trivial + insignificant + marginal + non_marginal
         total = zero + trivial + insignificant + marginal + non_marginal

LEVEL 5: non_marginal = critical + non_critical
         significant = marginal + critical + non_critical
         non_trivial = insignificant + marginal + critical + non_critical
         non_zero = trivial + insignificant + marginal + critical + non_critical
         total = zero + trivial + insignificant + marginal + critical + non_critical

LEVEL 6: non_critical = error + non_error
         non_marginal = critical + error + non_error
         significant = marginal + critical + error + non_error
         non_trivial = insignificant + marginal + critical + error + non_error
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
3. **Critical Difference**: Any difference exceeding `thresh.critical` in the valid numerical range
4. **Significant Difference Threshold**: When non-marginal, non-critical significant differences exceed user expectations

**Pass-with-Warning Scenario**: If non-marginal, non-critical significant differences represent less than 2% of total elements, files are considered "close enough" (configurable behavior).

---

## Code Organization

**Analysis Flow**:
1. `FileComparator::compare_files()` orchestrates file reading
2. `FileComparator::process_line()` parses each line
3. `FileComparator::process_column()` extracts column values
4. `FileComparator::process_difference()` → `DifferenceAnalyzer::process_difference()`
5. `DifferenceAnalyzer::process_raw_values()` implements LEVEL 1
6. `DifferenceAnalyzer::process_rounded_values()` implements LEVELS 2-6

**Summary Generation**:
- `FileComparator::print_diff_like_summary()` — LEVEL 1 summary
- `FileComparator::print_rounded_summary()` — LEVEL 2 summary
- `FileComparator::print_significant_summary()` — LEVELS 3-6 summary

**Unit Tests**: Located in `src/cpp/tests/`, verifying:
- Counter summation invariants
- Edge case handling (sub-LSB, percent mode, sensitive mode)
- Threshold boundary conditions
- File structure validation

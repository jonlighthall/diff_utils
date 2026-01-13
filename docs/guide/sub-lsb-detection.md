# Sub-LSB Difference Detection

## Overview

This document explains the concept of **sub-LSB (Least Significant Bit) differences** and why they are critical for precision-aware numerical file comparison, especially in cross-platform and cross-language scientific computing scenarios.

## The Problem

When comparing numerical output files from different sources (platforms, programming languages, compilers, or even different output formatting configurations), we encounter a fundamental question:

**Should values that differ by less than the minimum representable difference at the coarser precision be considered equivalent?**

## Example: The Canonical Edge Case

### Scenario
- **File 1**: `30.8` (1 decimal place precision)
- **File 2**: `30.85` (2 decimal places precision)
- **User threshold**: `0.0` (maximum sensitivity requested)

### Mathematical Analysis

```
Raw difference:      |30.8 - 30.85| = 0.05
LSB at 1dp:          10^(-1) = 0.1
Half-LSB:            0.05
Rounded values:      30.8 vs 30.9
Rounded difference:  0.1
```

### The Question
With `threshold=0`, should this be considered:
- **A failure** (difference of 0.1 after rounding)?
- **A pass** (raw difference 0.05 is sub-LSB)?

## The Rigorous Answer: PASS (Sub-LSB = Trivial)

### 1. Information-Theoretic Argument

File 1's printed value "30.8" **does not represent a single number**. It represents an **interval**:

```
30.8 (1dp) implicitly represents: [30.75, 30.85)
```

The value `30.85` from File 2 falls **exactly at the boundary** of what File 1 could represent. They are **informationally equivalent** at File 1's precision.

### 2. Cross-Platform Robustness

Consider two platforms calculating the same physical quantity:

**Platform A** (x86-64, GCC, -O2):
```c
double result = complex_calculation();  // → 30.849999999...
printf("%.1f\n", result);                // Output: 30.8
```

**Platform B** (ARM, Clang, -O3):
```c
double result = complex_calculation();  // → 30.850000001...
printf("%.2f\n", result);                // Output: 30.85
```

**Same calculation, different output formatting → should NOT fail comparison!**

### 3. Scientific Validity

In experimental or numerical simulation contexts:
- **Measurement precision**: An instrument reporting "30.8" has an implicit uncertainty of ±0.05
- **Numerical stability**: Differences smaller than the LSB are within numerical noise
- **Reproducibility**: Sub-LSB differences should not invalidate validation tests

## Implementation

### The Fix

**Before (BUGGY)**:
```cpp
bool trivial_after_rounding = (rounded_diff <= big_zero);
```

This checked if the **rounded** difference was small, which fails for the edge case because:
- `rounded_diff = 0.1` (after rounding 30.8 and 30.85 to 1dp → 30.8 and 30.9)
- `big_zero = 0.05`
- `0.1 <= 0.05` is FALSE → incorrectly classified as NON-TRIVIAL

**After (CORRECT)**:
```cpp
bool trivial_after_rounding = (rounded_diff == 0.0 || raw_diff <= big_zero);
```

This correctly checks:
1. **First**: Are the rounded values identical? (handles exact matches)
2. **Second**: Is the raw difference sub-LSB? (handles the edge case)

### Classification Hierarchy

```
LEVEL 1: All elements
├─ Exact matches (diff = 0.0)
└─ Non-zero differences (diff > 0.0)
    ├─ SUB-LSB (raw_diff <= LSB/2) → TRIVIAL
    │   └─ Example: 30.8 vs 30.85 (diff=0.05, LSB=0.1)
    │
    └─ SUPRA-LSB (raw_diff > LSB/2) → NON-TRIVIAL
        ├─ Example: 30.8 vs 30.9 (diff=0.1, LSB=0.1)
        └─ Further classified as significant/insignificant
```

## Test Coverage

The fix is validated by `test_sub_lsb_boundary.cpp` with the following test cases:

1. **ExactHalfLSBDifferenceAtZeroThreshold**: The canonical 30.8 vs 30.85 case
2. **SubLSBAtMultiplePrecisionLevels**: Sub-LSB detection at 1dp, 2dp, 3dp, 4dp
3. **SupraLSBDifferencesAreNonTrivial**: Confirms 1 LSB difference is still non-trivial
4. **MixedSubLSBAndSupraLSBDifferences**: Multiple values with different classifications
5. **CrossPlatformFormattingEquivalence**: Simulates cross-platform scenario
6. **SubLSBWithNonZeroThreshold**: Validates behavior with user thresholds > 0

## Mathematical Formulation

### Definitions

```cpp
LSB = 10^(-min_dp)              // Least Significant Bit (minimum step)
big_zero = LSB / 2.0            // Half-LSB tolerance
raw_diff = |value1 - value2|    // Unrounded difference
```

### Classification Logic

```cpp
if (raw_diff == 0.0) {
    // EXACT MATCH
    classification = EXACT;

} else if (raw_diff <= big_zero) {
    // SUB-LSB: Difference smaller than half the minimum representable step
    // These are indistinguishable at the coarser precision
    classification = TRIVIAL;

} else {
    // SUPRA-LSB: Difference is representable
    // Round both values and compare
    rounded1 = round(value1, min_dp);
    rounded2 = round(value2, min_dp);

    if (rounded1 == rounded2) {
        classification = TRIVIAL;
    } else {
        classification = NON_TRIVIAL;
        // Further classify as significant/insignificant based on threshold
    }
}
```

## Implications for Users

### When `threshold = 0.0` (Maximum Sensitivity)

**Before the fix**:
- 30.8 vs 30.85 → **FAIL** (classified as significant)
- Cross-platform validation fails unnecessarily

**After the fix**:
- 30.8 vs 30.85 → **PASS** (classified as trivial/sub-LSB)
- Cross-platform robust comparison
- Only truly distinguishable differences fail

### Backward Compatibility

This fix **only affects edge cases** where:
1. Values differ by **exactly** half-LSB or less
2. The difference becomes visible only after rounding to coarser precision

For most comparisons, behavior is unchanged. The fix makes the comparison:
- **More robust**: Cross-platform formatting differences don't fail
- **More rigorous**: Aligns with information theory and measurement principles
- **More intuitive**: "30.8" and "30.85" are the same value at 1dp precision

## References

### Related Concepts
- **ULP (Unit in the Last Place)**: Similar concept in floating-point comparison
- **Epsilon testing**: Comparing floating-point numbers with tolerance
- **Significant figures**: Scientific notation and measurement uncertainty

### IEEE 754 Floating-Point
While this is about **printed decimal precision**, not binary floating-point precision, the principles are analogous:
- Binary ULP corresponds to decimal LSB
- Half-ULP rounding corresponds to big_zero tolerance
- Representational limits apply at both levels

## Future Enhancements

### Potential LEVEL 1.5 Discrimination

Could add explicit sub-LSB counting:

```cpp
struct CountStats {
    size_t diff_sub_lsb;     // raw_diff <= LSB/2 (new category)
    size_t diff_trivial;     // rounded_diff == 0 OR sub-LSB
    size_t diff_non_trivial; // rounded_diff > 0 AND supra-LSB
};
```

This would provide users with detailed breakdown:
```
SUMMARY:
  Sub-LSB differences (indistinguishable): 5
  Trivial differences (formatting only):   3
  Non-trivial differences:                 2
```

## Conclusion

The sub-LSB fix ensures that `uband_diff` performs **true precision-aware comparison**, where:

1. Values are compared at their **representational precision**, not just numeric precision
2. Cross-platform formatting differences are **tolerated** as expected
3. Only **distinguishable differences** at the coarser precision level are reported
4. The tool behaves **rigorously** according to information theory and measurement principles

This makes `uband_diff` suitable for:
- ✅ Cross-platform numerical validation
- ✅ Multi-language code comparison (Fortran vs C++ vs Python)
- ✅ Compiler optimization testing
- ✅ Scientific reproducibility verification
- ✅ Regression testing with format changes

For the code-level implementation, unit tests, and the complete file-level change list, see `docs/IMPLEMENTATION_SUMMARY.md`.

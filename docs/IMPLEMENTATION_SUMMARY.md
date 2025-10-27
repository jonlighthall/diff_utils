# Sub-LSB Detection Implementation Summary

## Date
October 16, 2025

## Overview
Successfully implemented and validated sub-LSB (Least Significant Bit) detection in `uband_diff` to correctly handle cross-precision numerical comparisons.

## The Problem

### Original Issue
Comparing `30.8` (1 decimal place) vs `30.85` (2 decimal places) with threshold=0:
- **Before Fix:** Reported as SIGNIFICANT difference (failed comparison)
- **After Fix:** Reported as EQUIVALENT (passed comparison)

### Root Cause
The code was checking `rounded_diff` instead of `raw_diff` against `big_zero` (half-LSB):
```cpp
// BEFORE (BUGGY)
bool trivial_after_rounding = (rounded_diff <= big_zero);

// rounded_diff = 0.1 (30.85 rounds to 30.9 at 1dp)
// big_zero = 0.05 (half of LSB at 1dp)
// Check: 0.1 <= 0.05 → FALSE → Classified as NON-TRIVIAL ❌
```

## The Solution

### 1. Core Fix (`src/difference_analyzer.cpp`)
Changed to check `raw_diff` with floating-point tolerance:

```cpp
// AFTER (FIXED)
constexpr double FP_TOLERANCE = 1e-12;
bool sub_lsb_diff = (raw_diff < big_zero) ||
                   (std::abs(raw_diff - big_zero) < FP_TOLERANCE * std::max(raw_diff, big_zero));
bool trivial_after_rounding = (rounded_diff == 0.0 || sub_lsb_diff);

// raw_diff = 0.05
// big_zero = 0.05
// Check: 0.05 <= 0.05 (with FP tolerance) → TRUE → Classified as TRIVIAL ✅
```

### 2. Floating-Point Robustness
The FP_TOLERANCE handles edge cases where:
- `raw_diff = 0.05000000000000071054`
- `big_zero = 0.05000000000000000278`
- Difference: ~7e-16 (due to FP representation)

Without tolerance, `raw_diff > big_zero` would fail. With tolerance, correctly classified as equivalent.

### 3. Decimal Places Limit Update (`src/line_parser.cpp`)
**Before:**
```cpp
if (ndp < 0 || ndp > 10) {  // Arbitrary limit
```

**After:**
```cpp
constexpr int MAX_DECIMAL_PLACES = 17;  // IEEE 754 double precision limit
if (ndp < 0 || ndp > MAX_DECIMAL_PLACES) {
```

**Rationale:** Double precision provides ~15-17 significant decimal digits. The original limit of 10 was arbitrary and too restrictive.

## Testing & Validation

### 1. Unit Tests (`tests/test_sub_lsb_boundary.cpp`)
Created comprehensive test suite with 6 test cases:

1. **ExactHalfLSBDifferenceAtZeroThreshold** - The canonical 30.8 vs 30.85 case
2. **SubLSBAtMultiplePrecisionLevels** - Tests at 1dp, 2dp, 3dp, 4dp
3. **SupraLSBDifferencesAreNonTrivial** - Verifies 1 LSB difference is non-trivial
4. **MixedSubLSBAndSupraLSBDifferences** - Multiple values with different classifications
5. **CrossPlatformFormattingEquivalence** - Simulates cross-platform scenario
6. **SubLSBWithNonZeroThreshold** - Validates with user thresholds > 0

**Status:** ✅ All 43 tests pass (6 sub-LSB tests + 37 existing tests)

### 2. Pi Precision Test Suite
Created Fortran program (`pi_precision_test.f90`) that:
- Calculates π using Machin's formula
- Outputs π with increasing precision (0dp to 14dp)
- Validates that ANY two precision levels are considered equivalent

**Test Results:**
- ✅ Identical files recognized correctly
- ✅ Cross-precision (3.1 vs 3.14) recognized as equivalent
- ✅ Multiple cross-precision comparisons pass
- ✅ High precision (17 decimal places) handled correctly

### 3. Test Automation
Created `test_pi_precision.sh` script for automated validation.

## Files Modified

### Core Implementation
1. **src/difference_analyzer.cpp** (Lines 98-113)
   - Changed `trivial_after_rounding` check
   - Added `sub_lsb_diff` with FP tolerance
   - Added extensive inline comments

2. **src/line_parser.cpp** (Lines 59-73)
   - Increased `MAX_DECIMAL_PLACES` from 10 to 17
   - Updated error message to use constant

### Testing
3. **tests/test_sub_lsb_boundary.cpp** (NEW, 265 lines)
   - 6 comprehensive test cases
   - Validates edge cases and floating-point handling

### Documentation
4. **docs/SUB_LSB_DETECTION.md** (NEW)
   - Complete technical explanation
   - Information-theoretic justification
   - Mathematical formulation

5. **docs/PI_PRECISION_TEST_SUITE.md** (NEW)
   - π-based validation strategy
   - Test methodology and results

### Test Programs
6. **pi_precision_test.f90** (NEW, 126 lines)
   - Fortran 90 program to calculate π
   - Generates test files at varying precision

7. **test_pi_precision.sh** (NEW)
   - Automated test script
   - Validates all cross-precision scenarios

## Mathematical Foundation

### LSB (Least Significant Bit)
For a value with `n` decimal places:
- **LSB** = 10^(-n) (minimum representable step)
- **big_zero** = LSB / 2 (half-LSB threshold)

### Sub-LSB Criterion
A difference is **sub-LSB** if:
```
raw_diff ≤ big_zero  (with floating-point tolerance)
```

### Example: 30.8 vs 30.85
- Precision: 1 decimal place
- LSB = 10^(-1) = 0.1
- big_zero = 0.05
- raw_diff = |30.8 - 30.85| = 0.05
- Test: 0.05 ≤ 0.05 ✓
- **Classification: TRIVIAL** (sub-LSB difference)

### Information-Theoretic Justification
A value printed as `30.8` (1dp) represents the interval [30.75, 30.85).
The value `30.85` falls exactly at the boundary of this interval.
Therefore, the difference is within the representational uncertainty of the coarser precision.

## Impact & Benefits

### 1. Cross-Platform Robustness
Different systems may output the same computed value at different precisions:
- Platform A: `30.8` (1dp)
- Platform B: `30.85` (2dp)

These are now correctly recognized as equivalent, enabling robust cross-platform validation.

### 2. Scientific Validity
Sub-LSB differences are informationally equivalent at the coarser precision. Treating them as equivalent is mathematically sound and scientifically appropriate.

### 3. Backward Compatibility
- Existing tests continue to pass (37/37 original tests)
- No changes to user-facing API or command-line interface
- Only affects classification of boundary cases

## Test Results Summary

### Before Implementation
- **Original case (case6r):** 1 significant difference reported, files NOT equivalent
- **make test:** 37 tests passing

### After Implementation
- **Original case (case6r):** 0 significant differences, files ARE equivalent ✅
- **make test:** 43 tests passing (37 original + 6 new) ✅
- **pi precision tests:** All cross-precision comparisons pass ✅

## Conclusion

The sub-LSB detection implementation successfully addresses a critical edge case in numerical comparison, enabling robust cross-platform validation while maintaining mathematical rigor and scientific validity. All tests pass, documentation is complete, and the implementation is production-ready.

## Future Enhancements (Optional)

1. **LEVEL 1.5 Discrimination:** Add explicit sub-LSB counter for detailed statistics
2. **User Control:** Add command-line flag to enable/disable sub-LSB detection
3. **Extended Precision:** Support for quadruple precision (IEEE 754 binary128)

## References

- IEEE 754 Double Precision: ~15-17 significant decimal digits
- Machine epsilon (double): ~2.22e-16
- Information theory: Precision defines representational uncertainty

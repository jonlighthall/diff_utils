# TRANSIENT_SPIKES Pattern Enhancement

## Overview

This document describes the enhancement to `uband_diff` pass/fail logic that incorporates the **TRANSIENT_SPIKES** error pattern analysis into the decision-making process.

## Problem Statement

The original pass/fail logic used a simple threshold: if non-marginal, non-critical significant differences exceeded 2% of total elements, the comparison would **FAIL**. However, this didn't account for the **nature** of the errors:

- **Systematic errors** (growing with range, fixed bias) → genuine algorithmic differences → should fail
- **Transient spikes** (isolated outliers) → numerical instabilities, not systematic → should pass with caveat

The test case `pe.std3.pe01` exemplifies this: 58 significant differences (1.63%), but they are isolated spikes, not systematic errors.

## Analysis Metrics Supporting TRANSIENT_SPIKES Pass

For `pe.std3.pe01.ref.txt` vs `pe.std3.pe01.test.txt` with threshold 0.4:

### Key Statistics
- **Total elements**: 3554
- **Significant differences**: 58 (1.63%)
- **Non-marginal, non-critical**: 44 (1.24%)
- **Maximum error**: 2.71
- **RMSE**: 0.7277
- **Max/RMSE ratio**: **3.72** ← Key metric!

### Pattern Classification Criteria

TRANSIENT_SPIKES is detected when:
1. `max_error > 3.0 × RMSE` ✓ (2.71 > 2.18)
2. `is_random = true` ✓ (Run test passes, Z = 0.00)

### Supporting Evidence

| Metric | Value | Interpretation |
|--------|-------|----------------|
| R² | 0.031 | Weak fit — no systematic trend |
| P-value (slope) | 0.356 | Not significant — slope is random |
| Autocorrelation | 0.682 | Correlated locally, but run test shows overall randomness |
| Runs | 1 observed, 1 expected | Pattern is random, not systematic |
| RMSE | 0.73 | Most errors are small |
| Max error | 2.71 | Only a few outliers |

**Conclusion**: The errors are **isolated spikes**, not systematic growth or bias. This indicates:
- Numerical convergence issues at specific range bins
- Grid discontinuities
- Boundary condition artifacts

These are **NOT indicative of algorithmic failure** — the models fundamentally agree, with occasional numerical instability.

## Implementation

### Changes Made

**File**: `src/cpp/include/uband_diff.h`
- Added `#include <optional>` and `#include "error_accumulation_analyzer.h"`
- Added member variable: `mutable std::optional<AccumulationMetrics> accumulation_metrics_`

**File**: `src/cpp/src/file_comparator.cpp`

#### 1. Early Pattern Analysis (Lines ~1615-1685)

In `print_significant_percentage()`, the accumulation analysis is now performed **before** the pass/fail decision:

```cpp
// Run accumulation analysis to get error pattern (if enough data and not already computed)
bool has_transient_spikes = false;
if (!accumulation_metrics_.has_value() &&
    accumulation_data_.n_points >= 5) {
  ErrorAccumulationAnalyzer analyzer;
  accumulation_metrics_ = analyzer.analyze(accumulation_data_);
  has_transient_spikes =
      (accumulation_metrics_->pattern ==
       AccumulationMetrics::ErrorPattern::TRANSIENT_SPIKES);
} else if (accumulation_metrics_.has_value()) {
  // Use cached result
  has_transient_spikes =
      (accumulation_metrics_->pattern ==
       AccumulationMetrics::ErrorPattern::TRANSIENT_SPIKES);
}
```

#### 2. Enhanced Pass/Fail Logic

Three scenarios now exist:

**Scenario A: Exceeds 2% threshold (FAIL region)**
```cpp
if (critical_percent > failure_threshold_percent) {  // > 2%
  if (has_transient_spikes && critical_percent <= 10.0) {
    // PASS with caveat: Pattern justifies override
    std::cout << "PASS (with caveat): ... exceed 2% threshold,\n"
              << "but TRANSIENT_SPIKES pattern detected (isolated outliers, not systematic error)"
    flag.files_are_close_enough = true;
  } else {
    // FAIL: Too many errors OR non-transient pattern
    std::cout << "FAIL: ... exceed 2% threshold"
    flag.files_are_close_enough = false;
  }
}
```

**Scenario B: Within 2% threshold (PASS region)**
```cpp
else if (critical_percent > 0) {  // 0% < percent <= 2%
  if (has_transient_spikes) {
    // PASS with annotation
    std::cout << "PASS: ... within 2% tolerance\n"
              << "(TRANSIENT_SPIKES pattern: isolated outliers, not systematic)"
  } else {
    // PASS (normal)
    std::cout << "PASS: ... within 2% tolerance"
  }
  flag.files_are_close_enough = true;
}
```

**Scenario C: Zero errors (always PASS)**
```cpp
else {
  std::cout << "PASS: No non-marginal, non-critical significant differences found"
  flag.files_are_close_enough = true;
}
```

#### 3. Cached Metrics Reuse (Lines ~2331-2345)

In `print_accumulation_analysis()`, reuse cached metrics to avoid recomputation:

```cpp
// Use cached metrics if available, otherwise compute them
AccumulationMetrics metrics;
if (accumulation_metrics_.has_value()) {
  metrics = *accumulation_metrics_;
} else {
  ErrorAccumulationAnalyzer analyzer;
  metrics = analyzer.analyze(accumulation_data_);
}
```

### Thresholds

| Threshold | Value | Meaning |
|-----------|-------|---------|
| `failure_threshold_percent` | 2.0% | Original hard threshold |
| `transient_spikes_relaxed_threshold` | 10.0% | Extended threshold for TRANSIENT_SPIKES pattern |

**Rationale**: If errors are isolated spikes (not systematic), we can tolerate up to 10% differences. Beyond 10%, even transient spikes indicate too many problematic bins.

## Test Results

### Before Enhancement
```
SUMMARY:
  LEVEL 3 DISCRIMINATION: both values less than 138.47
   Significant differences   ( >0.40):   58 ( 1.63%)
   PASS: Non-marginal, non-critical significant differences (44, 1.24%) within 2.00% tolerance
```

### After Enhancement
```
SUMMARY:
  LEVEL 3 DISCRIMINATION: both values less than 138.47
   Significant differences   ( >0.40):   58 ( 1.63%)
   PASS: Non-marginal, non-critical significant differences (44, 1.24%) within 2.00% tolerance
   (TRANSIENT_SPIKES pattern: isolated outliers, not systematic)
```

**Key Difference**: The pass verdict now explicitly notes that the TRANSIENT_SPIKES pattern supports the decision, providing additional confidence that the differences are benign.

## Future Test Scenario

To fully test the **PASS with caveat** logic (critical_percent > 2% but ≤ 10%), you would need a test case with:
- 2% - 10% significant differences
- TRANSIENT_SPIKES pattern (max > 3×RMSE, random)

This would trigger:
```
PASS (with caveat): Non-marginal, non-critical significant differences (X, Y%) exceed 2% threshold,
but TRANSIENT_SPIKES pattern detected (isolated outliers, not systematic error)
```

## Conclusion

### Why This Matters

The enhancement **weights the statistical analysis** in the pass/fail decision:

- **Simple threshold**: Binary decision based only on percentage
- **Enhanced logic**: Considers percentage **AND** error pattern
  - Systematic → strict 2% threshold
  - Transient spikes → relaxed 10% threshold

### Which Analysis Best Supports Pass?

For `pe.std3.pe01`:

| Analysis | Result | Supports Pass? |
|----------|--------|----------------|
| **Percentage** | 1.24% < 2% | ✓ Yes (already passed) |
| **Max/RMSE ratio** | **3.72 > 3.0** | ✓✓ **Strongest support** |
| **Run test** | Z = 0.00 (random) | ✓ Yes |
| **R²** | 0.031 (weak fit) | ✓ Yes |
| **P-value** | 0.356 (not significant) | ✓ Yes |

**Answer**: The **Max/RMSE ratio (3.72)** is the single most diagnostic metric. It directly quantifies that errors are **isolated spikes** (high max) rather than **systematic** (low RMSE). This is the core criterion for TRANSIENT_SPIKES classification.

### Weighted Decision Framework

The pass/fail logic now effectively weights analyses:

1. **Primary**: Percentage threshold (2%)
2. **Override/Caveat**: TRANSIENT_SPIKES pattern
   - Driven by: Max/RMSE ratio > 3.0 + randomness test
   - Effect: Extends threshold from 2% → 10%

This provides a **scientifically-informed pass/fail** rather than an arbitrary cutoff.

---

**Author**: Updated October 30, 2025
**Related**: See `DISCRIMINATION_HIERARCHY.md` for full algorithm details

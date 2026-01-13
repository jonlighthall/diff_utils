# RMSE Implementation Summary

## Overview

Added Root Mean Square Error (RMSE) calculation and reporting to `uband_diff` with intelligent handling of multi-column data structures.

## Implementation Details

### Data Structures

**File**: `src/cpp/include/uband_diff.h`

Added new `RMSEStats` structure with three levels of RMSE tracking:

```cpp
struct RMSEStats {
  // Global RMSE (all elements)
  double sum_squared_errors_all = 0.0;
  size_t count_all = 0;

  // RMSE excluding range column (column 0)
  double sum_squared_errors_data = 0.0;
  size_t count_data = 0;

  // Per-column RMSE (for multi-column TL data)
  std::map<size_t, double> sum_squared_errors_per_column;
  std::map<size_t, size_t> count_per_column;

  void add_error(size_t column_index, double error);
  double get_rmse_all() const;
  double get_rmse_data() const;
  double get_rmse_column(size_t column_index) const;
};
```

### Accumulation

**File**: `src/cpp/src/file_comparator.cpp`

Errors are accumulated in `process_difference()` immediately after calculating the raw difference:

```cpp
// Calculate unrounded and rounded differences
double diff_unrounded = std::abs(column_data.value1 - column_data.value2);
// ...

// Accumulate RMSE statistics (use raw difference for all comparisons)
rmse_stats.add_error(column_index, diff_unrounded);
```

### Reporting

RMSE statistics are printed in the summary section, after general statistics and before detailed discrimination levels:

**Output Format**:

1. **All elements**: Global RMSE across all columns
2. **Data only**: RMSE excluding range column (if multi-column detected)
3. **Per-column**: Individual RMSE for each data column (if > 2 columns)

## Test Results

### Multi-Column TL Data (pe.std1.pe01)

```
RMSE (Root Mean Square Error):
   All elements:     1.0461e-02
   Data only (excluding range): 1.0971e-02
   Per-column RMSE:
      Column  1 (range):   0.0000e+00
      Column  2 (curve 1): 1.9198e-02
      Column  3 (curve 2): 1.6865e-02
      Column  4 (curve 3): 1.5139e-02
      Column  5 (curve 4): 1.0856e-02
      Column  6 (curve 5): 1.0764e-02
      Column  7 (curve 6): 7.6020e-03
      Column  8 (curve 7): 5.3225e-04
      Column  9 (curve 8): 0.0000e+00
      Column 10 (curve 9): 5.3225e-04
      Column 11 (curve 10): 5.4279e-03
```

**Analysis**:
- 11 columns total (1 range + 10 TL curves)
- Range column: perfect match (RMSE = 0)
- Curves 1-3: highest RMSE (~1.5-1.9e-02)
- Curves 8-9: perfect matches (RMSE = 0)
- Per-curve RMSE allows identification of which TL curves differ most

### Single-Column TL Data (pe.std3.pe01)

```
RMSE (Root Mean Square Error):
   All elements:     1.2054e-01
   Data only (excluding range): 1.7045e-01
```

**Analysis**:
- 2 columns (range + single TL curve)
- All elements includes range differences
- Data-only RMSE (1.7045e-01) represents pure TL error
- Higher than accumulation analysis RMSE (7.277e-01) because:
  - This RMSE includes ALL points (3554 total)
  - Accumulation RMSE only uses significant errors (58 points)

### Single-Column Pi Data (pi_fortran)

```
RMSE (Root Mean Square Error):
   All elements:     3.8106e-02
   Data only (excluding range): 5.3890e-02
```

**Analysis**:
- 2 columns (index + pi value)
- Index as "range" column
- RMSE captures precision differences in pi calculations

## RMSE vs Accumulation Analysis RMSE

Two different RMSE values are now reported:

| Metric | Scope | Use Case |
|--------|-------|----------|
| **RMSE Statistics** | All elements compared | Overall file similarity |
| **Accumulation RMSE** | Only significant errors (Level 3+) | Error pattern analysis |

**Example (pe.std3.pe01)**:
- Statistics RMSE (data): **1.7045e-01** (all 3554 elements)
- Accumulation RMSE: **7.2773e-01** (58 significant errors only)

The accumulation RMSE is higher because it excludes the 3496 near-zero differences, focusing only on the outliers.

## Special Handling

### Range Column Detection

When column 0 is detected as range data:
- Excluded from "data only" RMSE
- Reported separately in per-column output
- Labeled as "(range)" in output

### Multi-Column Detection

Per-column RMSE is only printed when:
- Number of columns > 2 (range + at least 2 data columns)
- Enables per-curve analysis for TL data

### Single-Column Files

For files with only 1 column:
- "Data only" RMSE is 0 (no data columns)
- Displays: "(Single column data)"

## Integration with Existing Features

RMSE complements existing metrics:

| Existing | New | Relationship |
|----------|-----|--------------|
| Maximum difference | RMSE | Max shows worst case, RMSE shows average |
| Percent error | RMSE | Both measure relative error magnitude |
| Significant count | RMSE | Count shows frequency, RMSE shows magnitude |
| TRANSIENT_SPIKES | RMSE | Pattern uses accumulation RMSE; statistics RMSE is global |

## Weighted RMSE Implementation

**Status**: ✅ **COMPLETED** October 30, 2025

### Weighting Function

Implements TL-based operational significance weighting:

```cpp
// Weighting parameters
TL_MIN_WEIGHT = 60.0;   // Full weight (1.0) for TL ≤ 60 dB
TL_MAX_WEIGHT = 110.0;  // Zero weight (0.0) for TL ≥ 110 dB

// Calculate weight
if (TL <= 60.0)        weight = 1.0;
else if (TL >= 110.0)  weight = 0.0;
else                   weight = (110.0 - TL) / 50.0;  // Linear
```

**Rationale**:
- **TL ≤ 60 dB**: Operational region (full weight = 1.0)
- **60-110 dB**: Transition region (linear weight 1.0 → 0.0)
- **TL ≥ 110 dB**: Marginal region (zero weight = 0.0)

### Implementation Details

**File**: `src/cpp/include/uband_diff.h`

Added to `RMSEStats`:
- `sum_weighted_squared_errors_data` - Global weighted sum
- `sum_weights_data` - Total weight sum
- `sum_weighted_squared_errors_per_column` - Per-column weighted sums
- `sum_weights_per_column` - Per-column weight sums
- `calculate_tl_weight(tl_value)` - Static weighting function
- `add_weighted_error(col, error, tl_ref, tl_test)` - Accumulator
- `get_weighted_rmse_data()` - Global weighted RMSE
- `get_weighted_rmse_column(col)` - Per-column weighted RMSE

**File**: `src/cpp/src/file_comparator.cpp`

In `process_difference()`:
```cpp
// Accumulate weighted RMSE for data columns (column > 0)
if (column_index > 0) {
  rmse_stats.add_weighted_error(column_index, diff_unrounded,
                                 column_data.value1, column_data.value2);
}
```

Uses average TL from both files for weighting: `avg_tl = (tl_ref + tl_test) / 2.0`

### Test Results

#### Multi-Column Case (pe.std1.pe01)

```
RMSE (Root Mean Square Error):
   All elements:     1.0461e-02
   Data only (excluding range): 1.0971e-02
   Weighted RMSE (TL-weighted, data only): 2.3026e-03 [weight: 1.0 at TL≤60 dB, 0.0 at TL≥110 dB]
   Per-column RMSE:
      Column  2 (curve 1): 1.9198e-02 (weighted: 1.5552e-03)
      Column  3 (curve 2): 1.6865e-02 (weighted: 2.4584e-03)
      Column  4 (curve 3): 1.5139e-02 (weighted: 3.3043e-03)
      ...
```

**Analysis**:
- Overall weighted RMSE: **4.76× lower** than unweighted (2.30e-03 vs 1.10e-02)
- Curve 1: **12.3× reduction** → errors concentrated at high TL (marginal region)
- Curves 3-4: **4-5× reduction** → errors more distributed

#### Single-Column Case (pe.std3.pe01)

```
RMSE (Root Mean Square Error):
   All elements:     1.2054e-01
   Data only (excluding range): 1.7045e-01
   Weighted RMSE (TL-weighted, data only): 1.0828e-01 [weight: 1.0 at TL≤60 dB, 0.0 at TL≥110 dB]
```

**Analysis**:
- Weighted RMSE: **1.57× lower** than unweighted (1.08e-01 vs 1.70e-01)
- Moderate reduction → errors span operational and marginal regions
- Supports TRANSIENT_SPIKES classification (not just high-TL noise)

### Interpretation Guide

| Weighted/Unweighted Ratio | Interpretation |
|---------------------------|----------------|
| **> 10×** | Errors heavily concentrated at high TL (marginal region) |
| **3-10×** | Errors more prevalent at high TL, but operational region affected |
| **1.5-3×** | Errors distributed across TL range |
| **< 1.5×** | Errors concentrated at low TL (operational region) - **concerning** |

### Use Cases

1. **Operational Assessment**: Low weighted RMSE → acceptable for deployment
2. **Error Localization**: High ratio → errors in marginal region (less critical)
3. **Model Validation**: Compare weighted vs unweighted to understand error distribution

---

**Author**: Implemented October 30, 2025
**Related**:
- `DISCRIMINATION_HIERARCHY.md` - Error classification levels
- `TRANSIENT_SPIKES_ENHANCEMENT.md` - Pattern-based pass/fail logic
- `task_list.md` - Weighted RMSE completed
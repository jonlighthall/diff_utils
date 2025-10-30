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

## Future Enhancements

As noted in `task_list.md`, the next step is **weighted RMSE**:

```matlab
% Weighting function for TL values
wTL_min = 60;
wTL_max = 110;
wTL_span = wTL_max - wTL_min;

weight = (wTL_max - TL) / wTL_span;
weight = min(weight, 1);
weight = max(weight, 0);
```

This will:
- Weight TL ≤ 60 dB at 1.0 (full weight - operational region)
- Weight TL ≥ 110 dB at 0.0 (zero weight - marginal region)
- Linear interpolation between (60-110 dB)

Implementation plan:
1. Add `RMSEStats::add_weighted_error(column_index, error, tl_value)`
2. Track weighted sum separately
3. Print weighted RMSE alongside unweighted

---

**Author**: Implemented October 30, 2025
**Related**:
- `DISCRIMINATION_HIERARCHY.md` - Error classification levels
- `TRANSIENT_SPIKES_ENHANCEMENT.md` - Pattern-based pass/fail logic
- `task_list.md` - Next: weighted RMSE implementation

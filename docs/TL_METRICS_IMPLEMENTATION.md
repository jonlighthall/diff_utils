# TL Curve Comparison Metrics Implementation

## Overview

This document describes the implementation of TL (Transmission Loss) curve comparison metrics in `uband_diff`, based on the methodology from Goodman et al. for comparing acoustic propagation models.

## Metrics Implemented

### M1: Weighted TL Difference (TL_diff_1)
- **Definition**: Weighted RMSE across all range points, using operational significance weighting
- **Weighting Function**:
  - Weight = 1.0 for TL ≤ 60 dB (operational region)
  - Weight = 0.0 for TL ≥ 110 dB (marginal region)
  - Linear interpolation between 60-110 dB
- **Already Implemented**: Uses existing weighted RMSE calculation

### M2: Last 4% Range TL Difference (TL_diff_2)
- **Definition**: Mean absolute difference in the last 4% of the range extent
- **Purpose**: Emphasizes differences at maximum range where detection is marginal
- **Implementation**: Retroactively computed after all data collected using finalize()

### M3: TL Correlation Coefficient
- **Definition**: Pearson correlation coefficient between TL curves
- **Range**: -1.0 (perfect negative correlation) to +1.0 (perfect positive correlation)
- **Purpose**: Measures how well the curves track each other in shape
- **Note**: Typically very high (>0.99) for well-matching curves

### M_curve: Combined Curve Metric
- **Definition**: Average of M1, M2, and M3 scores (0-100 scale)
- **Scoring Function** (from Goodman Figure 1):
  - For TL differences:
    - diff ≤ 3 dB: score = 100 - (diff/3.0) × 10 (range: 90-100)
    - 3 < diff < 20 dB: score = 90 - ((diff-3)/17) × 90 (linear decrease)
    - diff ≥ 20 dB: score = 0
  - For correlation:
    - score = max(0, correlation × 100)
- **M_curve = (score1 + score2 + score3) / 3**

## Data Structures

### TLMetrics Structure
```cpp
struct TLMetrics {
  // Last 4% range statistics
  double sum_diff_last_4pct;
  size_t count_last_4pct;
  double max_range;

  // Correlation data
  std::vector<double> tl1_values;   // TL from file 1
  std::vector<double> tl2_values;   // TL from file 2
  std::vector<double> ranges;       // Range values
  std::vector<double> diffs;        // Absolute differences

  bool has_data;

  // Methods
  void add_point(double range, double tl1, double tl2, double diff_abs);
  void finalize();  // Compute last 4% statistics
  double calculate_m2() const;
  double calculate_correlation() const;
  static double score_from_diff(double diff);
  double calculate_m_curve(double m1_diff) const;
};
```

## Implementation Details

### Data Collection
- **Location**: `FileComparator::process_difference()`
- **When**: For every data column comparison (column_index > 0)
- **What's Tracked**:
  - Range value (from column 0)
  - TL values from both files
  - Absolute difference
- **Code**:
```cpp
if (column_index > 0) {
  rmse_stats.add_weighted_error(column_index, diff_unrounded,
                                column_data.value1, column_data.value2);
  tl_metrics.add_point(column_data.range, column_data.value1,
                       column_data.value2, diff_unrounded);
}
```

### Finalization
- **Location**: `FileComparator::compare_files()` at end
- **Purpose**: Compute last 4% statistics after max range is known
- **Code**:
```cpp
flag.file_end_reached = true;
tl_metrics.finalize();  // Calculate last 4% statistics
```

### Output
- **Location**: `FileComparator::print_tl_metrics()`
- **Called From**: `print_summary()` after RMSE statistics
- **Output Format**:
```
TL Curve Comparison Metrics:
   M1 (weighted TL diff):    2.3026e-03 dB  (score: 100.0/100)
   M2 (last 4% TL diff):     0.0000e+00 dB  (score: 100.0/100)
   M3 (TL correlation):      1.0000      (score: 100.0/100)
   M_curve (avg of M1-M3):   100.0/100  (EXCELLENT agreement)
   Data points analyzed:     3530 (max range: 100.31, last 4%: 90 points)
```

## Interpretation Guidelines

### M_curve Score Ranges
- **90-100**: EXCELLENT agreement (green)
  - Curves are nearly identical
  - Suitable for operational use

- **80-89**: GOOD agreement (green)
  - Minor differences, operationally acceptable

- **70-79**: FAIR agreement (yellow)
  - Noticeable differences but may be acceptable

- **60-69**: MARGINAL agreement (yellow)
  - Significant differences, review carefully

- **<60**: POOR agreement (red)
  - Major differences, investigate causes

### Component Interpretation

#### M1 (Weighted TL Diff)
- **Low values** (<0.01 dB): Excellent operational match
- **Medium values** (0.01-3 dB): Good match, minor differences
- **High values** (>3 dB): Significant operational differences

#### M2 (Last 4% TL Diff)
- **Purpose**: Detect differences at maximum range
- **Typical**: Often smaller than M1 since high-TL regions have natural variability
- **Concern**: Large M2 with small M1 suggests range-dependent bias

#### M3 (Correlation)
- **>0.99**: Curves have nearly identical shape
- **0.95-0.99**: Good shape match with minor deviations
- **<0.95**: Curves have different character or structure

## Test Results

### pe.std1.pe01 (Multi-column, minimal differences)
```
M1: 2.30e-03 dB (score: 100.0)
M2: 0.00e+00 dB (score: 100.0)
M3: 1.0000 (score: 100.0)
M_curve: 100.0/100 (EXCELLENT)
Data points: 3530 (max range: 100.31, last 4%: 90 points)
```
- **Interpretation**: Perfect agreement across all metrics

### pe.std3.pe01 (TRANSIENT_SPIKES pattern)
```
M1: 1.08e-01 dB (score: 99.6)
M2: 3.58e-02 dB (score: 99.9)
M3: 0.9998 (score: 100.0)
M_curve: 99.8/100 (EXCELLENT)
Data points: 1777 (max range: 20.01, last 4%: 66 points)
```
- **Interpretation**:
  - High M_curve (99.8) despite 58 significant differences
  - M1 shows small weighted error (0.108 dB) because errors occur at high TL
  - M2 very small (0.036 dB) - good agreement at max range
  - M3 nearly perfect (0.9998) - curves have same shape
  - **Conclusion**: Curves agree excellently; differences are transient outliers in marginal region

## Relationship to Other Metrics

### vs. Unweighted RMSE
- **Unweighted RMSE**: Treats all TL values equally
- **M1 (Weighted)**: Emphasizes operationally significant region (TL ≤ 60 dB)
- **Example (pe.std1)**: Unweighted = 1.10e-02, Weighted = 2.30e-03 (4.76× reduction)

### vs. Error Accumulation Analysis
- **Accumulation Analysis**: Examines error patterns (trending, correlated, random)
- **TL Metrics**: Quantifies overall curve agreement
- **Complementary**: Both provide different perspectives on differences

### vs. Pass/Fail Thresholds
- **Pass/Fail**: Binary decision based on error count and pattern
- **TL Metrics**: Continuous quantitative assessment
- **Use Together**: M_curve provides confidence in PASS result

## Future Enhancements

### M4: Range Coverage Energy Difference
- **Requires**: Figure of Merit (FOM) input parameter
- **Definition**: Percentage difference in range coverage where signal excess > 0
- **Status**: Not yet implemented (requires FOM as additional input)

### M5: Near-Continuous Detection Range
- **Requires**: Figure of Merit (FOM) input parameter
- **Definition**: Percentage difference in continuous detection range
- **Status**: Not yet implemented (requires FOM as additional input)

### M_total
- **Definition**: Average of all 5 metrics (M1-M5)
- **Status**: Awaiting M4/M5 implementation

## References

Based on methodology from:
- Goodman, R., et al. "Comparison of Acoustic Propagation Models"
- TL weighting parameters from operational acoustic analysis standards
- Scoring function (Figure 1) from Goodman paper

## Code Locations

- **Header**: `src/cpp/include/uband_diff.h` (lines ~314-407)
- **Data Collection**: `src/cpp/src/file_comparator.cpp` (lines ~777-783)
- **Finalization**: `src/cpp/src/file_comparator.cpp` (line ~532)
- **Output**: `src/cpp/src/file_comparator.cpp` (lines ~2024-2070)
- **Standalone Example**: `tl_metrics.cpp` (reference implementation)

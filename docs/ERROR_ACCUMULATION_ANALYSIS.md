# Error Accumulation Analysis for Acoustic Propagation Models

## Overview

This document describes a proposed enhancement to `uband_diff` that analyzes the **spatial coherence** of errors as a function of range. This addresses the operational need to distinguish between:

1. **Transient/spurious errors** - Random noise that can be ignored
2. **Systematic errors** - Meaningful differences that indicate model problems

## Problem Statement

### Current Limitation: The 2% Rule

The current approach uses a hand-waving "2% threshold" to determine if differences are significant. This is a **global metric** that doesn't consider the **structure** of errors:

- Does the error grow with range? (propagation error)
- Does the error have a fixed offset? (calibration/bias error)
- Are errors correlated in adjacent range bins? (systematic difference)
- Or are errors random/isolated? (numerical artifacts, noise)

### Operational Insight

**Unimportant errors** (can ignore):
- Occur at null points (low signal, high TL)
- Transient numerical artifacts (platform differences)
- Random, uncorrelated between range bins
- Tend to cancel out when accumulated

**Important errors** (require attention):
- Build up with increasing range
- Have fixed offset across all ranges
- Show spatial correlation (adjacent bins have similar errors)
- Accumulate rather than cancel

## Established Metrics

### 1. Linear Regression: Error vs Range

**Metric:** Slope of error as a function of range

```
error[i] = slope × range[i] + intercept
```

**Interpretation:**
- `slope > 0`: Error grows with range (propagation issue)
- `slope ≈ 0, high R²`: Fixed offset (bias/calibration)
- `slope ≈ 0, low R²`: Random noise (ignore)

**Acoustic modeling context:**
- Propagation models accumulate error with distance
- A non-zero slope indicates systematic model difference
- Used in ray-tracing validation (Bellhop, RAM, etc.)

### 2. Autocorrelation of Residuals

**Metric:** Pearson correlation between adjacent errors

```
ρ(lag=1) = corr(error[i], error[i+1])
```

**Interpretation:**
- `ρ > 0.5`: Strong positive correlation (systematic pattern)
- `ρ ≈ 0`: Independent errors (random noise)
- `ρ < -0.5`: Oscillating pattern (numerical instability)

**Statistical significance:**
- For random noise: `|ρ| < 2/√N` (95% confidence)
- Higher correlation suggests non-random structure

### 3. Run Test (Wald-Wolfowitz)

**Metric:** Number of sign changes in error sequence

```
run = continuous sequence of positive or negative errors
expected_runs = (2×n_pos×n_neg)/N + 1  (for random data)
```

**Interpretation:**
- Too few runs → systematic trend (errors don't change sign)
- Too many runs → oscillation
- Expected runs → random noise

**Example:**
```
Errors: [+0.1, +0.2, +0.3, -0.05, -0.02, +0.1]
Runs:    [+++++++++++++]  [-------]    [++]  = 3 runs
Expected (random): ~4 runs
Conclusion: Fewer runs suggests positive bias trend
```

### 4. Cumulative Sum (CUSUM)

**Metric:** Running sum of errors

```
CUSUM[i] = Σ(error[0:i])
```

**Interpretation:**
- Random errors: CUSUM oscillates around zero
- Systematic bias: CUSUM trends up/down
- Propagation error: CUSUM has quadratic growth

**Visual test:**
- Plot CUSUM vs range
- Random noise → noisy horizontal line
- Systematic error → monotonic trend

### 5. Root Mean Square Error Growth

**Metric:** RMSE in sliding windows vs range

```
RMSE(window) = √(Σ error² / n_points)
```

**Interpretation:**
- Constant RMSE → uniform error distribution
- Increasing RMSE → error grows with range
- Decreasing RMSE → model converges (unusual)

## Proposed Implementation

### Phase 1: Data Collection (During Comparison)

Add to existing `FileComparator` class:

```cpp
struct ErrorAccumulationData {
  std::vector<double> ranges;           // Range values (column 1)
  std::vector<double> errors;           // raw_diff values
  std::vector<double> tl_values_ref;    // TL from reference file
  std::vector<double> tl_values_test;   // TL from test file
  std::vector<bool> is_significant;     // Flags for significance

  // Metadata
  size_t n_points = 0;
  double range_min = 0.0;
  double range_max = 0.0;
};

// In FileComparator class:
ErrorAccumulationData accumulation_data_;
```

### Phase 2: Statistical Analysis (After Comparison)

Add new analysis class:

```cpp
class ErrorAccumulationAnalyzer {
public:
  struct AccumulationMetrics {
    // Linear regression: error vs range
    double slope;              // d(error)/d(range)
    double intercept;          // y-intercept
    double r_squared;          // goodness of fit
    double p_value_slope;      // statistical significance

    // Autocorrelation
    double autocorr_lag1;      // correlation(error[i], error[i+1])
    bool is_correlated;        // |autocorr| > threshold

    // Run test
    int n_runs;                // observed runs
    int expected_runs;         // expected for random
    double run_test_z_score;   // standardized test statistic
    bool is_random;            // passes run test

    // CUSUM
    double max_cusum;          // maximum absolute CUSUM
    double final_cusum;        // net accumulated error
    bool cusum_trending;       // CUSUM shows trend

    // RMSE analysis
    double overall_rmse;
    std::vector<double> windowed_rmse;  // RMSE in range windows
    double rmse_growth_rate;            // d(RMSE)/d(range)

    // Classification
    ErrorPattern pattern;      // SYSTEMATIC, RANDOM, NOISE, etc.
    std::string interpretation;
  };

  enum class ErrorPattern {
    SYSTEMATIC_GROWTH,    // Error increases with range (slope > threshold)
    SYSTEMATIC_BIAS,      // Fixed offset (slope ≈ 0, high R²)
    RANDOM_NOISE,         // Uncorrelated, passes run test
    TRANSIENT_SPIKES,     // Isolated large errors
    NULL_POINT_NOISE,     // Errors at low signal regions
    OSCILLATING           // Unstable numerical behavior
  };

  AccumulationMetrics analyze(const ErrorAccumulationData& data,
                              const Thresholds& thresh);

private:
  // Helper methods
  std::pair<double, double> linear_regression(
      const std::vector<double>& x,
      const std::vector<double>& y);

  double calculate_autocorrelation(
      const std::vector<double>& data, int lag);

  int count_runs(const std::vector<double>& data);

  std::vector<double> calculate_cusum(
      const std::vector<double>& errors);

  std::vector<double> windowed_rmse(
      const std::vector<double>& ranges,
      const std::vector<double>& errors,
      int window_size);

  ErrorPattern classify_pattern(const AccumulationMetrics& metrics);
};
```

### Phase 3: Enhanced Reporting

Add to `FileComparator::print_summary()`:

```cpp
void FileComparator::print_accumulation_analysis() const {
  if (accumulation_data_.n_points < 10) {
    std::cout << "\nInsufficient data for accumulation analysis (need ≥10 points)\n";
    return;
  }

  ErrorAccumulationAnalyzer analyzer;
  auto metrics = analyzer.analyze(accumulation_data_, thresh);

  std::cout << "\n=== ERROR ACCUMULATION ANALYSIS ===\n";
  std::cout << "Range: " << accumulation_data_.range_min
            << " to " << accumulation_data_.range_max << "\n";
  std::cout << "Data points: " << accumulation_data_.n_points << "\n\n";

  // Linear trend
  std::cout << "Linear Regression (error vs range):\n";
  std::cout << "  Slope:       " << metrics.slope
            << " (error per unit range)\n";
  std::cout << "  Intercept:   " << metrics.intercept << "\n";
  std::cout << "  R²:          " << metrics.r_squared
            << (metrics.r_squared > 0.7 ? " (strong fit)" : " (weak fit)") << "\n";
  std::cout << "  P-value:     " << metrics.p_value_slope
            << (metrics.p_value_slope < 0.05 ? " (SIGNIFICANT)" : " (not significant)") << "\n\n";

  // Autocorrelation
  std::cout << "Autocorrelation Analysis:\n";
  std::cout << "  Lag-1 correlation: " << metrics.autocorr_lag1 << "\n";
  std::cout << "  Status: " << (metrics.is_correlated ?
      "CORRELATED (systematic pattern)" :
      "UNCORRELATED (random)") << "\n\n";

  // Run test
  std::cout << "Run Test (randomness):\n";
  std::cout << "  Observed runs:  " << metrics.n_runs << "\n";
  std::cout << "  Expected runs:  " << metrics.expected_runs << "\n";
  std::cout << "  Z-score:        " << metrics.run_test_z_score << "\n";
  std::cout << "  Status: " << (metrics.is_random ?
      "RANDOM (passes test)" :
      "NON-RANDOM (systematic trend)") << "\n\n";

  // CUSUM
  std::cout << "Cumulative Sum Analysis:\n";
  std::cout << "  Max |CUSUM|:    " << metrics.max_cusum << "\n";
  std::cout << "  Final CUSUM:    " << metrics.final_cusum
            << (std::abs(metrics.final_cusum) > metrics.overall_rmse ?
                " (trending)" : " (balanced)") << "\n\n";

  // Overall classification
  std::cout << "=== CLASSIFICATION ===\n";
  std::cout << "Pattern: " << get_pattern_name(metrics.pattern) << "\n";
  std::cout << "Interpretation:\n" << metrics.interpretation << "\n\n";

  // Recommendation
  print_recommendation(metrics);
}

void FileComparator::print_recommendation(
    const AccumulationMetrics& metrics) const {
  std::cout << "=== RECOMMENDATION ===\n";

  switch (metrics.pattern) {
    case ErrorPattern::SYSTEMATIC_GROWTH:
      std::cout << "⚠️  ATTENTION REQUIRED\n";
      std::cout << "Error grows with range (slope = " << metrics.slope << ").\n";
      std::cout << "This suggests a systematic difference in propagation models.\n";
      std::cout << "Action: Investigate model physics or numerical methods.\n";
      break;

    case ErrorPattern::SYSTEMATIC_BIAS:
      std::cout << "⚠️  CALIBRATION ISSUE\n";
      std::cout << "Fixed offset detected (intercept = " << metrics.intercept << ").\n";
      std::cout << "Models may differ by a constant factor or scaling.\n";
      std::cout << "Action: Check input parameters, units, or reference values.\n";
      break;

    case ErrorPattern::RANDOM_NOISE:
      std::cout << "✓ ACCEPTABLE\n";
      std::cout << "Errors appear random and uncorrelated.\n";
      std::cout << "Likely due to platform differences or numerical precision.\n";
      std::cout << "Action: Current 2% threshold may be appropriate.\n";
      break;

    case ErrorPattern::NULL_POINT_NOISE:
      std::cout << "✓ BENIGN\n";
      std::cout << "Errors concentrated at null points (low signal/high TL).\n";
      std::cout << "These are numerically unstable regions and can be ignored.\n";
      std::cout << "Action: Errors are operationally insignificant.\n";
      break;

    case ErrorPattern::TRANSIENT_SPIKES:
      std::cout << "⚠️  INVESTIGATE SPIKES\n";
      std::cout << "Isolated large errors detected.\n";
      std::cout << "May indicate numerical instabilities or convergence issues.\n";
      std::cout << "Action: Examine specific range bins with large errors.\n";
      break;

    case ErrorPattern::OSCILLATING:
      std::cout << "⚠️  NUMERICAL INSTABILITY\n";
      std::cout << "Errors show oscillating pattern (negative autocorrelation).\n";
      std::cout << "May indicate step-size or grid resolution issues.\n";
      std::cout << "Action: Review numerical parameters (Δr, Δz, etc.).\n";
      break;
  }
  std::cout << "\n";
}
```

## Example Output

```
=== ERROR ACCUMULATION ANALYSIS ===
Range: 0.5 km to 150 km
Data points: 512

Linear Regression (error vs range):
  Slope:       0.00234 dB/km (error per unit range)
  Intercept:   0.089 dB
  R²:          0.89 (strong fit)
  P-value:     0.00012 (SIGNIFICANT)

Autocorrelation Analysis:
  Lag-1 correlation: 0.78
  Status: CORRELATED (systematic pattern)

Run Test (randomness):
  Observed runs:  12
  Expected runs:  87
  Z-score:        -8.2
  Status: NON-RANDOM (systematic trend)

Cumulative Sum Analysis:
  Max |CUSUM|:    47.3 dB
  Final CUSUM:    45.1 dB (trending)

=== CLASSIFICATION ===
Pattern: SYSTEMATIC_GROWTH
Interpretation:
  Errors increase linearly with range at a rate of 0.00234 dB/km.
  This is consistent with accumulated propagation error or differing
  attenuation models. The high R² (0.89) and strong autocorrelation (0.78)
  indicate this is NOT random noise.

=== RECOMMENDATION ===
⚠️  ATTENTION REQUIRED
Error grows with range (slope = 0.00234).
This suggests a systematic difference in propagation models.
Action: Investigate model physics or numerical methods.
```

## Implementation Strategy

### Step 1: Minimal Viable Product (MVP)

Start with just the **slope test**:

1. Collect `(range, error)` pairs during comparison
2. After comparison, compute linear regression
3. Report slope and R²
4. Simple decision: `if (abs(slope) > threshold && R² > 0.5) → SYSTEMATIC`

### Step 2: Add Statistical Tests

Add autocorrelation and run test:
- Both are simple to implement (< 50 lines each)
- Provide strong evidence for randomness vs systematic

### Step 3: Full Analysis Suite

Add CUSUM and windowed RMSE for comprehensive analysis

### Step 4: Machine Learning (Future)

Train a classifier on known good/bad test cases:
- Features: slope, R², autocorr, runs, CUSUM
- Labels: "IGNORE" vs "INVESTIGATE"
- Replace hand-waving 2% with data-driven threshold

## Integration Points

### Where to collect data?

In `FileComparator::process_difference()`:

```cpp
// After calculating raw_diff
if (flag.column1_is_range_data && column_index > 0) {
  // Column 1 is range, column >0 is TL
  accumulation_data_.ranges.push_back(column_data.range);
  accumulation_data_.errors.push_back(raw_diff);
  accumulation_data_.tl_values_ref.push_back(column_data.value1);
  accumulation_data_.tl_values_test.push_back(column_data.value2);
  accumulation_data_.is_significant.push_back(
      raw_diff > thresh.significant);
  accumulation_data_.n_points++;
}
```

### Where to analyze?

In `FileComparator::print_summary()`:

```cpp
// After existing summary
if (flag.column1_is_range_data && accumulation_data_.n_points >= 10) {
  print_accumulation_analysis();
}
```

## Benefits

1. **Objective Classification**: Replace "2% threshold" with data-driven metrics
2. **Operational Insight**: Distinguish noise from real model differences
3. **Diagnostic Power**: Identify specific error patterns (bias, growth, oscillation)
4. **Automated Validation**: Could trigger different exit codes based on pattern
5. **Research Value**: Quantitative metrics for model comparison papers

## References

1. **Acoustic propagation validation:**
   - Porter, M.B. (2011). "The BELLHOP Manual and User's Guide"
   - Collins, M.D. (1993). "A split-step Padé solution for the parabolic equation method"

2. **Statistical tests:**
   - Wald, A., & Wolfowitz, J. (1940). "On a test whether two samples are from the same population"
   - Box, G.E.P., & Jenkins, G.M. (1976). "Time Series Analysis: Forecasting and Control"

3. **Numerical analysis:**
   - Press, W.H., et al. (2007). "Numerical Recipes" (Chapter 15: Modeling of Data)
   - Wilkinson, L. (1999). "The Grammar of Graphics" (visualization of error patterns)

## Next Steps

Would you like me to:
1. Implement the MVP (linear regression slope test)?
2. Create the full `ErrorAccumulationAnalyzer` class?
3. Add data collection hooks to `FileComparator`?
4. Build a test case with synthetic data showing systematic vs random errors?

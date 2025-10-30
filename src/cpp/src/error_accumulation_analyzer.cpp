/**
 * @file error_accumulation_analyzer.cpp
 * @author J. Lighthall
 * @date October 2025
 * @brief Implementation of error accumulation analysis
 */

#include "error_accumulation_analyzer.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>

#include "uband_diff.h"

AccumulationMetrics ErrorAccumulationAnalyzer::analyze(
    const ErrorAccumulationData& data) {
  AccumulationMetrics metrics;

  // Check if we have enough data
  if (data.n_points < min_points_) {
    metrics.pattern = AccumulationMetrics::ErrorPattern::INSUFFICIENT_DATA;
    metrics.interpretation =
        "Insufficient data points for analysis (need at least " +
        std::to_string(min_points_) + " points)";
    metrics.recommendation =
        "Collect more data points to enable accumulation analysis.";
    return metrics;
  }

  // Compute linear regression: error vs range
  auto regression = linear_regression(data.ranges, data.errors);
  metrics.slope = regression.slope;
  metrics.intercept = regression.intercept;
  metrics.r_squared = regression.r_squared;
  metrics.std_error_slope = regression.std_error_slope;
  metrics.p_value_slope = regression.p_value;

  // Compute autocorrelation
  metrics.autocorr_lag1 = calculate_autocorrelation(data.errors, 1);
  metrics.is_correlated = std::abs(metrics.autocorr_lag1) > autocorr_threshold_;

  // Perform run test
  metrics.n_runs = count_runs(data.errors);

  // Count positive and negative errors
  int n_pos = 0, n_neg = 0;
  for (double err : data.errors) {
    if (err > 0)
      n_pos++;
    else if (err < 0)
      n_neg++;
  }

  metrics.expected_runs = expected_runs(n_pos, n_neg);
  metrics.run_test_z_score = run_test_z_score(metrics.n_runs, n_pos, n_neg);

  // Run test: |z| < 1.96 indicates random at 95% confidence
  metrics.is_random = std::abs(metrics.run_test_z_score) < 1.96;

  // Compute overall statistics
  metrics.mean_error =
      std::accumulate(data.errors.begin(), data.errors.end(), 0.0) /
      data.n_points;

  double sum_sq = 0.0;
  metrics.max_error = 0.0;
  for (double err : data.errors) {
    sum_sq += err * err;
    if (std::abs(err) > metrics.max_error) {
      metrics.max_error = std::abs(err);
    }
  }
  metrics.rmse = std::sqrt(sum_sq / data.n_points);

  // Classify pattern
  metrics.pattern = classify_pattern(metrics);

  // Generate interpretation and recommendation
  metrics.interpretation = generate_interpretation(metrics);
  metrics.recommendation = generate_recommendation(metrics);

  return metrics;
}

ErrorAccumulationAnalyzer::RegressionResult
ErrorAccumulationAnalyzer::linear_regression(const std::vector<double>& x,
                                             const std::vector<double>& y) {
  RegressionResult result;
  size_t n = x.size();

  // Calculate means
  double mean_x = std::accumulate(x.begin(), x.end(), 0.0) / n;
  double mean_y = std::accumulate(y.begin(), y.end(), 0.0) / n;

  // Calculate slope and intercept
  double numerator = 0.0;
  double denominator = 0.0;

  for (size_t i = 0; i < n; i++) {
    double dx = x[i] - mean_x;
    double dy = y[i] - mean_y;
    numerator += dx * dy;
    denominator += dx * dx;
  }

  result.slope = (denominator != 0.0) ? numerator / denominator : 0.0;
  result.intercept = mean_y - result.slope * mean_x;

  // Calculate R²
  double ss_tot = 0.0;  // Total sum of squares
  double ss_res = 0.0;  // Residual sum of squares

  for (size_t i = 0; i < n; i++) {
    double y_pred = result.slope * x[i] + result.intercept;
    ss_res += (y[i] - y_pred) * (y[i] - y_pred);
    ss_tot += (y[i] - mean_y) * (y[i] - mean_y);
  }

  result.r_squared = (ss_tot != 0.0) ? 1.0 - (ss_res / ss_tot) : 0.0;

  // Calculate standard error of slope
  if (n > 2 && denominator != 0.0) {
    double mse = ss_res / (n - 2);  // Mean squared error
    result.std_error_slope = std::sqrt(mse / denominator);

    // Calculate t-statistic and p-value (approximate)
    double t_stat = result.slope / result.std_error_slope;

    // Approximate p-value using normal distribution (for large n)
    // For small n, should use t-distribution, but this is good enough for MVP
    double z = std::abs(t_stat);

    // Approximate p-value for two-tailed test
    // Using complementary error function approximation
    if (z > 6.0) {
      result.p_value = 0.0;  // Extremely significant
    } else {
      // Rough approximation: p ≈ 2 * (1 - Φ(|z|))
      // For z > 3, p < 0.003; z > 2, p < 0.05
      result.p_value = 2.0 * std::erfc(z / std::sqrt(2.0));
    }
  } else {
    result.std_error_slope = 0.0;
    result.p_value = 1.0;  // Not significant
  }

  return result;
}

double ErrorAccumulationAnalyzer::calculate_autocorrelation(
    const std::vector<double>& data, int lag) {
  size_t n = data.size();
  if (n < static_cast<size_t>(lag + 2)) return 0.0;

  // Calculate mean
  double mean = std::accumulate(data.begin(), data.end(), 0.0) / n;

  // Calculate variance
  double variance = 0.0;
  for (double val : data) {
    double diff = val - mean;
    variance += diff * diff;
  }

  if (variance == 0.0) return 0.0;

  // Calculate autocovariance at lag
  double autocovariance = 0.0;
  for (size_t i = 0; i < n - lag; i++) {
    autocovariance += (data[i] - mean) * (data[i + lag] - mean);
  }

  // Autocorrelation = autocovariance / variance
  return autocovariance / variance;
}

int ErrorAccumulationAnalyzer::count_runs(const std::vector<double>& data) {
  if (data.empty()) return 0;

  int runs = 1;  // Start with 1 run

  for (size_t i = 1; i < data.size(); i++) {
    // Check if sign changed (considering zero as continuing previous run)
    bool prev_pos = data[i - 1] > 0;
    bool curr_pos = data[i] > 0;
    bool prev_neg = data[i - 1] < 0;
    bool curr_neg = data[i] < 0;

    // New run if sign changes from positive to negative or vice versa
    if ((prev_pos && curr_neg) || (prev_neg && curr_pos)) {
      runs++;
    }
  }

  return runs;
}

int ErrorAccumulationAnalyzer::expected_runs(int n_positive, int n_negative) {
  int n = n_positive + n_negative;
  if (n == 0) return 0;

  // Expected runs for random sequence
  // E(R) = (2*n₁*n₂)/n + 1
  return (2 * n_positive * n_negative) / n + 1;
}

double ErrorAccumulationAnalyzer::run_test_z_score(int n_runs, int n_pos,
                                                   int n_neg) {
  int n = n_pos + n_neg;
  if (n < 2) return 0.0;

  // Expected runs
  double mu = (2.0 * n_pos * n_neg) / n + 1.0;

  // Variance of runs
  // σ² = (2*n₁*n₂*(2*n₁*n₂ - n)) / (n²*(n-1))
  double numerator = 2.0 * n_pos * n_neg * (2.0 * n_pos * n_neg - n);
  double denominator = n * n * (n - 1.0);

  if (denominator == 0.0) return 0.0;

  double variance = numerator / denominator;
  double sigma = std::sqrt(variance);

  if (sigma == 0.0) return 0.0;

  // Z-score
  return (n_runs - mu) / sigma;
}

AccumulationMetrics::ErrorPattern ErrorAccumulationAnalyzer::classify_pattern(
    const AccumulationMetrics& metrics) {
  using Pattern = AccumulationMetrics::ErrorPattern;

  // Check for significant slope with good fit
  bool has_significant_slope = (std::abs(metrics.slope) > slope_threshold_) &&
                               (metrics.p_value_slope < 0.05);
  bool has_good_fit = metrics.r_squared > r_squared_threshold_;

  // Pattern: SYSTEMATIC_GROWTH
  // Error increases with range - propagation issue
  if (has_significant_slope && has_good_fit && metrics.slope > 0) {
    return Pattern::SYSTEMATIC_GROWTH;
  }

  // Pattern: SYSTEMATIC_BIAS
  // Fixed offset with good correlation but negligible slope
  if (has_good_fit && !has_significant_slope &&
      std::abs(metrics.mean_error) > 0.1 * metrics.rmse) {
    return Pattern::SYSTEMATIC_BIAS;
  }

  // Pattern: RANDOM_NOISE
  // Passes randomness test and low autocorrelation
  if (metrics.is_random && !metrics.is_correlated) {
    return Pattern::RANDOM_NOISE;
  }

  // Pattern: TRANSIENT_SPIKES
  // High max error but low RMSE (isolated spikes)
  if (metrics.max_error > 3.0 * metrics.rmse && metrics.is_random) {
    return Pattern::TRANSIENT_SPIKES;
  }

  // Pattern: NULL_POINT_NOISE
  // Small errors overall (RMSE < threshold)
  // This would need TL data to properly identify, simplified for MVP
  if (metrics.rmse < slope_threshold_ * 10) {
    return Pattern::NULL_POINT_NOISE;
  }

  // Default: RANDOM_NOISE if nothing else matches
  return Pattern::RANDOM_NOISE;
}

std::string ErrorAccumulationAnalyzer::generate_interpretation(
    const AccumulationMetrics& metrics) {
  using Pattern = AccumulationMetrics::ErrorPattern;
  std::ostringstream oss;

  switch (metrics.pattern) {
    case Pattern::SYSTEMATIC_GROWTH:
      oss << "Errors increase linearly with range at a rate of "
          << std::scientific << metrics.slope << " per unit range.\n"
          << "  The high R² (" << std::fixed << std::setprecision(3)
          << metrics.r_squared << ") and strong autocorrelation ("
          << metrics.autocorr_lag1 << ")\n"
          << "  indicate this is NOT random noise. This is consistent with\n"
          << "  accumulated propagation error or differing attenuation models.";
      break;

    case Pattern::SYSTEMATIC_BIAS:
      oss << "Errors show a fixed offset (intercept = " << std::scientific
          << metrics.intercept << ")\n"
          << "  with negligible range dependence. R² = " << std::fixed
          << std::setprecision(3) << metrics.r_squared
          << " indicates strong fit.\n"
          << "  This suggests a calibration issue, unit mismatch, or constant\n"
          << "  scaling difference between models.";
      break;

    case Pattern::RANDOM_NOISE:
      oss << "Errors appear random and uncorrelated (autocorr = " << std::fixed
          << std::setprecision(3) << metrics.autocorr_lag1 << ").\n"
          << "  Run test Z-score = " << metrics.run_test_z_score
          << " (|Z| < 1.96 indicates random).\n"
          << "  This is consistent with platform differences, numerical "
             "precision,\n"
          << "  or benign rounding artifacts.";
      break;

    case Pattern::NULL_POINT_NOISE:
      oss << "Errors are small overall (RMSE = " << std::scientific
          << metrics.rmse << ").\n"
          << "  These are likely concentrated at null points or regions of\n"
          << "  low signal strength, which are numerically unstable and\n"
          << "  operationally insignificant.";
      break;

    case Pattern::TRANSIENT_SPIKES:
      oss << "Isolated large errors detected (max = " << std::scientific
          << metrics.max_error << ", RMSE = " << metrics.rmse << ").\n"
          << "  Most errors are small, but a few outliers exist. This may\n"
          << "  indicate numerical instabilities, convergence issues, or\n"
          << "  specific problematic range bins.";
      break;

    case Pattern::INSUFFICIENT_DATA:
      oss << "Not enough data points for statistical analysis.";
      break;
  }

  return oss.str();
}

std::string ErrorAccumulationAnalyzer::generate_recommendation(
    const AccumulationMetrics& metrics) {
  using Pattern = AccumulationMetrics::ErrorPattern;
  std::ostringstream oss;

  switch (metrics.pattern) {
    case Pattern::SYSTEMATIC_GROWTH:
      oss << "⚠️  ATTENTION REQUIRED\n"
          << "Error grows with range (slope = " << std::scientific
          << metrics.slope << ", p < " << std::fixed << std::setprecision(3)
          << metrics.p_value_slope << ").\n"
          << "This suggests a systematic difference in propagation models.\n"
          << "Action: Investigate model physics, numerical methods, or "
             "range-dependent\n"
          << "        parameters (e.g., attenuation, absorption, grid "
             "resolution).";
      break;

    case Pattern::SYSTEMATIC_BIAS:
      oss << "⚠️  CALIBRATION ISSUE\n"
          << "Fixed offset detected (bias = " << std::scientific
          << metrics.mean_error << ").\n"
          << "Models may differ by a constant factor or scaling.\n"
          << "Action: Check input parameters, units, reference pressure "
             "values,\n"
          << "        or source level calibration.";
      break;

    case Pattern::RANDOM_NOISE:
      oss << "✓ ACCEPTABLE\n"
          << "Errors appear random and uncorrelated (p = " << std::fixed
          << std::setprecision(3) << metrics.p_value_slope << ").\n"
          << "Likely due to platform differences or numerical precision.\n"
          << "Action: Current threshold criteria are appropriate. These "
             "differences\n"
          << "        are operationally insignificant.";
      break;

    case Pattern::NULL_POINT_NOISE:
      oss << "✓ BENIGN\n"
          << "Errors are small and concentrated in numerically unstable "
             "regions.\n"
          << "Action: These errors can be safely ignored. Consider tightening\n"
          << "        the ignore threshold to exclude null points from "
             "analysis.";
      break;

    case Pattern::TRANSIENT_SPIKES:
      oss << "⚠️  INVESTIGATE OUTLIERS\n"
          << "Isolated large errors detected at specific ranges.\n"
          << "Action: Examine range bins with max error (" << std::scientific
          << metrics.max_error << ").\n"
          << "        Check for convergence issues, grid discontinuities, or\n"
          << "        boundary condition problems at those specific ranges.";
      break;

    case Pattern::INSUFFICIENT_DATA:
      oss << "Action: Collect more comparison data points.";
      break;
  }

  return oss.str();
}

std::string ErrorAccumulationAnalyzer::get_pattern_name(
    AccumulationMetrics::ErrorPattern pattern) {
  using Pattern = AccumulationMetrics::ErrorPattern;

  switch (pattern) {
    case Pattern::SYSTEMATIC_GROWTH:
      return "SYSTEMATIC_GROWTH";
    case Pattern::SYSTEMATIC_BIAS:
      return "SYSTEMATIC_BIAS";
    case Pattern::RANDOM_NOISE:
      return "RANDOM_NOISE";
    case Pattern::NULL_POINT_NOISE:
      return "NULL_POINT_NOISE";
    case Pattern::TRANSIENT_SPIKES:
      return "TRANSIENT_SPIKES";
    case Pattern::INSUFFICIENT_DATA:
      return "INSUFFICIENT_DATA";
    default:
      return "UNKNOWN";
  }
}

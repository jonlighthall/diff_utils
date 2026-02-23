/**
 * @file test_tl_analysis.cpp
 * @brief Unit tests for error accumulation analysis (tl_analysis)
 *
 * @author J. Lighthall
 * @date February 2026
 */

#include <gtest/gtest.h>

#include <cmath>
#include <numeric>
#include <vector>

#include "tl_analysis.h"

// ============================================================================
// ErrorAccumulationData tests
// ============================================================================

TEST(ErrorAccumulationDataTest, AddPointUpdatesMetadata) {
  ErrorAccumulationData data;
  data.add_point(10.0, 0.5, 70.0, 70.5, false);
  data.add_point(20.0, -0.3, 72.0, 71.7, false);
  data.add_point(30.0, 1.2, 75.0, 76.2, true);

  EXPECT_EQ(data.n_points, 3u);
  EXPECT_DOUBLE_EQ(data.range_min, 10.0);
  EXPECT_DOUBLE_EQ(data.range_max, 30.0);
  EXPECT_EQ(data.ranges.size(), 3u);
  EXPECT_EQ(data.errors.size(), 3u);
  EXPECT_EQ(data.is_significant.size(), 3u);
}

TEST(ErrorAccumulationDataTest, ClearResetsEverything) {
  ErrorAccumulationData data;
  data.add_point(10.0, 0.5, 70.0, 70.5, false);
  data.add_point(20.0, -0.3, 72.0, 71.7, false);
  data.clear();

  EXPECT_EQ(data.n_points, 0u);
  EXPECT_TRUE(data.ranges.empty());
  EXPECT_TRUE(data.errors.empty());
  EXPECT_EQ(data.range_min, std::numeric_limits<double>::max());
  EXPECT_EQ(data.range_max, std::numeric_limits<double>::lowest());
}

// ============================================================================
// Insufficient data
// ============================================================================

TEST(ErrorAccumulationAnalyzerTest, InsufficientData) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Add fewer than 10 points (default minimum)
  for (int i = 0; i < 5; ++i) {
    data.add_point(i * 10.0, 0.1, 70.0, 70.1, false);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);
  EXPECT_EQ(metrics.pattern,
            AccumulationMetrics::ErrorPattern::INSUFFICIENT_DATA);
}

TEST(ErrorAccumulationAnalyzerTest, CustomMinPoints) {
  ErrorAccumulationAnalyzer analyzer;
  analyzer.set_min_points(3);

  ErrorAccumulationData data;
  for (int i = 0; i < 5; ++i) {
    data.add_point(i * 10.0, 0.1, 70.0, 70.1, false);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);
  // With min_points=3, 5 points should be sufficient
  EXPECT_NE(metrics.pattern,
            AccumulationMetrics::ErrorPattern::INSUFFICIENT_DATA);
}

// ============================================================================
// Linear regression
// ============================================================================

TEST(ErrorAccumulationAnalyzerTest, PerfectLinearGrowth) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Error grows linearly with range: error = 0.01 * range
  for (int i = 0; i < 100; ++i) {
    double range = static_cast<double>(i);
    double error = 0.01 * range;
    data.add_point(range, error, 70.0, 70.0 + error, error > 0.05);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);

  // Should detect significant slope
  EXPECT_GT(metrics.slope, 0.0);
  EXPECT_NEAR(metrics.r_squared, 1.0, 0.01);
  EXPECT_LT(metrics.p_value_slope, 0.05);
}

TEST(ErrorAccumulationAnalyzerTest, ZeroSlope) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Constant error (no range dependence)
  for (int i = 0; i < 50; ++i) {
    double range = static_cast<double>(i);
    data.add_point(range, 1.0, 70.0, 71.0, true);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);

  // Slope should be zero
  EXPECT_NEAR(metrics.slope, 0.0, 1e-10);
}

// ============================================================================
// Random noise classification
// ============================================================================

TEST(ErrorAccumulationAnalyzerTest, AlternatingErrors) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Alternating positive/negative errors — many runs, looks random
  for (int i = 0; i < 100; ++i) {
    double range = static_cast<double>(i);
    double error = (i % 2 == 0) ? 0.1 : -0.1;
    data.add_point(range, error, 70.0, 70.0 + error, false);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);

  // Should have many runs (100 sign changes)
  EXPECT_GE(metrics.n_runs, 90);
  // Mean error should be near zero
  EXPECT_NEAR(metrics.mean_error, 0.0, 0.01);
}

// ============================================================================
// Pattern name strings
// ============================================================================

TEST(ErrorAccumulationAnalyzerTest, PatternNames) {
  using Pattern = AccumulationMetrics::ErrorPattern;

  EXPECT_EQ(
      ErrorAccumulationAnalyzer::get_pattern_name(Pattern::SYSTEMATIC_GROWTH),
      "SYSTEMATIC_GROWTH");
  EXPECT_EQ(
      ErrorAccumulationAnalyzer::get_pattern_name(Pattern::SYSTEMATIC_BIAS),
      "SYSTEMATIC_BIAS");
  EXPECT_EQ(ErrorAccumulationAnalyzer::get_pattern_name(Pattern::RANDOM_NOISE),
            "RANDOM_NOISE");
  EXPECT_EQ(
      ErrorAccumulationAnalyzer::get_pattern_name(Pattern::NULL_POINT_NOISE),
      "NULL_POINT_NOISE");
  EXPECT_EQ(
      ErrorAccumulationAnalyzer::get_pattern_name(Pattern::TRANSIENT_SPIKES),
      "TRANSIENT_SPIKES");
  EXPECT_EQ(
      ErrorAccumulationAnalyzer::get_pattern_name(Pattern::INSUFFICIENT_DATA),
      "INSUFFICIENT_DATA");
}

// ============================================================================
// Overall statistics
// ============================================================================

TEST(ErrorAccumulationAnalyzerTest, StatisticsCorrect) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Known errors: 1, 2, 3, 4, 5, 6, 7, 8, 9, 10
  for (int i = 1; i <= 10; ++i) {
    double err = static_cast<double>(i);
    data.add_point(i * 10.0, err, 70.0, 70.0 + err, true);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);

  // Mean error = 5.5
  EXPECT_NEAR(metrics.mean_error, 5.5, 1e-10);
  // Max error = 10
  EXPECT_NEAR(metrics.max_error, 10.0, 1e-10);
  // RMSE = sqrt(mean(1² + 2² + ... + 10²)) = sqrt(38.5) ≈ 6.2048
  double expected_rmse = std::sqrt(385.0 / 10.0);
  EXPECT_NEAR(metrics.rmse, expected_rmse, 1e-10);
}

// ============================================================================
// Autocorrelation
// ============================================================================

TEST(ErrorAccumulationAnalyzerTest, HighAutocorrelationDetected) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Smoothly increasing errors — high autocorrelation
  for (int i = 0; i < 50; ++i) {
    double range = static_cast<double>(i);
    double error = std::sin(range * 0.1);  // slow oscillation → high autocorr
    data.add_point(range, error, 70.0, 70.0 + error, false);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);

  // Slowly varying signal → high lag-1 autocorrelation
  EXPECT_GT(metrics.autocorr_lag1, 0.5);
  EXPECT_TRUE(metrics.is_correlated);
}

// ============================================================================
// Run test
// ============================================================================

TEST(ErrorAccumulationAnalyzerTest, RunTestManyRuns) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Alternating +/- → maximum number of runs
  for (int i = 0; i < 100; ++i) {
    double error = (i % 2 == 0) ? 1.0 : -1.0;
    data.add_point(i * 1.0, error, 70.0, 70.0 + error, false);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);

  // 100 sign alternations → 100 runs
  EXPECT_EQ(metrics.n_runs, 100);
}

TEST(ErrorAccumulationAnalyzerTest, RunTestFewRuns) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // 50 positive then 50 negative → only 2 runs
  for (int i = 0; i < 100; ++i) {
    double error = (i < 50) ? 1.0 : -1.0;
    data.add_point(i * 1.0, error, 70.0, 70.0 + error, false);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);

  EXPECT_EQ(metrics.n_runs, 2);
  // Very few runs → not random
  EXPECT_FALSE(metrics.is_random);
}

// ============================================================================
// Systematic growth classification
// ============================================================================

TEST(ErrorAccumulationAnalyzerTest, ClassifiesSystematicGrowth) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Error grows linearly with range
  for (int i = 0; i < 100; ++i) {
    double range = static_cast<double>(i);
    double error = 0.01 * range;
    data.add_point(range, error, 70.0, 70.0 + error, error > 0.05);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);

  EXPECT_EQ(metrics.pattern,
            AccumulationMetrics::ErrorPattern::SYSTEMATIC_GROWTH);
}

// ============================================================================
// Set thresholds
// ============================================================================

TEST(ErrorAccumulationAnalyzerTest, SetThresholds) {
  ErrorAccumulationAnalyzer analyzer;

  // Should not throw
  EXPECT_NO_THROW(analyzer.set_thresholds(0.01, 0.7, 0.6));
  EXPECT_NO_THROW(analyzer.set_min_points(20));
}

// ============================================================================
// Interpretation and recommendation strings are non-empty
// ============================================================================

TEST(ErrorAccumulationAnalyzerTest, InterpretationNonEmpty) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  for (int i = 0; i < 20; ++i) {
    double error = (i % 2 == 0) ? 0.1 : -0.1;
    data.add_point(i * 10.0, error, 70.0, 70.0 + error, false);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);

  EXPECT_FALSE(metrics.interpretation.empty());
  EXPECT_FALSE(metrics.recommendation.empty());
}

// ============================================================================
// Verification criteria tests
// ============================================================================

// Simulate the three-criteria verification logic used by --verify mode.
// This mirrors the decision logic in main/tl_analysis.cpp.
// The quantization step (lsb) defaults to 0 for synthetic tests,
// meaning no quantization tolerance is applied unless specified.
struct VerifyResult {
  bool rmse_pass;
  bool spike_pass;
  bool trend_pass;
  bool all_pass;
};

static VerifyResult run_verify(const AccumulationMetrics& m,
                               double rmse_limit = 0.01,
                               double spike_factor = 10.0,
                               double lsb = 0.0,
                               double range_extent = 0.0) {
  VerifyResult v;
  v.rmse_pass = m.rmse < rmse_limit;
  // Spike: max_error must exceed both ratio threshold AND the RMSE tolerance
  double spike_limit = std::max(spike_factor * m.rmse, rmse_limit);
  v.spike_pass = (m.rmse == 0.0) || (m.max_error < spike_limit);
  // Trend: sub-LSB drift is not meaningful
  double total_drift = (range_extent > 0.0) ?
      std::abs(m.slope) * range_extent : 0.0;
  v.trend_pass = m.p_value_slope > 0.05 || std::isnan(m.p_value_slope) ||
                 (lsb > 0 && total_drift < lsb);
  v.all_pass = v.rmse_pass && v.spike_pass && v.trend_pass;
  return v;
}

TEST(VerificationTest, IdenticalCurvesPass) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Zero error everywhere
  for (int i = 0; i < 50; ++i) {
    data.add_point(i * 1.0, 0.0, 70.0, 70.0, false);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);
  auto v = run_verify(metrics);

  EXPECT_TRUE(v.rmse_pass);
  EXPECT_TRUE(v.spike_pass);
  EXPECT_TRUE(v.trend_pass);
  EXPECT_TRUE(v.all_pass);
}

TEST(VerificationTest, TinyNoisePasses) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Floating-point-level noise: alternating ± 0.001 dB
  for (int i = 0; i < 100; ++i) {
    double error = (i % 2 == 0) ? 0.001 : -0.001;
    data.add_point(i * 1.0, error, 70.0, 70.0 + error, false);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);
  auto v = run_verify(metrics);

  EXPECT_LT(metrics.rmse, 0.01);
  EXPECT_TRUE(v.rmse_pass);
  EXPECT_TRUE(v.all_pass);
}

TEST(VerificationTest, LargeRMSEFails) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // 1 dB errors — way above 0.01 dB tolerance
  for (int i = 0; i < 50; ++i) {
    double error = (i % 2 == 0) ? 1.0 : -1.0;
    data.add_point(i * 1.0, error, 70.0, 70.0 + error, true);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);
  auto v = run_verify(metrics);

  EXPECT_GT(metrics.rmse, 0.01);
  EXPECT_FALSE(v.rmse_pass);
  EXPECT_FALSE(v.all_pass);
}

TEST(VerificationTest, AnomalousSpikeFailsTest2) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Mostly tiny errors, one huge spike at a much higher ratio
  for (int i = 0; i < 200; ++i) {
    double error = 0.001;
    if (i == 100) error = 10.0;  // single massive spike
    data.add_point(i * 1.0, error, 70.0, 70.0 + error, error > 0.05);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);
  // Use rmse_limit=1.0 so Test 1 passes (RMSE ≈ 0.71) but spike_limit is
  // dominated by spike_factor * RMSE ≈ 7.07, which max_error (10.0) exceeds.
  auto v = run_verify(metrics, 1.0, 10.0);

  // max_error / RMSE should be well above 10 with 200 points diluting the spike
  EXPECT_GT(metrics.max_error / metrics.rmse, 10.0);
  EXPECT_FALSE(v.spike_pass);
}

TEST(VerificationTest, SystematicTrendFailsTest3) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Error grows linearly with range
  for (int i = 0; i < 100; ++i) {
    double range = static_cast<double>(i);
    double error = 0.0001 * range;  // small but systematic
    data.add_point(range, error, 70.0, 70.0 + error, false);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);
  auto v = run_verify(metrics, 1.0);  // very loose RMSE to isolate trend test

  // Should detect significant slope
  EXPECT_LT(metrics.p_value_slope, 0.05);
  EXPECT_FALSE(v.trend_pass);
}

TEST(VerificationTest, CustomRMSELimit) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // 0.05 dB errors — fails at 0.01, passes at 0.1
  for (int i = 0; i < 50; ++i) {
    double error = (i % 2 == 0) ? 0.05 : -0.05;
    data.add_point(i * 1.0, error, 70.0, 70.0 + error, false);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);

  auto strict = run_verify(metrics, 0.01);
  auto loose = run_verify(metrics, 0.1);

  EXPECT_FALSE(strict.rmse_pass);
  EXPECT_TRUE(loose.rmse_pass);
}

// ============================================================================
// Quantization-aware verification tests
// ============================================================================

TEST(QuantizationTest, SubLSBSpikePassesWithQuantizationFloor) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Simulate 1-decimal data: most points match, a few have 0.1 dB rounding diff
  // RMSE is small, so max/RMSE ratio is high, but max is within rmse_limit
  for (int i = 0; i < 200; ++i) {
    double error = 0.0;
    if (i == 50 || i == 100) error = 0.1;  // single-LSB rounding differences
    data.add_point(i * 0.5, error, 70.0, 70.0 + error, false);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);

  // With auto-calibrated rmse_limit for 1-decimal data
  double rmse_limit = 0.122;  // 3-sigma for 1-decimal data
  auto v = run_verify(metrics, rmse_limit, 10.0);

  EXPECT_LT(metrics.rmse, rmse_limit);                // RMSE passes
  EXPECT_GE(metrics.max_error / metrics.rmse, 10.0);   // ratio >= 10
  EXPECT_TRUE(v.spike_pass);  // passes because max < rmse_limit
  EXPECT_TRUE(v.all_pass);
}

TEST(QuantizationTest, SubLSBTrendPassesWithDriftCheck) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Small systematic slope that is statistically significant but sub-LSB total
  for (int i = 0; i < 200; ++i) {
    double range = static_cast<double>(i);
    double error = 0.0001 * range;  // slope 1e-4 dB/unit
    data.add_point(range, error, 70.0, 70.0 + error, false);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);

  // Without drift check: p < 0.05 → FAIL
  EXPECT_LT(metrics.p_value_slope, 0.05);

  // With drift check: total drift = 0.0001 * 199 = 0.02 dB < 0.1 LSB
  double lsb = 0.1;
  double range_extent = 199.0;
  auto v = run_verify(metrics, 1.0, 10.0, lsb, range_extent);

  EXPECT_TRUE(v.trend_pass);  // passes because total drift < LSB
}

TEST(QuantizationTest, RealTrendStillFails) {
  ErrorAccumulationAnalyzer analyzer;
  ErrorAccumulationData data;

  // Large systematic slope: total drift exceeds LSB
  for (int i = 0; i < 200; ++i) {
    double range = static_cast<double>(i);
    double error = 0.01 * range;  // slope 0.01 dB/unit, total drift = 2 dB
    data.add_point(range, error, 70.0, 70.0 + error, false);
  }

  AccumulationMetrics metrics = analyzer.analyze(data);

  double lsb = 0.1;
  double range_extent = 199.0;
  auto v = run_verify(metrics, 100.0, 10.0, lsb, range_extent);

  // Total drift = 0.01 * 199 = 1.99 dB >> 0.1 LSB
  EXPECT_LT(metrics.p_value_slope, 0.05);
  EXPECT_FALSE(v.trend_pass);  // fails because drift exceeds LSB
}

TEST(QuantizationTest, ComputeQuantizationRMSELimit) {
  // Verify the mathematical formula: 3 * 10^(-d) / sqrt(6)
  // For 1 decimal: 3 * 0.1 / sqrt(6) = 0.3 / 2.449 ≈ 0.1225
  // For 2 decimals: 3 * 0.01 / sqrt(6) ≈ 0.01225
  // For 0 decimals: 3 * 1.0 / sqrt(6) ≈ 1.225

  auto compute = [](int d) {
    return 3.0 * std::pow(10.0, -d) / std::sqrt(6.0);
  };

  EXPECT_NEAR(compute(0), 1.2247, 0.001);
  EXPECT_NEAR(compute(1), 0.1225, 0.001);
  EXPECT_NEAR(compute(2), 0.01225, 0.001);
  EXPECT_NEAR(compute(3), 0.001225, 0.0001);
}

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

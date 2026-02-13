/**
 * @file test_tl_metric.cpp
 * @brief Unit tests for Fabre TL curve comparison metrics (M1-M5)
 *
 * @author J. Lighthall
 * @date February 2026
 */

#include <gtest/gtest.h>

#include <cmath>
#include <vector>

#include "tl_metric.h"

// ============================================================================
// Helper: build a simple linear TL curve (range, TL) pairs
// ============================================================================
static std::vector<RangeTLPair> makeLinearCurve(double r0, double r1, int n,
                                                double tl0, double tl1) {
  std::vector<RangeTLPair> curve;
  for (int i = 0; i < n; ++i) {
    double t = static_cast<double>(i) / (n - 1);
    curve.push_back({r0 + t * (r1 - r0), tl0 + t * (tl1 - tl0)});
  }
  return curve;
}

// Helper: constant TL curve
static std::vector<RangeTLPair> makeConstantCurve(double r0, double r1, int n,
                                                  double tl) {
  return makeLinearCurve(r0, r1, n, tl, tl);
}

// ============================================================================
// scoreFromDiff tests (via identical/offset curves)
// ============================================================================

TEST(TLMetricTest, IdenticalCurvesScorePerfect) {
  // Use linear curve — correlation requires non-constant data
  auto curve = makeLinearCurve(0, 100, 50, 60.0, 90.0);
  TLMetrics metrics(curve, curve);

  EXPECT_NEAR(metrics.calculateTLDiff1(), 0.0, 1e-6);
  EXPECT_NEAR(metrics.calculateTLDiff2(), 0.0, 1e-6);
  EXPECT_NEAR(metrics.calculateCorrelation(), 1.0, 1e-10);
  EXPECT_NEAR(metrics.getMetric1(), 100.0, 1e-6);
  EXPECT_NEAR(metrics.getMetric2(), 100.0, 1e-6);
  EXPECT_NEAR(metrics.getMetric3(), 100.0, 1e-10);
  EXPECT_NEAR(metrics.getMCurve(), 100.0, 1e-6);
}

TEST(TLMetricTest, ConstantCurvesCorrelationUndefined) {
  // Correlation is undefined for constant sequences (zero variance)
  auto curve = makeConstantCurve(0, 100, 50, 70.0);
  TLMetrics metrics(curve, curve);

  EXPECT_DOUBLE_EQ(metrics.calculateTLDiff1(), 0.0);
  EXPECT_DOUBLE_EQ(metrics.calculateTLDiff2(), 0.0);
  // Correlation returns 0 for constant curves (undefined)
  EXPECT_DOUBLE_EQ(metrics.calculateCorrelation(), 0.0);
  EXPECT_NEAR(metrics.getMetric1(), 100.0, 1e-10);
  EXPECT_NEAR(metrics.getMetric2(), 100.0, 1e-10);
  EXPECT_DOUBLE_EQ(metrics.getMetric3(), 0.0);  // 0 correlation → 0 score
}

TEST(TLMetricTest, IdenticalLinearCurvesScorePerfect) {
  auto curve = makeLinearCurve(0, 200, 100, 50.0, 90.0);
  TLMetrics metrics(curve, curve);

  EXPECT_NEAR(metrics.getMetric1(), 100.0, 1e-6);
  EXPECT_NEAR(metrics.getMetric2(), 100.0, 1e-6);
  EXPECT_NEAR(metrics.getMCurve(), 100.0, 1e-6);
}

// ============================================================================
// Weighting: TL < 60 full weight, TL > 110 zero weight
// ============================================================================

TEST(TLMetricTest, ZeroWeightRegion) {
  // Both curves at TL = 120 dB — above TL_MAX = 110, so weight = 0
  auto curve1 = makeConstantCurve(0, 100, 50, 120.0);
  auto curve2 = makeConstantCurve(0, 100, 50, 125.0);
  TLMetrics metrics(curve1, curve2);

  // TLDiff1 should be 0 since all weights are 0
  EXPECT_DOUBLE_EQ(metrics.calculateTLDiff1(), 0.0);
}

// ============================================================================
// Score mapping: key breakpoints
// ============================================================================

TEST(TLMetricTest, ComputeAllPopulatesAllFields) {
  auto curve = makeLinearCurve(0, 100, 50, 60.0, 90.0);
  TLMetricResults results = TLMetrics(curve, curve).computeAll();

  EXPECT_EQ(results.n_points, 50u);
  EXPECT_NEAR(results.range_min, 0.0, 1e-10);
  EXPECT_NEAR(results.range_max, 100.0, 1e-10);
  EXPECT_FALSE(results.has_fom);
  EXPECT_EQ(results.fom, 0.0);
  EXPECT_NEAR(results.m1, 100.0, 1e-6);
  EXPECT_NEAR(results.m_curve, 100.0, 1e-6);
}

// ============================================================================
// M4/M5 without FOM return 0
// ============================================================================

TEST(TLMetricTest, NoFOMReturnsZeroM4M5) {
  auto curve = makeLinearCurve(0, 100, 30, 60.0, 80.0);
  TLMetrics metrics(curve, curve);

  EXPECT_FALSE(metrics.hasFOM());
  EXPECT_DOUBLE_EQ(metrics.getMetric4(), 0.0);
  EXPECT_DOUBLE_EQ(metrics.getMetric5(), 0.0);
  // M_total falls back to M_curve when no FOM
  EXPECT_NEAR(metrics.getMTotal(), metrics.getMCurve(), 1e-10);
}

// ============================================================================
// M4/M5 with FOM
// ============================================================================

TEST(TLMetricTest, WithFOMComputesM4M5) {
  auto curve1 = makeConstantCurve(0, 100, 50, 70.0);
  auto curve2 = makeConstantCurve(0, 100, 50, 70.0);
  TLMetrics metrics(curve1, curve2, 90.0);  // FOM = 90 dB

  EXPECT_TRUE(metrics.hasFOM());
  EXPECT_DOUBLE_EQ(metrics.getFOM(), 90.0);
  // Identical curves → M4 and M5 should be 100
  EXPECT_NEAR(metrics.getMetric4(), 100.0, 1e-10);
  EXPECT_NEAR(metrics.getMetric5(), 100.0, 1e-10);
  // M_total should average all 5
  double expected =
      (metrics.getMetric1() + metrics.getMetric2() + metrics.getMetric3() +
       metrics.getMetric4() + metrics.getMetric5()) /
      5.0;
  EXPECT_NEAR(metrics.getMTotal(), expected, 1e-10);
}

// ============================================================================
// Correlation tests
// ============================================================================

TEST(TLMetricTest, AnticorrelatedCurvesLowM3) {
  auto curve1 = makeLinearCurve(0, 100, 50, 60.0, 90.0);
  auto curve2 = makeLinearCurve(0, 100, 50, 90.0, 60.0);  // reversed slope
  TLMetrics metrics(curve1, curve2);

  // Anti-correlated curves should have negative correlation
  double corr = metrics.calculateCorrelation();
  EXPECT_LT(corr, 0.0);
  // M3 clamps negative correlation to zero
  EXPECT_DOUBLE_EQ(metrics.getMetric3(), 0.0);
}

TEST(TLMetricTest, PositiveCorrelationHighM3) {
  auto curve1 = makeLinearCurve(0, 100, 50, 60.0, 90.0);
  auto curve2 = makeLinearCurve(0, 100, 50, 62.0, 92.0);  // same trend + 2 dB
  TLMetrics metrics(curve1, curve2);

  double corr = metrics.calculateCorrelation();
  EXPECT_GT(corr, 0.9);
  EXPECT_GT(metrics.getMetric3(), 90.0);
}

// ============================================================================
// Grid interpolation: differing number of points
// ============================================================================

TEST(TLMetricTest, DifferentGridSizes) {
  // Same range and same linear TL, different number of points
  auto curve1 = makeLinearCurve(0, 100, 50, 60.0, 90.0);
  auto curve2 = makeLinearCurve(0, 100, 52, 60.0, 90.0);
  TLMetrics metrics(curve1, curve2);

  // Common grid should use max(50, 52) = 52 points
  EXPECT_EQ(metrics.getNumPoints(), 52u);
  // Identical linear TL → should score very high despite grid mismatch
  EXPECT_GT(metrics.getMCurve(), 99.0);
}

TEST(TLMetricTest, VeryDifferentGridSizes) {
  // 20 points vs 200 points
  auto curve1 = makeLinearCurve(0, 100, 20, 60.0, 80.0);
  auto curve2 = makeLinearCurve(0, 100, 200, 60.0, 80.0);
  TLMetrics metrics(curve1, curve2);

  EXPECT_EQ(metrics.getNumPoints(), 200u);
  // Same linear curve → should score very high
  EXPECT_GT(metrics.getMCurve(), 95.0);
}

TEST(TLMetricTest, DifferentRangeExtents) {
  // Curve 1 extends to 100, curve 2 extends to 110
  auto curve1 = makeConstantCurve(0, 100, 50, 70.0);
  auto curve2 = makeConstantCurve(0, 110, 55, 70.0);
  TLMetrics metrics(curve1, curve2);

  // Common range should truncate to min(100, 110) = 100
  TLMetricResults results = metrics.computeAll();
  EXPECT_NEAR(results.range_max, 100.0, 1e-10);
}

// ============================================================================
// Constructor validation
// ============================================================================

TEST(TLMetricTest, EmptyCurveThrows) {
  std::vector<RangeTLPair> empty;
  auto good = makeConstantCurve(0, 100, 10, 70.0);

  EXPECT_THROW(TLMetrics(empty, good), std::invalid_argument);
  EXPECT_THROW(TLMetrics(good, empty), std::invalid_argument);
  EXPECT_THROW(TLMetrics(empty, empty), std::invalid_argument);
}

// ============================================================================
// M2: last 4% of range
// ============================================================================

TEST(TLMetricTest, TLDiff2UsesLast4Percent) {
  // Curve with large difference only in last 4% of range
  const int n = 100;
  std::vector<RangeTLPair> curve1, curve2;
  for (int i = 0; i < n; ++i) {
    double r = static_cast<double>(i);
    double tl = 70.0;
    curve1.push_back({r, tl});
    // Small difference everywhere except last 4% (range >= 96)
    double tl2 = (r >= 96.0) ? 80.0 : 70.0;
    curve2.push_back({r, tl2});
  }

  TLMetrics metrics(curve1, curve2);

  // TLDiff2 should capture the large difference in the last 4%
  double diff2 = metrics.calculateTLDiff2();
  EXPECT_GT(diff2, 5.0);  // Should be around 10 dB in the tail

  // TLDiff1 should be much smaller (diluted by the 96% of matching data)
  double diff1 = metrics.calculateTLDiff1();
  EXPECT_LT(diff1, diff2);
}

// ============================================================================
// printResults doesn't crash
// ============================================================================

TEST(TLMetricTest, PrintResultsNoCrash) {
  auto curve = makeLinearCurve(0, 100, 30, 60.0, 80.0);
  TLMetrics metrics(curve, curve);

  // Redirect stdout to suppress output during test
  testing::internal::CaptureStdout();
  EXPECT_NO_THROW(metrics.printResults());
  testing::internal::GetCapturedStdout();
}

TEST(TLMetricTest, PrintResultsWithFOMNoCrash) {
  auto curve = makeLinearCurve(0, 100, 30, 60.0, 80.0);
  TLMetrics metrics(curve, curve, 85.0);

  testing::internal::CaptureStdout();
  EXPECT_NO_THROW(metrics.printResults());
  testing::internal::GetCapturedStdout();
}

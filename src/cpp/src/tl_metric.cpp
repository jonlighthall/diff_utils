/**
 * @file tl_metric.cpp
 * @brief Implementation of Fabre TL curve comparison metrics (M1-M5)
 *
 * @author J. Lighthall
 * @date February 2026
 * Adapted from reference/tl_metrics.cpp
 */

#include "tl_metric.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <stdexcept>

// ============================================================================
// Constructor
// ============================================================================

TLMetrics::TLMetrics(const std::vector<RangeTLPair>& curve1,
                     const std::vector<RangeTLPair>& curve2,
                     double figure_of_merit)
    : fom_(figure_of_merit), use_fom_(figure_of_merit > 0.0) {
  if (curve1.empty() || curve2.empty()) {
    throw std::invalid_argument("Input curves cannot be empty");
  }
  interpolateToCommonGrid(curve1, curve2);
}

// ============================================================================
// Internal helpers
// ============================================================================

double TLMetrics::calculateWeight(double tl) const {
  if (tl <= TL_MIN) return 1.0;
  if (tl >= TL_MAX) return 0.0;
  return (TL_MAX - tl) / (TL_MAX - TL_MIN);
}

double TLMetrics::dbToIntensity(double db) {
  return std::pow(10.0, -db / 10.0);
}

double TLMetrics::intensityToDb(double intensity) {
  return -10.0 * std::log10(intensity);
}

double TLMetrics::scoreFromDiff(double diff) const {
  // Score mapping function (Figure 1 in Fabre 2009)
  if (diff <= 3.0) {
    // High score for differences 0-3 dB (90s range)
    return 100.0 - (diff / 3.0) * 10.0;
  } else if (diff < 20.0) {
    // Linear decrease from ~90 to 0 for 3-20 dB
    return std::max(0.0, 90.0 - ((diff - 3.0) / 17.0) * 90.0);
  } else {
    return 0.0;
  }
}

void TLMetrics::interpolateToCommonGrid(
    const std::vector<RangeTLPair>& curve1,
    const std::vector<RangeTLPair>& curve2) {
  // Find common max range
  double maxRange1 = curve1.back().range;
  double maxRange2 = curve2.back().range;
  double maxRange = std::min(maxRange1, maxRange2);

  // Create common range grid
  size_t numPoints = std::max(curve1.size(), curve2.size());
  ranges_.clear();
  tl1_.clear();
  tl2_.clear();

  for (size_t i = 0; i < numPoints; ++i) {
    double r = (maxRange * static_cast<double>(i)) /
               static_cast<double>(numPoints - 1);
    ranges_.push_back(r);
  }

  // Interpolate both curves to intensity space, then back to dB
  for (double r : ranges_) {
    double intensity1 = interpolateIntensity(curve1, r);
    tl1_.push_back(intensityToDb(intensity1));

    double intensity2 = interpolateIntensity(curve2, r);
    tl2_.push_back(intensityToDb(intensity2));
  }
}

double TLMetrics::interpolateIntensity(const std::vector<RangeTLPair>& curve,
                                       double r) const {
  if (r <= curve.front().range) {
    return dbToIntensity(curve.front().tl);
  }
  if (r >= curve.back().range) {
    return dbToIntensity(curve.back().tl);
  }

  // Find bracketing points and interpolate linearly in intensity space
  for (size_t i = 0; i < curve.size() - 1; ++i) {
    if (r >= curve[i].range && r <= curve[i + 1].range) {
      double t = (r - curve[i].range) / (curve[i + 1].range - curve[i].range);
      double int1 = dbToIntensity(curve[i].tl);
      double int2 = dbToIntensity(curve[i + 1].tl);
      return int1 + t * (int2 - int1);
    }
  }

  return dbToIntensity(curve.back().tl);
}

// ============================================================================
// Component calculations
// ============================================================================

double TLMetrics::calculateTLDiff1() const {
  double sumWeightedDiff = 0.0;
  double sumWeights = 0.0;

  for (size_t i = 0; i < tl1_.size(); ++i) {
    double diff = std::abs(tl1_[i] - tl2_[i]);
    double weight = calculateWeight(tl1_[i]);
    sumWeightedDiff += diff * weight;
    sumWeights += weight;
  }

  if (sumWeights < 1e-10) {
    return 0.0;
  }

  return sumWeightedDiff / sumWeights;
}

double TLMetrics::calculateTLDiff2() const {
  double maxRange = ranges_.back();
  double rangeThreshold = maxRange * 0.96;  // Last 4%

  double sumDiff = 0.0;
  int count = 0;

  for (size_t i = 0; i < ranges_.size(); ++i) {
    if (ranges_[i] >= rangeThreshold) {
      sumDiff += std::abs(tl1_[i] - tl2_[i]);
      count++;
    }
  }

  if (count == 0) return 0.0;
  return sumDiff / count;
}

double TLMetrics::calculateCorrelation() const {
  if (tl1_.size() < 2) return 0.0;

  double mean1 = std::accumulate(tl1_.begin(), tl1_.end(), 0.0) / tl1_.size();
  double mean2 = std::accumulate(tl2_.begin(), tl2_.end(), 0.0) / tl2_.size();

  double numerator = 0.0;
  double denom1 = 0.0;
  double denom2 = 0.0;

  for (size_t i = 0; i < tl1_.size(); ++i) {
    double diff1 = tl1_[i] - mean1;
    double diff2 = tl2_[i] - mean2;
    numerator += diff1 * diff2;
    denom1 += diff1 * diff1;
    denom2 += diff2 * diff2;
  }

  if (denom1 < 1e-10 || denom2 < 1e-10) return 0.0;

  return numerator / std::sqrt(denom1 * denom2);
}

double TLMetrics::calculateRangeCoverage() const {
  if (!use_fom_) return 0.0;

  double maxRange = ranges_.back();
  double coverage1 = 0.0;
  double coverage2 = 0.0;

  for (size_t i = 0; i < ranges_.size(); ++i) {
    double se1 = fom_ - tl1_[i];  // Signal excess
    double se2 = fom_ - tl2_[i];

    if (se1 > 0) coverage1 += se1;
    if (se2 > 0) coverage2 += se2;
  }

  // Convert to percentage by normalizing by max range
  double pctCoverage1 = (coverage1 / maxRange) * 100.0;
  double pctCoverage2 = (coverage2 / maxRange) * 100.0;

  return std::abs(pctCoverage1 - pctCoverage2);
}

double TLMetrics::calculateDetectionRange() const {
  if (!use_fom_) return 0.0;

  auto findDetectionRange =
      [this](const std::vector<double>& tl_curve) -> double {
    double detRange = 0.0;
    int consecutiveDips = 0;
    const int maxDips = 1;  // Allow 1 range step dip

    for (size_t i = 0; i < ranges_.size(); ++i) {
      double se = fom_ - tl_curve[i];

      if (se > 0) {
        detRange = ranges_[i];
        consecutiveDips = 0;
      } else {
        consecutiveDips++;
        if (consecutiveDips > maxDips) {
          break;
        }
      }
    }
    return detRange;
  };

  double detRange1 = findDetectionRange(tl1_);
  double detRange2 = findDetectionRange(tl2_);
  double maxRange = ranges_.back();

  double pctRange1 = (detRange1 / maxRange) * 100.0;
  double pctRange2 = (detRange2 / maxRange) * 100.0;

  return std::abs(pctRange1 - pctRange2);
}

// ============================================================================
// Scored metrics (0-100)
// ============================================================================

double TLMetrics::getMetric1() const {
  return scoreFromDiff(calculateTLDiff1());
}
double TLMetrics::getMetric2() const {
  return scoreFromDiff(calculateTLDiff2());
}

double TLMetrics::getMetric3() const {
  double corr = calculateCorrelation();
  // Normalize: negative correlations score 0, positive scale 0-100
  return std::max(0.0, corr * 100.0);
}

double TLMetrics::getMetric4() const {
  if (!use_fom_) return 0.0;
  double covDiff = calculateRangeCoverage();
  return std::max(0.0, 100.0 - covDiff);
}

double TLMetrics::getMetric5() const {
  if (!use_fom_) return 0.0;
  double drDiff = calculateDetectionRange();
  return std::max(0.0, 100.0 - drDiff);
}

// ============================================================================
// Composite scores
// ============================================================================

double TLMetrics::getMCurve() const {
  return (getMetric1() + getMetric2() + getMetric3()) / 3.0;
}

double TLMetrics::getMTotal() const {
  if (!use_fom_) {
    return getMCurve();  // Fall back to M_curve if no FOM
  }
  return (getMetric1() + getMetric2() + getMetric3() + getMetric4() +
          getMetric5()) /
         5.0;
}

// ============================================================================
// Complete results
// ============================================================================

TLMetricResults TLMetrics::computeAll() const {
  TLMetricResults results;

  // Raw values
  results.tl_diff_1 = calculateTLDiff1();
  results.tl_diff_2 = calculateTLDiff2();
  results.correlation = calculateCorrelation();
  results.range_coverage_diff = calculateRangeCoverage();
  results.detection_range_diff = calculateDetectionRange();

  // Scored metrics
  results.m1 = getMetric1();
  results.m2 = getMetric2();
  results.m3 = getMetric3();
  results.m4 = getMetric4();
  results.m5 = getMetric5();

  // Composites
  results.m_curve = getMCurve();
  results.m_total = getMTotal();

  // Metadata
  results.n_points = ranges_.size();
  results.range_min = ranges_.empty() ? 0.0 : ranges_.front();
  results.range_max = ranges_.empty() ? 0.0 : ranges_.back();
  results.has_fom = use_fom_;
  results.fom = fom_;

  return results;
}

// ============================================================================
// Output
// ============================================================================

void TLMetrics::printResults() const {
  std::cout << "\n===== TL Curve Comparison Metrics =====\n";
  std::cout << "Number of range points: " << ranges_.size() << "\n";
  std::cout << "Range: " << ranges_.front() << " to " << ranges_.back() << "\n";
  if (use_fom_) {
    std::cout << "Figure of Merit (FOM): " << fom_ << " dB\n";
  }

  std::cout << "\n----- Component Values -----\n";
  std::cout << "TL_diff_1 (weighted diff):    " << calculateTLDiff1()
            << " dB\n";
  std::cout << "TL_diff_2 (last 4% diff):     " << calculateTLDiff2()
            << " dB\n";
  std::cout << "Correlation coefficient:      " << calculateCorrelation()
            << "\n";
  if (use_fom_) {
    std::cout << "Range coverage diff:          " << calculateRangeCoverage()
              << " %\n";
    std::cout << "Detection range diff:         " << calculateDetectionRange()
              << " %\n";
  }

  std::cout << "\n----- Metric Scores (0-100) -----\n";
  std::cout << "M1 (weighted diff):           " << getMetric1() << "\n";
  std::cout << "M2 (last 4% diff):            " << getMetric2() << "\n";
  std::cout << "M3 (correlation):             " << getMetric3() << "\n";
  if (use_fom_) {
    std::cout << "M4 (range coverage):          " << getMetric4() << "\n";
    std::cout << "M5 (detection range):         " << getMetric5() << "\n";
  }

  std::cout << "\n----- Final Metrics -----\n";
  std::cout << "M_curve (avg of M1-M3):       " << getMCurve() << "\n";
  if (use_fom_) {
    std::cout << "M_total (avg of M1-M5):       " << getMTotal() << "\n";
  }
  std::cout << "======================================\n\n";
}

void TLMetrics::printStats() const {
  TLMetricResults r = computeAll();
  std::cout << std::defaultfloat << std::setprecision(6);
  std::cout << "TL_METRIC_STATS"
            << "\ttl_diff_1=" << r.tl_diff_1 << "\ttl_diff_2=" << r.tl_diff_2
            << "\tcorrelation=" << r.correlation << "\tm1=" << r.m1
            << "\tm2=" << r.m2 << "\tm3=" << r.m3 << "\tm_curve=" << r.m_curve
            << "\tn_points=" << r.n_points << std::endl;
}

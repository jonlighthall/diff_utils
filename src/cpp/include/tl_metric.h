/**
 * @file tl_metric.h
 * @brief TL curve comparison metrics (Fabre M1-M5)
 *
 * Implements the quantitative TL curve comparison method from:
 *   Fabre & Zingarelli, "A synthesis of transmission loss comparison methods,"
 *   IEEE OCEANS 2009, doi:10.23919/OCEANS.2009.5422312
 *
 * Five metric components:
 *   M1 - Weighted mean absolute difference (Equation 1)
 *   M2 - Mean difference over last 4% of range (Equation 2)
 *   M3 - Correlation coefficient
 *   M4 - Range coverage energy difference (requires FOM)
 *   M5 - Near-continuous detection range difference (requires FOM)
 *
 * Composite scores:
 *   M_curve = mean(M1, M2, M3)          — curve shape comparison
 *   M_total = mean(M1, M2, M3, M4, M5)  — full operational comparison
 *
 * @author J. Lighthall
 * @date February 2026
 * Adapted from reference/tl_metrics.cpp
 */

#ifndef TL_METRIC_H
#define TL_METRIC_H

#include <string>
#include <vector>

/**
 * @brief Range-TL pair for curve representation
 */
struct RangeTLPair {
  double range;
  double tl;
};

/**
 * @brief Results from TL metric computation
 */
struct TLMetricResults {
  // Raw component values
  double tl_diff_1;             // Weighted mean absolute difference (dB)
  double tl_diff_2;             // Mean difference over last 4% of range (dB)
  double correlation;           // Correlation coefficient [-1, 1]
  double range_coverage_diff;   // Range coverage energy difference (%)
  double detection_range_diff;  // Detection range difference (%)

  // Scored metric values (0-100)
  double m1;  // Score from tl_diff_1
  double m2;  // Score from tl_diff_2
  double m3;  // Score from correlation
  double m4;  // Score from range coverage (0 if no FOM)
  double m5;  // Score from detection range (0 if no FOM)

  // Composite scores
  double m_curve;  // Mean of M1-M3
  double m_total;  // Mean of M1-M5 (or M_curve if no FOM)

  // Metadata
  size_t n_points;
  double range_min;
  double range_max;
  bool has_fom;
  double fom;
};

/**
 * @brief Computes Fabre TL curve comparison metrics
 *
 * This class takes two TL curves (as range-TL pairs), interpolates them
 * to a common grid in the intensity domain, and computes the five Fabre
 * metric components plus composite scores.
 */
class TLMetrics {
 public:
  /**
   * @brief Construct from two TL curves
   * @param curve1 Reference TL curve (range-TL pairs, sorted by range)
   * @param curve2 Test TL curve (range-TL pairs, sorted by range)
   * @param figure_of_merit Optional FOM for M4/M5 (0 = not used)
   * @throws std::invalid_argument if either curve is empty
   */
  TLMetrics(const std::vector<RangeTLPair>& curve1,
            const std::vector<RangeTLPair>& curve2,
            double figure_of_merit = 0.0);

  ~TLMetrics() = default;

  // ========================================================================
  // Component calculations (raw values)
  // ========================================================================
  double calculateTLDiff1() const;         // Weighted mean abs diff (dB)
  double calculateTLDiff2() const;         // Last 4% range mean diff (dB)
  double calculateCorrelation() const;     // Pearson correlation
  double calculateRangeCoverage() const;   // Requires FOM
  double calculateDetectionRange() const;  // Requires FOM

  // ========================================================================
  // Scored metrics (0-100)
  // ========================================================================
  double getMetric1() const;
  double getMetric2() const;
  double getMetric3() const;
  double getMetric4() const;  // Returns 0 if no FOM
  double getMetric5() const;  // Returns 0 if no FOM

  // ========================================================================
  // Composite scores
  // ========================================================================
  double getMCurve() const;  // Mean of M1-M3
  double getMTotal() const;  // Mean of M1-M5 (falls back to M_curve if no FOM)

  // ========================================================================
  // Complete results
  // ========================================================================
  TLMetricResults computeAll() const;

  // ========================================================================
  // Output
  // ========================================================================
  void printResults() const;
  void printStats() const;

  // ========================================================================
  // Accessors
  // ========================================================================
  size_t getNumPoints() const { return ranges_.size(); }
  bool hasFOM() const { return use_fom_; }
  double getFOM() const { return fom_; }

 private:
  // Interpolated curve data on common grid
  std::vector<double> ranges_;
  std::vector<double> tl1_;
  std::vector<double> tl2_;

  // FOM configuration
  double fom_;
  bool use_fom_;

  // Weighting parameters (Fabre Table I)
  static constexpr double TL_MIN = 60.0;   // Full weight below this
  static constexpr double TL_MAX = 110.0;  // Zero weight above this

  // Internal methods
  double calculateWeight(double tl) const;
  static double dbToIntensity(double db);
  static double intensityToDb(double intensity);
  double scoreFromDiff(double diff) const;
  void interpolateToCommonGrid(const std::vector<RangeTLPair>& curve1,
                               const std::vector<RangeTLPair>& curve2);
  double interpolateIntensity(const std::vector<RangeTLPair>& curve,
                              double r) const;
};

#endif  // TL_METRIC_H

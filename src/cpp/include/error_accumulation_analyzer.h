/**
 * @file error_accumulation_analyzer.h
 * @author J. Lighthall
 * @date October 2025
 * @brief Analyzes error accumulation patterns to distinguish systematic from
 * random errors
 *
 * This class implements statistical tests to determine if errors in numerical
 * comparisons are systematic (growing with range, correlated) or random
 * (transient noise, platform differences). Used in acoustic propagation model
 * validation to replace hand-waving threshold rules with objective metrics.
 */

#ifndef ERROR_ACCUMULATION_ANALYZER_H
#define ERROR_ACCUMULATION_ANALYZER_H

#include <string>
#include <vector>

// Forward declaration
struct ErrorAccumulationData;

/**
 * @brief Statistical metrics for error accumulation analysis
 */
struct AccumulationMetrics {
  // Linear regression: error vs range
  double slope;            // d(error)/d(range) - error growth rate
  double intercept;        // y-intercept - baseline error offset
  double r_squared;        // goodness of fit (0-1, higher is better fit)
  double std_error_slope;  // standard error of slope estimate
  double p_value_slope;    // statistical significance (< 0.05 is significant)

  // Autocorrelation
  double autocorr_lag1;  // correlation(error[i], error[i+1])
  bool is_correlated;    // true if |autocorr| exceeds threshold

  // Run test (Wald-Wolfowitz)
  int n_runs;               // observed runs of consecutive +/- errors
  int expected_runs;        // expected runs for random sequence
  double run_test_z_score;  // standardized test statistic
  bool is_random;           // true if passes randomness test

  // Overall statistics
  double rmse;        // root mean square error
  double mean_error;  // average error (indicates bias)
  double max_error;   // maximum absolute error

  // Classification
  enum class ErrorPattern {
    SYSTEMATIC_GROWTH,  // Error increases with range (propagation issue)
    SYSTEMATIC_BIAS,    // Fixed offset (calibration/scaling issue)
    RANDOM_NOISE,       // Uncorrelated, passes randomness test (benign)
    NULL_POINT_NOISE,   // Errors at low signal regions (benign)
    TRANSIENT_SPIKES,   // Isolated large errors (investigate)
    INSUFFICIENT_DATA   // Not enough points for analysis
  };

  ErrorPattern pattern;
  std::string interpretation;  // Human-readable explanation
  std::string recommendation;  // Suggested action
};

/**
 * @brief Analyzer for error accumulation patterns in numerical comparisons
 */
class ErrorAccumulationAnalyzer {
 public:
  /**
   * @brief Analyze error accumulation data and classify pattern
   * @param data Collected error data with ranges and values
   * @return Statistical metrics and pattern classification
   */
  AccumulationMetrics analyze(const ErrorAccumulationData& data);

  /**
   * @brief Set minimum number of data points required for analysis
   * @param min_points Minimum points (default: 10)
   */
  void set_min_points(size_t min_points) { min_points_ = min_points; }

  /**
   * @brief Set thresholds for pattern classification
   * @param slope_threshold Minimum significant slope (default: 0.001)
   * @param r_squared_threshold Minimum R² for linear fit (default: 0.5)
   * @param autocorr_threshold Minimum autocorr for correlation (default: 0.5)
   */
  void set_thresholds(double slope_threshold, double r_squared_threshold,
                      double autocorr_threshold) {
    slope_threshold_ = slope_threshold;
    r_squared_threshold_ = r_squared_threshold;
    autocorr_threshold_ = autocorr_threshold;
  }

  /**
   * @brief Get pattern name as string
   * @param pattern Error pattern enum value
   * @return Pattern name
   */
  static std::string get_pattern_name(
      AccumulationMetrics::ErrorPattern pattern);

 private:
  // Configuration
  size_t min_points_ = 10;
  double slope_threshold_ = 0.001;    // Minimum significant slope
  double r_squared_threshold_ = 0.5;  // Minimum R² for good fit
  double autocorr_threshold_ = 0.5;   // Minimum autocorr for correlation

  // Statistical methods

  /**
   * @brief Compute linear regression: y = slope*x + intercept
   * @param x Independent variable (range)
   * @param y Dependent variable (error)
   * @return {slope, intercept, r_squared, std_error_slope, p_value}
   */
  struct RegressionResult {
    double slope;
    double intercept;
    double r_squared;
    double std_error_slope;
    double p_value;
  };
  RegressionResult linear_regression(const std::vector<double>& x,
                                     const std::vector<double>& y);

  /**
   * @brief Calculate autocorrelation at given lag
   * @param data Time series data
   * @param lag Lag distance (typically 1)
   * @return Correlation coefficient [-1, 1]
   */
  double calculate_autocorrelation(const std::vector<double>& data, int lag);

  /**
   * @brief Count runs (consecutive sequences of same sign)
   * @param data Sequence of values
   * @return Number of runs
   */
  int count_runs(const std::vector<double>& data);

  /**
   * @brief Calculate expected runs for random sequence
   * @param n_positive Number of positive values
   * @param n_negative Number of negative values
   * @return Expected number of runs
   */
  int expected_runs(int n_positive, int n_negative);

  /**
   * @brief Perform Wald-Wolfowitz run test for randomness
   * @param n_runs Observed runs
   * @param n_pos Number of positive values
   * @param n_neg Number of negative values
   * @return Z-score (|z| > 1.96 indicates non-random at 95% confidence)
   */
  double run_test_z_score(int n_runs, int n_pos, int n_neg);

  /**
   * @brief Classify error pattern based on metrics
   * @param metrics Computed statistical metrics
   * @return Error pattern classification
   */
  AccumulationMetrics::ErrorPattern classify_pattern(
      const AccumulationMetrics& metrics);

  /**
   * @brief Generate interpretation text for pattern
   * @param metrics Metrics with pattern classification
   * @return Human-readable interpretation
   */
  std::string generate_interpretation(const AccumulationMetrics& metrics);

  /**
   * @brief Generate recommendation based on pattern
   * @param metrics Metrics with pattern classification
   * @return Suggested action
   */
  std::string generate_recommendation(const AccumulationMetrics& metrics);
};

#endif  // ERROR_ACCUMULATION_ANALYZER_H

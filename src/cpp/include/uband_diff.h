/**
 * @author J. Lighthall
 * @date July 2025
 * Refactored from main/uband_diff.cpp
 */

#ifndef UBAND_DIFF_H
#define UBAND_DIFF_H

#include <cmath>
#include <fstream>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "difference_analyzer.h"
#include "error_accumulation_analyzer.h"
#include "file_reader.h"
#include "format_tracker.h"
#include "line_parser.h"
#include "print_level.h"

// Data structures

struct Thresholds {
  // user-defined thresholds
  // -------------------------------------------------------
  double significant;  // lower threshold for significant difference (fail)
  double critical;     // threshold for critical difference (exit)
  double print;        // threshold for printing entry in table (print)

  // fixed thresholds
  // -------------------------------------------------------

  // Single precision epsilon - calculate once to avoid duplicate computations
  static constexpr double SINGLE_PRECISION_EPSILON =
      std::pow(2.0, -23);  // ~1.19e-07

  // define epsilon when threshold is zero
  const double zero = SINGLE_PRECISION_EPSILON;

  // Define the maximum significant value of TL (Transmission Loss). This will
  // be referred to as the marginal threshold. It is equal to the upper upper
  // threshold given in the following reference. TL values above this threshold
  // are considered insignificant.
  //
  //   https://doi.org/10.23919/OCEANS.2009.5422312
  const double marginal =
      110;  // upper threshold for significant difference (warning)

  // Define the maximum numerically valid value for TL. The value given
  // below is based on the smallest value that can be represented in single
  // precision floating point format, corresponding to the smallest pressure
  // magnitude that can be represented.
  //
  // Values below this threshold are considered numerically "meaningless" and
  // will not trigger any action in the comparison.
  const double ignore =
      -20 * log10(SINGLE_PRECISION_EPSILON);  // threshold for meaningless
                                              // difference (no action)

  // Cached calculations for performance optimization
  mutable double cached_log10_significant = 0.0;
  mutable bool log10_significant_cached = false;

  // Percent-mode support: when true, interpret 'significant_percent' as a
  // fractional value (e.g. 0.01 for 1%). This overrides the usual
  // absolute 'significant' comparison for deciding significance.
  bool significant_is_percent = false;
  double significant_percent = 0.0;  // fractional (0.01 == 1%)

  // Helper method to get log10(significant) with caching
  double get_log10_significant() const {
    if (!log10_significant_cached || significant <= 0) {
      if (significant > 0) {
        cached_log10_significant = std::log10(significant);
        log10_significant_cached = true;
      } else {
        return 0.0;  // Handle edge case for zero/negative thresholds
      }
    }
    return cached_log10_significant;
  }

  // Method to invalidate cache when significant threshold changes
  void update_significant(double new_significant) {
    if (significant != new_significant) {
      significant = new_significant;
      log10_significant_cached = false;
    }
  }
};
struct CountStats {
  // number of...
  size_t line_number = 0;  // lines read
  size_t elem_number = 0;  // elements checked

  // non-zero differences found (independent of arguments)
  size_t diff_non_zero = 0;     // based on value and format (strict)
  size_t diff_trivial = 0;      // non-zero but within format precision
  size_t diff_non_trivial = 0;  // based on value only (format independent)

  // differences found greater than user defined...
  size_t diff_significant = 0;  // nominal threshold ("good enough")
  size_t diff_insignificant =
      0;                     // non-trivial but both values > ignore threshold
  size_t diff_marginal = 0;  // marginal threshold (pass and warn)
  size_t diff_critical = 0;  // critical threshold (fail and exit)

  // LEVEL 6 counters: non_critical = error + non_error (based on user
  // threshold). These are only populated for non-marginal, non-critical,
  // significant differences.
  size_t diff_error = 0;      // differences > user threshold (argument 3)
  size_t diff_non_error = 0;  // differences <= user threshold (argument 3)

  size_t diff_print = 0;  // print threshold (for difference table)
  // Count of non-trivial differences where BOTH values exceed ignore threshold
  size_t diff_high_ignore =
      0;  // aids semantic consistency checks for zero threshold
};
struct Flags {
  bool new_fmt = false;
  bool file_end_reached = false;  // Indicates if the end of file was reached
  bool error_found =
      false;  // Global error flag (critical threshold or file access)
  bool file_access_error = false;     // Specific flag for file access errors
  bool structures_compatible = true;  // Files have compatible column structures

  // Counter-associated flags (correspond to CountStats)
  bool has_non_zero_diff =
      false;  // Any non-zero difference found (format-dependent, like diff)
  bool has_non_trivial_diff = false;  // Difference exceeds format precision
                                      // threshold (format-independent)
  bool has_significant_diff =
      false;                       // Difference exceeds user-defined threshold
  bool has_marginal_diff = false;  // Difference exceeds marginal threshold
  bool has_critical_diff = false;  // Difference exceeds critical/hard threshold

  // LEVEL 5 flags: non_critical differences subdivision
  bool has_error_diff = false;      // Difference exceeds user threshold
  bool has_non_error_diff = false;  // Difference within user threshold

  bool has_printed_diff = false;  // Difference exceeds print threshold

  // Unit mismatch detection: if column 1 appears scaled by a factor
  // approximately equal to one nautical mile in meters (1852), set true.
  bool unit_mismatch = false;
  size_t unit_mismatch_line = 0;
  double unit_mismatch_ratio = 0.0;

  // Range data detection: if column 1 is monotonically increasing with fixed
  // delta, it's likely range data and TL thresholds don't apply
  bool column1_is_range_data = false;

  // Overall comparison state flags
  bool files_are_same = true;  // Files are identical
  bool files_have_same_values =
      true;  // Files have same values within precision
  bool files_are_close_enough =
      true;  // Files are same within user-defined threshold
};

struct DiffStats {
  // track the maximum difference found
  double max_non_zero = 0;
  double max_non_trivial = 0;
  double max_significant = 0;  // maximum significant difference

  // maximum percentage error observed for non-trivial differences (%),
  // calculated as 100 * |v1 - v2| / |v2| when |v2| > thresh.zero. If the
  // reference is effectively zero, this value may remain 0 (or be set to
  // a large sentinel by the analyzer).
  double max_percent_error = 0.0;

  int ndp_non_zero = 0;     // number of decimal places for non-zero values
  int ndp_non_trivial = 0;  // number of decimal places for non-trivial values
  int ndp_significant = 0;  // number of decimal places
  int ndp_max = 0;          // number of decimal places for maximum difference
  const int ndp_single_precision =
      7;  // number of decimal places for single precision
};

// RMSE (Root Mean Square Error) Statistics
struct RMSEStats {
  // Unweighted RMSE - Global (all elements)
  double sum_squared_errors_all = 0.0;
  size_t count_all = 0;

  // Unweighted RMSE - excluding range column (column 0)
  double sum_squared_errors_data = 0.0;
  size_t count_data = 0;

  // Unweighted RMSE - Per-column (for multi-column TL data)
  std::map<size_t, double>
      sum_squared_errors_per_column;          // column index -> sum
  std::map<size_t, size_t> count_per_column;  // column index -> count

  // Weighted RMSE - Global (all data elements, excluding range)
  double sum_weighted_squared_errors_data = 0.0;
  double sum_weights_data = 0.0;

  // Weighted RMSE - Per-column
  std::map<size_t, double> sum_weighted_squared_errors_per_column;
  std::map<size_t, double> sum_weights_per_column;

  // TL weighting parameters (operational significance region)
  static constexpr double TL_MIN_WEIGHT = 60.0;   // Full weight below this
  static constexpr double TL_MAX_WEIGHT = 110.0;  // Zero weight above this
  static constexpr double TL_WEIGHT_SPAN = TL_MAX_WEIGHT - TL_MIN_WEIGHT;

  // Calculate TL-based weight for a value
  // Returns 1.0 for TL <= 60 dB, 0.0 for TL >= 110 dB, linear between
  static double calculate_tl_weight(double tl_value) {
    if (tl_value <= TL_MIN_WEIGHT) {
      return 1.0;
    } else if (tl_value >= TL_MAX_WEIGHT) {
      return 0.0;
    } else {
      // Linear interpolation: weight = (110 - TL) / 50
      return (TL_MAX_WEIGHT - tl_value) / TL_WEIGHT_SPAN;
    }
  }

  // Helper to add an unweighted squared error
  void add_error(size_t column_index, double error) {
    double sq_err = error * error;

    // Add to global
    sum_squared_errors_all += sq_err;
    count_all++;

    // Add to data (excluding column 0 - range)
    if (column_index > 0) {
      sum_squared_errors_data += sq_err;
      count_data++;
    }

    // Add to per-column
    sum_squared_errors_per_column[column_index] += sq_err;
    count_per_column[column_index]++;
  }

  // Helper to add a weighted squared error (for TL data)
  // tl_ref and tl_test are the TL values from both files
  void add_weighted_error(size_t column_index, double error, double tl_ref,
                          double tl_test) {
    // Skip range column (column 0) for weighted RMSE
    if (column_index == 0) {
      return;
    }

    // Use average TL for weighting (could also use ref or test specifically)
    double avg_tl = (tl_ref + tl_test) / 2.0;
    double weight = calculate_tl_weight(avg_tl);

    double weighted_sq_err = weight * error * error;

    // Add to global weighted data
    sum_weighted_squared_errors_data += weighted_sq_err;
    sum_weights_data += weight;

    // Add to per-column weighted
    sum_weighted_squared_errors_per_column[column_index] += weighted_sq_err;
    sum_weights_per_column[column_index] += weight;
  }

  // Calculate unweighted RMSE for all elements
  double get_rmse_all() const {
    return (count_all > 0) ? std::sqrt(sum_squared_errors_all / count_all)
                           : 0.0;
  }

  // Calculate unweighted RMSE for data only (excluding range column)
  double get_rmse_data() const {
    return (count_data > 0) ? std::sqrt(sum_squared_errors_data / count_data)
                            : 0.0;
  }

  // Calculate unweighted RMSE for a specific column
  double get_rmse_column(size_t column_index) const {
    auto it_sum = sum_squared_errors_per_column.find(column_index);
    auto it_count = count_per_column.find(column_index);
    if (it_sum != sum_squared_errors_per_column.end() &&
        it_count != count_per_column.end() && it_count->second > 0) {
      return std::sqrt(it_sum->second / it_count->second);
    }
    return 0.0;
  }

  // Calculate weighted RMSE for data only (excluding range column)
  double get_weighted_rmse_data() const {
    return (sum_weights_data > 0)
               ? std::sqrt(sum_weighted_squared_errors_data / sum_weights_data)
               : 0.0;
  }

  // Calculate weighted RMSE for a specific column
  double get_weighted_rmse_column(size_t column_index) const {
    auto it_sum = sum_weighted_squared_errors_per_column.find(column_index);
    auto it_weight = sum_weights_per_column.find(column_index);
    if (it_sum != sum_weighted_squared_errors_per_column.end() &&
        it_weight != sum_weights_per_column.end() && it_weight->second > 0) {
      return std::sqrt(it_sum->second / it_weight->second);
    }
    return 0.0;
  }

  // Check if we have weighted data
  bool has_weighted_data() const { return sum_weights_data > 0; }
};

// TL Curve Comparison Metrics (based on Goodman et al.)
// Tracks data needed for M2, M3, and M_curve calculations
struct TLMetrics {
  // For M2: Last 4% of range analysis
  double sum_diff_last_4pct = 0.0;
  size_t count_last_4pct = 0;
  double max_range = 0.0;

  // For M3: Correlation coefficient
  std::vector<double> tl1_values;  // TL values from file 1
  std::vector<double> tl2_values;  // TL values from file 2
  std::vector<double> ranges;      // Range values for each point
  std::vector<double> diffs;       // Absolute differences

  // Track which column has TL data (usually column index > 0)
  size_t tl_column_index = 0;
  bool has_data = false;

  // Add a data point for analysis
  void add_point(double range, double tl1, double tl2, double diff_abs) {
    has_data = true;

    // Update max range
    if (range > max_range) {
      max_range = range;
    }

    // Store for later analysis
    ranges.push_back(range);
    tl1_values.push_back(tl1);
    tl2_values.push_back(tl2);
    diffs.push_back(diff_abs);
  }

  // Finalize - compute last 4% statistics after all data collected
  void finalize() {
    if (!has_data || ranges.empty()) return;

    // Calculate last 4% threshold
    double range_threshold = max_range * 0.96;

    sum_diff_last_4pct = 0.0;
    count_last_4pct = 0;

    for (size_t i = 0; i < ranges.size(); ++i) {
      if (ranges[i] >= range_threshold) {
        sum_diff_last_4pct += diffs[i];
        count_last_4pct++;
      }
    }
  }

  // Calculate M2: Mean difference in last 4% of range
  double calculate_m2() const {
    if (!has_data || count_last_4pct == 0) return 0.0;
    return sum_diff_last_4pct / count_last_4pct;
  }

  // Calculate M3: Correlation coefficient
  double calculate_correlation() const {
    if (!has_data || tl1_values.size() < 2) return 0.0;

    size_t n = tl1_values.size();
    double mean1 = 0.0, mean2 = 0.0;

    // Calculate means
    for (size_t i = 0; i < n; ++i) {
      mean1 += tl1_values[i];
      mean2 += tl2_values[i];
    }
    mean1 /= n;
    mean2 /= n;

    // Calculate correlation
    double numerator = 0.0;
    double denom1 = 0.0;
    double denom2 = 0.0;

    for (size_t i = 0; i < n; ++i) {
      double diff1 = tl1_values[i] - mean1;
      double diff2 = tl2_values[i] - mean2;
      numerator += diff1 * diff2;
      denom1 += diff1 * diff1;
      denom2 += diff2 * diff2;
    }

    if (denom1 < 1e-10 || denom2 < 1e-10) return 0.0;

    return numerator / std::sqrt(denom1 * denom2);
  }

  // Score from difference (Figure 1 in Goodman paper)
  static double score_from_diff(double diff) {
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

  // Calculate M_curve: average of M1, M2, M3 scores
  // Note: M1 is the weighted RMSE (TL_diff_1), passed in
  double calculate_m_curve(double m1_diff) const {
    double m2_diff = calculate_m2();
    double corr = calculate_correlation();

    // Convert to scores (0-100)
    double score1 = score_from_diff(m1_diff);
    double score2 = score_from_diff(m2_diff);
    double score3 = std::max(0.0, corr * 100.0);  // Correlation to 0-100

    return (score1 + score2 + score3) / 3.0;
  }
};

struct LineData {
  std::vector<double> values;
  std::vector<int> decimal_places;
};

struct ColumnValues {
  double value1;  // Value from first file at current column
  double value2;  // Value from second file at current column
  double range;   // First value in the line (used as range indicator)
  int dp1;        // Decimal places for file1 value
  int dp2;        // Decimal places for file2 value
  int min_dp;     // Minimum decimal places (for rounding)
  int max_dp;     // Maximum decimal places (for more precise output)
};

struct SummaryParams {
  std::string file1;  // Path to first file
  std::string file2;  // Path to second file
  int fmt_wid;        // Formatting width for output alignment
};

// Error Accumulation Analysis - Track errors as a function of range
struct ErrorAccumulationData {
  std::vector<double> ranges;          // Range values (typically column 1)
  std::vector<double> errors;          // raw_diff values
  std::vector<double> tl_values_ref;   // TL from reference file (column 2+)
  std::vector<double> tl_values_test;  // TL from test file (column 2+)
  std::vector<bool> is_significant;    // Significance flags

  // Metadata
  size_t n_points = 0;
  double range_min = std::numeric_limits<double>::max();
  double range_max = std::numeric_limits<double>::lowest();

  // Add a data point
  void add_point(double range, double error, double tl_ref, double tl_test,
                 bool significant) {
    ranges.push_back(range);
    errors.push_back(error);
    tl_values_ref.push_back(tl_ref);
    tl_values_test.push_back(tl_test);
    is_significant.push_back(significant);
    n_points++;

    if (range < range_min) range_min = range;
    if (range > range_max) range_max = range;
  }

  // Clear all data
  void clear() {
    ranges.clear();
    errors.clear();
    tl_values_ref.clear();
    tl_values_test.clear();
    is_significant.clear();
    n_points = 0;
    range_min = std::numeric_limits<double>::max();
    range_max = std::numeric_limits<double>::lowest();
  }
};

// PrintLevel definition moved to print_level.h

// Forward declarations for utility functions
std::tuple<double, double, int, int> readComplex(std::istringstream& stream,
                                                 Flags& flag);

// Main class declaration
class FileComparator {
 public:
  // Constructor
  FileComparator(double user_thresh, double hard_thresh, double table_thresh,
                 int verbosity_level = 0, int debug_level = 0,
                 bool significant_is_percent = false,
                 double significant_percent = 0.0)
      : thresh{user_thresh, hard_thresh, table_thresh},
        verbosity{verbosity_level, verbosity_level < 0,
                  verbosity_level >= 1, verbosity_level >= 2},
        debug{debug_level, debug_level >= 1, debug_level >= 2,
              debug_level >= 3},
        table{table_thresh, 32, table_thresh == 0.0},
        file_reader_(std::make_unique<FileReader>()),
        line_parser_(std::make_unique<LineParser>()),
        format_tracker_(std::make_unique<FormatTracker>(
            verbosity, debug)),
        difference_analyzer_(thresh) {
    // Apply percent-mode significant settings
    thresh.significant_is_percent = significant_is_percent;
    thresh.significant_percent = significant_percent;
  };

  // ========================================================================
  // Friend declarations for testing
  // ========================================================================
  friend class FileComparatorTest;
  friend class FileComparatorTest_ExtractColumnValues_Test;
  friend class FileComparatorTest_ProcessDifference_Test;
  friend class FileComparatorTest_ParseLine_Test;
  friend class FileComparatorTest_CompareFiles_Test;

  // ========================================================================
  // Public Interface
  // ========================================================================
  bool compare_files(const std::string& file1, const std::string& file2);
  LineData parse_line(const std::string& line) const;
  /** @note the function parse_line() reads a line from the file and returns a
   LineData object */
  void print_summary(const std::string& file1, const std::string& file2,
                     int argc, char* argv[]) const;
  void print_settings(const std::string& file1, const std::string& file2) const;

  // ========================================================================
  // Flag Access Methods
  // ========================================================================
  const Flags& getFlag() const { return flag; }
  Flags& getFlag() { return flag; }

  // ========================================================================
  // Counter Access Methods (for testing)
  // ========================================================================
  const CountStats& getCountStats() const { return counter; }
  const DiffStats& getDiffStats() const { return differ; }

 private:
  // ========================================================================
  // Data Members (must be declared before composition classes that use them)
  // ========================================================================
  Thresholds thresh;
  VerbosityControl verbosity;
  DebugControl debug;
  TableControl table;
  // Cap for number of difference table rows to print
  bool truncation_notice_printed_ = false;  // ensure single truncation notice

  // ========================================================================
  // Composition - Specialized classes for different responsibilities
  // ========================================================================
  std::unique_ptr<FileReader> file_reader_;
  std::unique_ptr<LineParser> line_parser_;
  std::unique_ptr<FormatTracker> format_tracker_;
  DifferenceAnalyzer difference_analyzer_;

  // ========================================================================
  // State Members
  // ========================================================================
  mutable Flags flag;
  size_t this_fmt_line;
  size_t this_fmt_column;
  size_t last_fmt_line;
  size_t this_line_ncols;

  DiffStats differ;
  CountStats counter;
  RMSEStats rmse_stats;
  TLMetrics tl_metrics;  // TL curve comparison metrics (M2, M3, M_curve)

  // Error accumulation analysis data
  ErrorAccumulationData accumulation_data_;
  // Cached accumulation metrics (lazy evaluation cache for const methods)
  mutable std::optional<AccumulationMetrics> accumulation_metrics_;

  // ========================================================================
  // Line/Column Processing
  // ========================================================================
  bool process_line(const LineData& data1, const LineData& data2,
                    std::vector<int>& dp_per_col, size_t& prev_n_col);
  bool process_column(const LineData& data1, const LineData& data2,
                      size_t column_index, std::vector<int>& dp_per_col);

  // ========================================================================
  // Validation & Format Management
  // ========================================================================
  bool validate_and_track_column_format(size_t n_col1, size_t n_col2,
                                        std::vector<int>& dp_per_col,
                                        size_t& prev_n_col);
  bool validate_decimal_column_size(const std::vector<int>& dp_per_col,
                                    size_t column_index) const;

  // ========================================================================
  // Decimal Places Management
  // ========================================================================
  // New methods for refactoring
  bool initialize_decimal_place_format(const int min_dp,
                                       const size_t column_index,
                                       std::vector<int>& dp_per_col);
  bool update_decimal_place_format(const int min_dp, const size_t column_index,
                                   std::vector<int>& dp_per_col);
  double calculate_threshold(int decimal_places);

  // ========================================================================
  // Difference Processing
  // ========================================================================
  bool process_difference(const ColumnValues& column_data, size_t column_index);
  void process_raw_values(const ColumnValues& column_data);
  void process_rounded_values(const ColumnValues& column_data,
                              size_t column_index, double rounded_diff,
                              int minimum_deci);

  // ========================================================================
  // Output & Formatting
  // ========================================================================
  ColumnValues extract_column_values(const LineData& data1,
                                     const LineData& data2,
                                     size_t column_index) const;
  void print_table(const ColumnValues& column_data, size_t column_index,
                   double line_threshold, double diff_rounded,
                   double diff_unrounded, double percent_error);
  std::string format_number(double value, int prec, int max_integer_width,
                            int max_decimals) const;
  void print_hard_threshold_error(double rounded1, double rounded2,
                                  double diff_rounded,
                                  size_t column_index) const;
  void print_format_info(const ColumnValues& column_data,
                         size_t column_index) const;
  void print_diff_like_summary(const SummaryParams& params) const;
  void print_rounded_summary(const SummaryParams& params) const;
  void print_significant_summary(const SummaryParams& params) const;
  void print_accumulation_analysis() const;  // Error accumulation analysis

  // ========================================================================
  // Summary Helper Functions (for cognitive complexity reduction)
  // ========================================================================
  std::string format_boolean_status(bool value, bool showStatus, bool reversed,
                                    bool soft) const;
  void print_arguments_and_files(const std::string& file1,
                                 const std::string& file2, int argc,
                                 char* argv[]) const;
  void print_statistics(const std::string& file1) const;
  void print_flag_status() const;
  void print_counter_info() const;
  void print_rmse_statistics() const;  // RMSE statistics
  void print_tl_metrics() const;       // TL curve comparison metrics
  void print_detailed_summary(const SummaryParams& params) const;
  void print_additional_diff_info(const SummaryParams& params) const;
  void print_critical_threshold_info() const;
  void print_consistency_checks() const;  // Verify six-level identities

  // Diff-like summary helper functions
  void print_identical_files_message(const SummaryParams& params) const;
  void print_exact_matches_info(const SummaryParams& params) const;
  void print_non_zero_differences_info(const SummaryParams& params) const;
  void print_difference_counts(const SummaryParams& params) const;
  void print_maximum_difference_analysis(const SummaryParams& params) const;
  std::string get_count_color(size_t count) const;

  // Significant summary helper functions
  void print_significant_differences_count(const SummaryParams& params) const;
  void print_significant_percentage() const;
  void print_insignificant_differences_count(const SummaryParams& params) const;
  void print_maximum_significant_difference_analysis(
      const SummaryParams& params) const;
  void print_maximum_significant_difference_details() const;
  void print_max_diff_threshold_comparison_above() const;
  void print_max_diff_threshold_comparison_below() const;
  void print_file_comparison_result(const SummaryParams& params) const;
  void print_significant_differences_printing_status(
      const SummaryParams& params) const;
  void print_count_with_percent(const SummaryParams& params,
                                const std::string& label, size_t count,
                                const std::string& color = "") const;
};

#endif  // UBAND_DIFF_H

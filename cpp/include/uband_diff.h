/**
 * @author J. Lighthall
 * @date July 2025
 * Refactored from main/uband_diff.cpp
 */

#ifndef UBAND_DIFF_H
#define UBAND_DIFF_H

#include <cmath>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "difference_analyzer.h"
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
  bool error_found = false;       // Global error flag
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

  int ndp_non_zero = 0;     // number of decimal places for non-zero values
  int ndp_non_trivial = 0;  // number of decimal places for non-trivial values
  int ndp_significant = 0;  // number of decimal places
  int ndp_max = 0;          // number of decimal places for maximum difference
  const int ndp_single_precision =
      7;  // number of decimal places for single precision
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

// PrintLevel definition moved to print_level.h

// Forward declarations for utility functions
std::tuple<double, double, int, int> readComplex(std::istringstream& stream,
                                                 Flags& flag);

// Main class declaration
class FileComparator {
 public:
  // Constructor
  FileComparator(double user_thresh, double hard_thresh, double print_thresh,
                 int debug_level = 0)
      : thresh{user_thresh, hard_thresh, print_thresh},
        print{debug_level, debug_level < 0, debug_level >= 1, debug_level >= 2,
              debug_level >= 3},
        file_reader_(std::make_unique<FileReader>()),
        line_parser_(std::make_unique<LineParser>()),
        format_tracker_(std::make_unique<FormatTracker>(
            PrintLevel{debug_level, debug_level < 0, debug_level >= 1,
                       debug_level >= 2, debug_level >= 3})),
        difference_analyzer_(std::make_unique<DifferenceAnalyzer>(thresh)) {};

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
  PrintLevel print;
  // Cap for number of difference table rows to print. Analysis continues
  // after the cap; only additional table rows are suppressed.
  size_t max_print_rows_ = 50;  // reasonable default to avoid terminal spam
  bool truncation_notice_printed_ = false;  // ensure single truncation notice

  // ========================================================================
  // Composition - Specialized classes for different responsibilities
  // ========================================================================
  std::unique_ptr<FileReader> file_reader_;
  std::unique_ptr<LineParser> line_parser_;
  std::unique_ptr<FormatTracker> format_tracker_;
  std::unique_ptr<DifferenceAnalyzer> difference_analyzer_;

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
                   double diff_unrounded);
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

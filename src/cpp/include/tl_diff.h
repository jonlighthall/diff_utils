/**
 * @file tl_diff.h
 * @brief FileComparator class for precision-aware file comparison (tl_diff)
 *
 * @author J. Lighthall
 * @date July 2025
 * Refactored from main/uband_diff.cpp
 */

#ifndef TL_DIFF_H
#define TL_DIFF_H

#include "tl_common.h"

// Main class declaration
class FileComparator {
 public:
  // Constructor
  FileComparator(double user_thresh, double hard_thresh, double table_thresh,
                 int verbosity_level = 0, int debug_level = 0,
                 bool significant_is_percent = false,
                 double significant_percent = 0.0, size_t max_rows = 32)
      : thresh{user_thresh, hard_thresh, table_thresh},
        verbosity{verbosity_level, verbosity_level < 0, verbosity_level >= 1,
                  verbosity_level >= 2},
        debug{debug_level, debug_level >= 1, debug_level >= 2,
              debug_level >= 3},
        table{table_thresh, max_rows, table_thresh == 0.0},
        file_reader_(std::make_unique<FileReader>()),
        line_parser_(std::make_unique<LineParser>()),
        format_tracker_(std::make_unique<FormatTracker>(verbosity, debug)),
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

#endif  // TL_DIFF_H

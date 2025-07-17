#ifndef UBAND_DIFF_H
#define UBAND_DIFF_H

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

// Data structures

struct Flags {
  bool new_fmt = false;
  bool isERROR = false;  // Global error flag
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
};

struct SummaryParams {
  std::string file1;  // Path to first file
  std::string file2;  // Path to second file
  int fmt_wid;        // Formatting width for output alignment
};

struct DiffStats {
  // track the maximum difference found
  double max = 0;
  double max_rounded = 0;
};

struct CountStats {
  // number of...
  size_t line_number = 0;  // lines read
  size_t elem_number = 0;  // elements checked

  // non-zero differences found (independent of arguments)
  size_t diff_non_zero = 0;  // zero (non-zero differences)
  size_t diff_prec = 0;      // format precision threshold

  // differences found greater than user defined...
  size_t diff_user = 0;   // nominal threshold ("good enough")
  size_t diff_hard = 0;   // hard threshold (fail and exit)
  size_t diff_print = 0;  // print threshold (for difference table)
};

struct Thresholds {
  // user-defined thresholds
  // -------------------------------------------------------
  double user;   // lower threshold for significant difference (fail)
  double hard;   // threshold for critical difference (exit)
  double print;  // threshold for printing entry in table (print)

  // fixed thresholds
  // -------------------------------------------------------

  // define epsilon when threshold is zero
  const double zero = pow(2, -23);  // equal to single precision epsilon

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
      -20 *
      log10(pow(2, -23));  // threshold for meaningless difference (no action)
};

// Forward declarations for utility functions
std::tuple<double, double, int, int> readComplex(std::istringstream& stream,
                                                 Flags& flag);

// Main class declaration
class FileComparator {
 public:
  // Constructor
  FileComparator(double user_thresh, double hard_thresh, double print_thresh)
      : thresh{user_thresh, hard_thresh, print_thresh} {};

  // ========================================================================
  // Public Interface
  // ========================================================================
  bool compare_files(const std::string& file1, const std::string& file2);
  LineData parse_line(const std::string& line) const;
  /** @note the function parse_line() reads a line from the file and returns a
   LineData object */
  void print_summary(const std::string& file1, const std::string& file2,
                     int argc, char* argv[]) const;

  // ========================================================================
  // Flag Access Methods
  // ========================================================================
  const Flags& getFlag() const { return flag; }
  Flags& getFlag() { return flag; }

 private:
  // ========================================================================
  // Data Members
  // ========================================================================
  mutable Flags flag;
  unsigned long this_fmt_line;
  unsigned long this_fmt_column;
  unsigned long last_fmt_line;
  unsigned long this_line_ncols;

  DiffStats differ;
  CountStats counter;
  Thresholds thresh;

  // ========================================================================
  // File Operations
  // ========================================================================
  bool open_files(const std::string& file1, const std::string& file2,
                 std::ifstream& infile1, std::ifstream& infile2) const;
  size_t get_file_length(const std::string& file) const;
  bool compare_file_lengths(const std::string& file1,
                          const std::string& file2) const;

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
  bool validate_decimal_places(int dp1, int dp2) const;
  bool validate_decimal_column_size(const std::vector<int>& dp_per_col,
                                    size_t column_index) const;

  // ========================================================================
  // Decimal Places Management
  // ========================================================================
  // New methods for refactoring
  bool initialize_decimal_places(int min_dp, size_t column_index,
                                 std::vector<int>& dp_per_col);
  bool update_decimal_places_format(int min_dp, size_t column_index,
                                    std::vector<int>& dp_per_col);
  double calculate_threshold(int decimal_places);

  // ========================================================================
  // Difference Processing
  // ========================================================================
  bool process_difference(const ColumnValues& column_data, size_t column_index);
  void process_raw_values(double value1, double value2);
  void process_rounded_values(double rounded_diff, double minimum_deci);

  // ========================================================================
  // Output & Formatting
  // ========================================================================
  ColumnValues extract_column_values(const LineData& data1, const LineData& data2,
                                     size_t column_index) const;
  void print_table(const ColumnValues& column_data, size_t column_index,
                   double line_threshold, double diff_rounded);
  std::string format_number(double value, int prec, int max_integer_width,
                            int max_decimals) const;
  void print_hard_threshold_error(double rounded1, double rounded2,
                                  double diff_rounded, size_t column_index) const;
  void print_format_info(const ColumnValues& column_data,
                         size_t column_index) const;
  void print_diff_like_summary(const SummaryParams& params) const;
  void print_rounded_summary(const SummaryParams& params) const;
  void print_significant_summary(const SummaryParams& params) const;
};

#endif  // UBAND_DIFF_H

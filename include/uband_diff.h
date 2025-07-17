#ifndef UBAND_DIFF_H
#define UBAND_DIFF_H

#include <cmath>
#include <fstream>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

// Global error flag
extern bool isERROR;

// Forward declarations for utility functions
std::tuple<double, double, int, int> readComplex(std::istringstream& stream);

// Data structures
struct LineData {
  std::vector<double> values;
  std::vector<int> decimal_places;
};

struct DiffStats {
  // track the maximum difference found
  double max = 0;
  double max_rounded = 0;
};

struct CountStats {
  // number of...
  long unsigned int lineNumber = 0;  // lines read
  long unsigned int elemNumber = 0;  // elements checked

  // non-zero differences found (independent of arguments)
  long unsigned int diff_non_zero = 0;  // zero (non-zero differences)
  long unsigned int diff_prec = 0;      // format precision threshold

  // differences found greater than user defined...
  long unsigned int diff_user = 0;   // nominal threshold ("good enough")
  long unsigned int diff_hard = 0;   // hard threshold (fail and exit)
  long unsigned int diff_print = 0;  // print threshold (for difference table)
};

struct Thresholds {
  // user-defined thresholds
  double user;   // lower threshold for significant difference (fail)
  double hard;   // threshold for critical difference (exit)
  double print;  // threshold for printing entry in table (print)

  // fixed thresholds
  // define epsilon when threshold is zero
  const double zero = pow(2, -23);  // equal to single precision epsilon

  const double marginal =
      110;  // upper threshold for significant difference (warning)
  // define the maximum valid value for TL (Transmission Loss)
  const double ignore =
      -20 *
      log10(pow(2, -23));  // threshold for meaningless difference (no action)
};

// Main class declaration
class FileComparator {
 private:
  bool new_fmt = false;
  unsigned long this_fmt_line;
  unsigned long this_fmt_column;
  unsigned long last_fmt_line;
  unsigned long this_line_ncols;

  DiffStats differ;
  CountStats counter;
  Thresholds thresh;

 public:
  // Constructor
  FileComparator(double user_thresh, double hard_thresh, double print_thresh)
      : thresh{user_thresh, hard_thresh, print_thresh} {};

  // ========================================================================
  // Public Interface
  // ========================================================================
  bool compareFiles(const std::string& file1, const std::string& file2);
  LineData parseLine(const std::string& line) const;
  /** @note the function parseLine() reads a line from the file and returns a
   LineData object */
  void printSummary(const std::string& file1, const std::string& file2,
                    int argc, char* argv[]) const;

 private:
  // ========================================================================
  // File Operations
  // ========================================================================
  bool openFiles(const std::string& file1, const std::string& file2,
                 std::ifstream& infile1, std::ifstream& infile2) const;
  long unsigned int getFileLength(const std::string& file) const;
  bool compareFileLengths(const std::string& file1,
                          const std::string& file2) const;

  // ========================================================================
  // Line/Column Processing
  // ========================================================================
  bool processLine(const LineData& data1, const LineData& data2,
                   std::vector<int>& dp_per_col, size_t& prev_n_col);
  bool processColumn(const LineData& data1, const LineData& data2,
                     size_t columnIndex, std::vector<int>& dp_per_col);

  // ========================================================================
  // Validation & Format Management
  // ========================================================================
  bool validateAndTrackColumnFormat(size_t n_col1, size_t n_col2,
                                    std::vector<int>& dp_per_col,
                                    size_t& prev_n_col);
  bool validateDecimalPlaces(int dp1, int dp2) const;
  bool ValidateDeciColumnSize(const std::vector<int>& dp_per_col,
                              size_t columnIndex) const;

  // ========================================================================
  // Decimal Places Management
  // ========================================================================
  // New methods for refactoring
  bool initializeDecimalPlaces(int min_dp, size_t columnIndex,
                               std::vector<int>& dp_per_col);
  bool updateDecimalPlacesFormat(int min_dp, size_t columnIndex,
                                 std::vector<int>& dp_per_col);
  double calculateThreshold(int decimal_places);

  // ========================================================================
  // Difference Processing
  // ========================================================================
  bool processDifference(double value1, double value2, int min_dp, int dp1,
                         int dp2, size_t columnIndex, double rangeValue);
  void processRawValues(double value1, double value2);
  void processRoundedValues(double rounded_diff, double minimum_deci);

  // ========================================================================
  // Output & Formatting
  // ========================================================================
  void printTable(size_t columnIndex, double line_threshold, double rangeValue,
                  double val1, int deci1, double val2, int deci2,
                  double diff_rounded);
  std::string formatNumber(double value, int prec, int maxIntegerWidth,
                           int maxDecimals) const;
  void printHardThresholdError(double rounded1, double rounded2,
                               double diff_rounded, size_t columnIndex) const;
  void printFormatInfo(int dp1, int dp2, size_t columnIndex) const;
    void printDiffLikeSummary(const std::string& file1,
                                  const std::string& file2, const int fmt_wid) const;
    void printRoundedSummary(const std::string& file1,
                                  const std::string& file2, const int fmt_wid) const;
    void printSignificantSummary(const std::string& file1,
                                  const std::string& file2, const int fmt_wid) const;

};

#endif  // UBAND_DIFF_H

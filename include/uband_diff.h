#ifndef UBAND_DIFF_H
#define UBAND_DIFF_H

#include <cmath>
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

  // differences found (where the difference is greater than
  // threshold, which is the minimum between the function argument
  // threshold and the minimum difference between the two files, based on format
  // precision)

  long unsigned int diff_print = 0;     // differences printed
  long unsigned int diff_non_zero = 0;  // non-zero differences
  long unsigned int diff_user = 0;      // user-defined differences
  long unsigned int diff_prec = 0;      // precision-related differences
  long unsigned int diff_hard = 0;      // hard threshold differences
};

// Main class declaration
class FileComparator {
 private:
  double threshold;
  double hard_threshold;
  bool new_fmt = false;
  unsigned long this_fmt_line;
  unsigned long this_fmt_column;
  unsigned long last_fmt_line;
  unsigned long this_line_ncols;
  // define the maximum valid value for TL (Transmission Loss)
  const double max_TL = -20 * log10(pow(2, -23));
  DiffStats differ;
  CountStats counter;

 public:
  FileComparator(double thresh, double hard_thresh)
      : threshold(thresh), hard_threshold(hard_thresh) {};
  // the function parseLine() reads a line from the file and returns a LineData
  // object
  LineData parseLine(const std::string& line) const;
  bool compareFiles(const std::string& file1, const std::string& file2);

 private:
  double calculateThreshold(int decimal_places);
  void updateCounters(double diff_rounded);
  long unsigned int getFileLength(const std::string& file) const;
  bool compareFileLengths(const std::string& file1, const std::string& file2) const;
  bool compareColumn(const LineData& data1, const LineData& data2,
                     size_t columnIndex, std::vector<int>& dp_per_col,
                     bool& new_fmt, bool& is_same);

  void printDifferenceRow(double rounded1, double rounded2, double diff_rounded,
                          double ithreshold, int dp1, int dp2,
                          size_t columnIndex, double rangeValue);

  bool updateDecimalPlaces(size_t columnIndex, int min_dp,
                           std::vector<int>& dp_per_col, bool& new_fmt);

  void printHardThresholdError(double rounded1, double rounded2,
                               double diff_rounded, size_t columnIndex) const;
  void printFormatInfo(int dp1, int dp2, size_t columnIndex) const;
  std::string formatNumber(double value, int prec, int maxIntegerWidth,
                                int maxDecimals) const;
};

#endif  // UBAND_DIFF_H

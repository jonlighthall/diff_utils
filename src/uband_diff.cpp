/**
 * @file uband_diff.cpp
 * @brief Implementation of file comparison utilities for numerical data with
 * support for complex numbers.
 *
 * This file provides the implementation for comparing two files containing
 * numerical data, including support for complex numbers in the format (real,
 * imag). The comparison is performed line by line and column by column, with
 * configurable thresholds for floating-point differences. The code tracks and
 * reports differences, handles changes in file format (such as column count or
 * precision), and prints detailed diagnostics for mismatches.
 *
 * Features:
 * - Supports real and complex number formats.
 * - Automatically detects and adapts to changes in column count and decimal
 * precision.
 * - Configurable thresholds for reporting differences.
 * - Detailed output with color-coded diagnostics for easy identification of
 * issues.
 * - Debugging macros for step-by-step tracing.
 *
 * @author J. Lighthall
 * @date January 2025
 */
#include "uband_diff.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#undef DEBUG3
#undef DEBUG2
#undef DEBUG

#ifdef DEBUG3  // print each step of the process
#ifndef DEBUG2
#define DEBUG2
#endif
#endif

#ifdef DEBUG2  // print status of every line
#ifndef DEBUG
#define DEBUG
#endif
#endif

bool isERROR = false;

// Implementation moved outside the class definition

// Global utility functions

// Function to count decimal places in a string token
auto stream_countDecimalPlaces = [](std::istringstream& stream) {
  // Peek ahead to determine the format of the number
  std::streampos pos = stream.tellg();
  std::string token;
  stream >> token;

  int ndp = 0;
  if (size_t decimal_pos = token.find('.'); decimal_pos != std::string::npos) {
    ndp = static_cast<int>(token.size() - decimal_pos - 1);
  }
#ifdef DEBUG3
  std::cout << "DEBUG3: " << token << " has ";
  std::cout << ndp << " decimal places" << std::endl;
#endif
  // Reset stream to before reading token, so the original extraction
  // works
  stream.clear();
  stream.seekg(pos);

  return ndp;
};

// readComplex() returns the real and imaginary parts of the complex
// number as separate values
// Note: This assumes the complex number is in the format (real, imag)
// where real and imag are floating point numbers, a comma is used as the
// separator, and the complex number is enclosed in parentheses.
// The function reads the complex number from the stream and returns a
// pair containing the real and imaginary parts as doubles. It assumes the
// leading '(' has already been read from the stream.)
// e.g. (1.0, 2.0)
// or (1.0, 2.0) with spaces around the comma
// or (1.0, 2.0) without spaces
// or (1.0,2.0)  with no spaces at all

std::tuple<double, double, int, int> readComplex(std::istringstream& stream) {
  // values
  double real;
  double imag;
  // dummy characters to read the complex number format
  char comma;
  char closeParen;
  stream >> real >> comma >> imag >> closeParen;
  if (comma == ',' && closeParen == ')') {
    // Save the current position (just after '(')
    std::streampos start_pos = stream.tellg();

    // Read the real part as a string to count decimal places
    stream.seekg(start_pos);
    std::string real_token;
    stream >> real_token;
    std::istringstream real_stream(real_token);
    int real_dp = stream_countDecimalPlaces(real_stream);

    // Move to the comma after reading real part
    stream.seekg(start_pos);
    double dummy_real;
    stream >> dummy_real >> comma;
    std::streampos imag_pos = stream.tellg();

    // Read the imag part as a string to count decimal places
    stream.seekg(imag_pos);
    std::string imag_token;
    stream >> imag_token;
    std::istringstream imag_stream(imag_token);
    int imag_dp = stream_countDecimalPlaces(imag_stream);

    return {real, imag, real_dp, imag_dp};
  } else {
    std::cerr << "Error reading complex number";
    std::exit(EXIT_FAILURE);
  }
}

auto round_to_decimals = [](double value, int precision) {
  double scale = std::pow(10.0, precision);
  return std::round(value * scale) / scale;
};

// ========================================================================
// Public Interface
// ========================================================================

bool FileComparator::compareFiles(const std::string& file1,
                                  const std::string& file2) {
  std::ifstream infile1;
  std::ifstream infile2;
  if (!openFiles(file1, file2, infile1, infile2)) {
    return false;
  }

  // individual line conents
  std::string line1;
  std::string line2;

  // track if the file format has changed
  long unsigned int prev_n_col = 0;
  std::vector<int> dp_per_col;

  // track if the files are the same
  bool is_same = false;

  std::cout << "   Max TL: \033[1;34m" << max_TL << "\033[0m" << std::endl;

  // Process files line by line
  while (std::getline(infile1, line1) && std::getline(infile2, line2)) {
    // increment the line number
    counter.lineNumber++;

    LineData data1 = parseLine(line1);
    LineData data2 = parseLine(line2);

    if (!processLine(data1, data2, dp_per_col, prev_n_col)) {
      return false;
    }
  }

  // Validate file lengths and return result
  if (!compareFileLengths(file1, file2)) {
    return false;
  }
  return is_same;
}

LineData FileComparator::parseLine(const std::string& line) const {
  LineData result;
  std::istringstream stream(line);
  char ch;

  while (stream >> ch) {
    // check if the numbers are complex and read them accordingly
    // check if string starts with '('
    if (ch == '(') {
      // read the complex number
      auto [real, imag, dp_real, dp_imag] = readComplex(stream);
      result.values.push_back(real);
      result.values.push_back(imag);
      result.decimal_places.push_back(dp_real);
      result.decimal_places.push_back(dp_imag);
    } else {
      // if the character is not '(', it is a number
      stream.putback(ch);  // put back the character to the stream
      int dp = stream_countDecimalPlaces(
          stream);  // count the number of decimal places

      if (dp < 0) {
        std::cerr << "Error: Negative number of decimal places found in line: "
                  << line << std::endl;
        isERROR = true;
        return result;  // return empty result
      }

      result.decimal_places.push_back(
          dp);  // store the number of decimal places

      double value;
      stream >> value;
      result.values.push_back(value);
    }
  }
  return result;
}

// ========================================================================
// File Operations
// ========================================================================

bool FileComparator::openFiles(const std::string& file1,
                               const std::string& file2, std::ifstream& infile1,
                               std::ifstream& infile2) {
  infile1.open(file1);
  infile2.open(file2);

  if (!infile1.is_open()) {
    std::cerr << "\033[1;31mError opening file: " << file1 << "\033[0m"
              << std::endl;
    isERROR = true;
    return false;
  }
  if (!infile2.is_open()) {
    std::cerr << "\033[1;31mError opening file: " << file2 << "\033[0m"
              << std::endl;
    isERROR = true;
    return false;
  }
  return true;
}

long unsigned int FileComparator::getFileLength(const std::string& file) const {
  std::ifstream infile(file);
  if (!infile.is_open()) {
    std::cerr << "\033[1;31mError opening file: " << file << "\033[0m"
              << std::endl;
    isERROR = true;
    return 0;
  }
  long unsigned int length = 0;
  std::string line;
  while (std::getline(infile, line)) {
    ++length;
  }
  infile.close();
  return length;
}

bool FileComparator::compareFileLengths(const std::string& file1,
                                        const std::string& file2) const {
  long unsigned int length1 = getFileLength(file1);

  if (long unsigned int length2 = getFileLength(file2); length1 != length2) {
    std::cerr << "\033[1;31mFiles have different lengths: " << file1 << " ("
              << length1 << " lines) and " << file2 << " (" << length2
              << " lines)\033[0m" << std::endl;

    std::cerr << "\033[1;31mFiles have different number of lines!\033[0m"
              << std::endl;
    if (counter.lineNumber != length1 || counter.lineNumber != length2) {
      std::cout << "   First " << counter.lineNumber << " lines ";
      if (counter.diff_user > 0) {
        std::cout << "match" << std::endl;
      } else {
        std::cout << "checked" << std::endl;
      }
      std::cout << "   " << counter.elemNumber << " elements checked"
                << std::endl;
    }
    std::cerr << "   File1 has " << length1 << " lines " << std::endl;
    std::cerr << "   File2 has " << length2 << " lines " << std::endl;

    return false;
  }
#ifdef DEBUG
  std::cout << "Files have the same number of lines: " << length1 << std::endl;
  std::cout << "Files have the same number of elements: " << counter.elemNumber
            << std::endl;
#endif

  return true;
}

// ========================================================================
// Line/Column Processing
// ========================================================================
/* Main Workflow */

bool FileComparator::processLine(const LineData& data1, const LineData& data2,
                                 std::vector<int>& dp_per_col,
                                 size_t& prev_n_col) {
  // Get the number of columns in each line
  if (data1.values.empty() || data2.values.empty()) {
    std::cerr << "Line " << counter.lineNumber << " has no values to compare!"
              << std::endl;
    isERROR = true;
    return false;
  }
  size_t n_col1 = data1.values.size();
  size_t n_col2 = data2.values.size();

  // print debug information if DEBUG2 is defined
#ifdef DEBUG2
  std::cout << "DEBUG2: Line " << counter.lineNumber << std::endl;
  std::cout << "   CONTENTS:" << std::endl;
  std::cout << "      file1:";
  for (size_t i = 0; i < n_col1; ++i) {
    int ndp = data1.decimal_places[i];
    std::cout << " " << std::setprecision(ndp) << data1.values[i] << "(" << ndp
              << ")";
  }
  std::cout << std::endl << "      file2:";
  for (size_t i = 0; i < n_col2; ++i) {
    int ndp = data2.decimal_places[i];
    std::cout << " " << std::setprecision(ndp) << data2.values[i] << "(" << ndp
              << ")";
  }
  std::cout << std::endl;
#endif

  // Validate column format
  if (!validateAndTrackColumnFormat(n_col1, n_col2, dp_per_col, prev_n_col)) {
    return false;
  }

  // Process each column
  for (size_t i = 0; i < n_col1; ++i) {
    if (!processColumn(data1, data2, i, dp_per_col)) {
      return false;
    }
  }

  return true;
}

bool FileComparator::processColumn(const LineData& data1, const LineData& data2,
                                   size_t columnIndex,
                                   std::vector<int>& dp_per_col) {
  processRawValues(data1.values[columnIndex], data2.values[columnIndex]);

  // get the number of decimal places for each column
  int dp1 = data1.decimal_places[columnIndex];
  int dp2 = data2.decimal_places[columnIndex];

  if (!validateDecimalPlaces(dp1, dp2)) {
    return false;
  }

  // Calculate minimum difference
  int min_dp = std::min(dp1, dp2);

  // Handle decimal places initialization and updates
  if (counter.lineNumber == 1) {
    if (!initializeDecimalPlaces(min_dp, columnIndex, dp_per_col)) {
      return false;
    }
  } else {
    if (!updateDecimalPlacesFormat(min_dp, columnIndex, dp_per_col)) {
      return false;
    }
  }

  // Print format info if needed
  if (dp1 != dp2 && new_fmt) {
    printFormatInfo(dp1, dp2, columnIndex);
  }

  // now that the minimum decimal places are determined, we can
  // round the values to the minimum decimal places
  // and compare them

  return processDifference(data1.values[columnIndex], data2.values[columnIndex],
                           min_dp, dp1, dp2, columnIndex, data1.values[0]);
}

// ========================================================================
// Validation & Format Management
// ========================================================================
bool FileComparator::validateAndTrackColumnFormat(size_t n_col1, size_t n_col2,
                                                  std::vector<int>& dp_per_col,
                                                  size_t& prev_n_col) {
  // Check if both lines have the same number of columns
  if (n_col1 != n_col2) {
    std::cerr << "Line " << counter.lineNumber
              << " has different number of columns!" << std::endl;
    return false;
  }

  this_line_ncols = n_col1;

  // Check if the number of columns has changed
  if (counter.lineNumber == 1) {
    prev_n_col = n_col1;  // initialize prev_n_col on first line
#ifdef DEBUG2
    std::cout << "   FORMAT: " << n_col1
              << " columns (both files) - initialized" << std::endl;
#endif
  }

  if (prev_n_col > 0 && n_col1 != prev_n_col) {
    std::cerr << "\033[1;31mNote: Number of columns changed at line "
              << counter.lineNumber << " (previous: " << prev_n_col
              << ", current: " << n_col1 << ")\033[0m" << std::endl;
    dp_per_col.clear();
    new_fmt = true;
    this_fmt_line = counter.lineNumber;
    std::cout << this_fmt_line << ": FMT number of columns has changed"
              << std::endl;
    std::cout << "format has changed" << std::endl;
  } else {
    if (counter.lineNumber > 1) {
#ifdef DEBUG3
      std::cout << "Line " << counter.lineNumber << " same column format"
                << std::endl;
#endif
      new_fmt = false;
    }
  }

  prev_n_col = n_col1;
  return true;
}

bool FileComparator::validateDecimalPlaces(int dp1, int dp2) const {
  if (dp1 < 0 || dp2 < 0) {
    std::cerr << "Line " << counter.lineNumber
              << " has negative number of decimal places!" << std::endl;
    isERROR = true;
    return false;
  }
  return true;
}

bool FileComparator::ValidateDeciColumnSize(std::vector<int>& dp_per_col,
                                            size_t columnIndex) const {
#ifdef DEBUG3
  for (size_t j = 0; j < dp_per_col.size(); ++j) {
    std::cout << "   minimum decimal places in column " << j + 1 << " = "
              << dp_per_col[j] << std::endl;
  }
#endif

#ifdef DEBUG2
  std::cout << "   size of dp_per_col: " << dp_per_col.size();
  std::cout << ", columnIndex: " << columnIndex + 1 << std::endl;
#endif

  // Validate vector size
  if (dp_per_col.size() != columnIndex + 1) {
    std::cerr << "Warning: dp_per_col size mismatch at line "
              << counter.lineNumber << std::endl;
    isERROR = true;
    std::cerr << "Expected size: " << columnIndex + 1
              << ", Actual size: " << dp_per_col.size() << std::endl;
    std::cerr << "Please check the input files for consistency." << std::endl;
    return false;
  }

  if (dp_per_col.size() <= columnIndex) {
    std::cerr << "Warning: dp_per_col size (" << dp_per_col.size()
              << ") insufficient for column " << columnIndex + 1 << " at line "
              << counter.lineNumber << std::endl;
    isERROR = true;
    std::cerr << "Please check the input files for consistency." << std::endl;
    std::cerr << "Expected at least " << columnIndex + 1 << " columns, but got "
              << dp_per_col.size() << std::endl;

    return false;
  }
  return true;
}

// ========================================================================
// Decimal Places Management
// ========================================================================
bool FileComparator::initializeDecimalPlaces(int min_dp, size_t columnIndex,
                                             std::vector<int>& dp_per_col) {
  // initialize the dp_per_col vector with the minimum decimal places
  dp_per_col.push_back(min_dp);

  if (!ValidateDeciColumnSize(dp_per_col, columnIndex)) return false;

#ifdef DEBUG2
  std::cout << "FORMAT: Line " << counter.lineNumber << " initialization"
            << std::endl;
  std::cout << "   dp_per_col: ";
  for (const auto& d : dp_per_col) {
    std::cout << d << " ";
  }
  std::cout << std::endl;
#endif

  // since this is an initialization, it is always a new format
  new_fmt = true;
  this_fmt_line = counter.lineNumber;
  this_fmt_column = columnIndex + 1;
  return true;
}

bool FileComparator::updateDecimalPlacesFormat(int min_dp, size_t columnIndex,
                                               std::vector<int>& dp_per_col) {
#ifdef DEBUG3
  std::cout << "not first line" << std::endl;
#endif

  // check if the minimum decimal places for this column has changed
  if (dp_per_col[columnIndex] != min_dp) {
    // If the minimum decimal places for this column is different from the
// previous minimum decimal places, update it
#ifdef DEBUG3
    std::cout << "DEBUG3: different" << std::endl;
    std::cout << "DEBUG3: format has changed" << std::endl;
#endif
    dp_per_col[columnIndex] = min_dp;
    new_fmt = true;
    this_fmt_line = counter.lineNumber;
    std::cout << this_fmt_line << ": FMT number of decimal places has changed"
              << std::endl;
  }
#ifdef DEBUG3
  else {
    // If the minimum decimal places for this column is the same as the
    // previous minimum decimal places, do nothing
    std::cout << "DEBUG3: same" << std::endl;
  }
#endif
  return true;
}

double FileComparator::calculateThreshold(int ndp) {
  // determine the smallest possible difference, base on the number of
  // decimal places (equal to the minimum number of decimal places for the
  // current element)
  double dp_threshold = std::pow(10, -ndp);

  if (new_fmt) {
    if (this_fmt_line != last_fmt_line) {
#ifdef DEBUG
      // group together all format specifications on the same line
      std::cout << "PRECISION: Line " << this_fmt_line;
      if (counter.lineNumber == 1) {
        std::cout << " (initial format)";
      } else {
        std::cout << " (change in format)";
      }
      std::cout << std::endl;
#endif
    }
    last_fmt_line = this_fmt_line;
#ifdef DEBUG
    // Set the width for line numbers based on the number of digits in
    // this_fmt_line
    auto line_num_width =
        static_cast<int>(std::to_string(this_line_ncols).length());
    // indent the column format specification under the line number
    std::cout << "   Column " << std::setw(line_num_width) << this_fmt_column
              << ": ";
    std::cout << ndp << " decimal places or 10^(" << -ndp
              << ") = " << std::setprecision(ndp) << dp_threshold << std::endl;
#endif
  }
  if (threshold < dp_threshold) {
#ifdef DEBUG
    if (new_fmt) {
      std::cout << "   \033[1;33mNOTE: " << dp_threshold
                << " is greater than the specified threshold: " << threshold
                << "\033[0m" << std::endl;
    }
#endif
    return dp_threshold;
  }
  return threshold;
}

// ========================================================================
// Difference Processing
// ========================================================================
bool FileComparator::processDifference(double value1, double value2, int min_dp,
                                       int dp1, int dp2, size_t columnIndex,
                                       double rangeValue) {
  // Calculate rounded values and process difference
  double rounded1 = round_to_decimals(value1, min_dp);
  double rounded2 = round_to_decimals(value2, min_dp);
  double diff_rounded = std::abs(rounded1 - rounded2);

  processRoundedValues(diff_rounded, min_dp);

  double ithreshold = calculateThreshold(min_dp);

  // Print differences if above plot threshold
  if (diff_rounded > print_threshold) {
    printTable(columnIndex, ithreshold, rangeValue, value1, dp1, value2, dp2,
               diff_rounded);
    std::cout << std::endl;
  } else {
    counter.elemNumber++;
#ifdef DEBUG2
    auto line_num_width =
        static_cast<int>(std::to_string(this_line_ncols).length());
    // std::cout << "ncols: " << this_line_ncols           << ", columnIndex: "
    // << columnIndex + 1 << std::endl;
    std::cout << "   DIFF: Values at line " << counter.lineNumber << ", column "
              << std::setw(line_num_width) << columnIndex + 1
              << " are equal: " << std::setprecision(min_dp) << rounded1
              << std::endl;
#endif
  }

  // Check hard threshold
  if ((diff_rounded > hard_threshold) &&
      ((rounded1 <= max_TL) && (rounded2 <= max_TL))) {
    counter.diff_hard++;
    printHardThresholdError(rounded1, rounded2, diff_rounded, columnIndex);
    return false;
  }

  return true;
}

void FileComparator::processRawValues(double value1, double value2) {
  // compare values (without rounding)
  double diff = std::abs(value1 - value2);
  if (diff > differ.max) {
    differ.max = diff;
  }

  if (diff > eps_zero) {
    counter.diff_non_zero++;
  }
}

void FileComparator::processRoundedValues(double rounded_diff,
                                          double minimum_deci) {
  // compare values (with rounding)

  if (rounded_diff > differ.max_rounded) {
    differ.max_rounded = rounded_diff;
  }

  //   double ieps = ithreshold * 0.1;

  //   // Check precision threshold
  //   if (double thresh_prec = ithreshold + ieps; diff_rounded > thresh_prec) {
  //     counter.diff_prec++;
  //   }

  const double zero_prec =
      pow(10, -(minimum_deci + 1));  // "zero" can be anything with more decimal
                                     // places than the minimum

  if (rounded_diff > zero_prec) {
    counter.diff_prec++;
  }

  // define epsilon for user-defined threshold

  const double user_threshold = threshold + eps_zero;

  if (rounded_diff > user_threshold) {
    counter.diff_user++;
  }
}

// ========================================================================
// Output & Formatting
// ========================================================================
void FileComparator::printTable(size_t columnIndex, double line_threshold,
                                double rangeValue, double val1, int deci1,
                                double val2, int deci2, double diff_rounded) {
  // Print a row in the difference table
  // Contents of the table:
  //    [0] the line number is printed
  //    [1] the column number is printed (1-based index)
  //    [2] the first value in the line is printed as "range"
  //    [3-4] the values in file1 and file2 are printed, with color coding if
  //    they
  //        exceed the maximum threshold (max_TL)
  //    [5] the threshold for the line is printed
  //    [6] the difference is printed, with color coding if it exceeds the
  //    threshold

  // This function is called when a difference is found that exceeds the
  // threshold

#ifdef DEBUG2
  std::cout << "   DIFF: Difference at line " << counter.lineNumber
            << ", column " << columnIndex + 1 << ": " << diff_rounded
            << " (threshold: " << line_threshold << ")" << std::endl;
#endif

  // define maximum display format for the values being printed
  int mxint = 4;                      // maximum integer width
  int mxdec = 5;                      // maximum decimal places
  int val_width = mxint + mxdec + 1;  // total width for value columns

  // define column widths
  std::vector<int> col_widths = {5, 5, val_width, val_width, val_width, 5, 4};

  auto padLeft = [](const std::string& str, int width) {
    if (static_cast<int>(str.length()) >= width) return str;
    return std::string(width - str.length(), ' ') + str;
  };

  if (counter.diff_print == 0) {
    std::cout << "DIFFERENCES:" << std::endl;
    if (threshold < print_threshold) {
      std::cout << "\033[1;33mWarning: Threshold for printing ("
                << print_threshold << ") is less than the overall threshold ("
                << threshold << "). Some significant differences may not be "
                << "printed.\033[0m" << std::endl;
    }

    // Print header if this is the first difference
    std::cout << std::setw(col_widths[0]) << "line";
    std::cout << std::setw(col_widths[1]) << "col";
    std::cout << std::setw(col_widths[2]) << "range";
    std::cout << std::setw(col_widths[3] + 2) << "file1";
    std::cout << std::setw(col_widths[4] + 2) << "file2 |";
    std::cout << padLeft(" thres |", col_widths[5] + 2);
    std::cout << padLeft("diff", col_widths[6] + 2) << std::endl;

    std::cout << "-------------------------------------------+-------+------"
              << std::endl;
  }
  counter.diff_print++;

  /* PRINT DIFF TABLE ENTRY */
  // line
  std::cout << std::setw(col_widths[0]) << counter.lineNumber;
  // column
  std::cout << std::setw(col_widths[1]) << columnIndex + 1;

  // range (first value in the line)
  std::cout << std::fixed << std::setprecision(2) << std::setw(col_widths[2])
            << rangeValue << " ";

  // values in file1
  if (val1 > max_TL) {
    std::cout << "\033[1;34m";
  }
  std::cout << formatNumber(val1, deci1, mxint, mxdec);

  std::cout << "\033[0m ";

  // values in file2
  if (val2 > max_TL) {
    std::cout << "\033[1;34m";
  }
  std::cout << formatNumber(val2, deci2, mxint, mxdec);
  std::cout << "\033[0m" << " | ";

  // threshold
  if (line_threshold > threshold) {
    std::cout << "\033[1;33m";
  }
  std::cout << std::setw(col_widths[5]) << std::setprecision(3)
            << line_threshold << "\033[0m | ";

  double ieps = line_threshold * 0.1;
  double thresh_prec = (line_threshold + ieps);

  if (diff_rounded > thresh_prec) {
    counter.diff_prec++;
  }

  // difference
  if (val1 > max_TL || val2 > max_TL) {
    std::cout << "\033[1;34m";
  } else if (diff_rounded > threshold && diff_rounded < thresh_prec) {
    std::cout << "\033[1;33m";
  } else if (diff_rounded > hard_threshold) {
    std::cout << "\033[1;31m";
    isERROR = true;
  } else {
    std::cout << "\033[0m";
  }

  std::cout << std::setw(col_widths[6]) << diff_rounded << "\033[0m";

  // std::cout << " > " << ithreshold << " : " << (diff_rounded -
  // ithreshold)
  //           << " -> " << fabs(diff_rounded - ithreshold) << " > " <<
  //           ieps;
}

std::string FileComparator::formatNumber(double value, int prec,
                                         int maxIntegerWidth,
                                         int maxDecimals) const {
  //   // Error handling for input arguments
  //   if (prec < 0) {

  //     std::cerr << "Error: Negative precision in formatNumber: " << prec
  //               << std::endl;
  //     prec = 0;
  //   }
  //   if (maxIntegerWidth < 1) {
  //     std::cerr << "Error: maxIntegerWidth must be positive in formatNumber:
  //     "
  //               << maxIntegerWidth << std::endl;
  //     maxIntegerWidth = 1;
  //   }
  //   if (maxDecimals < 0) {
  //     std::cerr << "Error: maxDecimals must be non-negative in formatNumber:
  //     "
  //               << maxDecimals << std::endl;
  //     maxDecimals = 0;
  //   }

  // convert the number to a string with the specified precision
  std::ostringstream oss;

  int iprec = prec;  // Use the provided precision
  if (prec > maxDecimals) {
    iprec = maxDecimals;  // Ensure precision is within bounds
  }

  oss << std::fixed << std::setprecision(iprec) << value;
  std::string numStr = oss.str();

  // Find position of decimal point
  size_t dotPos = numStr.find('.');

  // Calculate padding for integer part
  int intWidth = (dotPos != std::string::npos)
                     ? static_cast<int>(dotPos)
                     : static_cast<int>(numStr.length());

  int padding_width = maxIntegerWidth - intWidth;
  // Prevent negative padding
  if (padding_width < 0) {
    std::cout << std::endl
              << "maxIntegerWidth: " << maxIntegerWidth << std::endl
              << "intWidth: " << intWidth << std::endl
              << "padding_width: " << padding_width << std::endl;
    std::cerr << "Error: Negative padding width in formatNumber: "
              << padding_width << std::endl;
    padding_width = 0;  // Prevent negative padding
  }

  int padRight = maxDecimals - iprec;
  if (padRight < 0) padRight = 0;  // Ensure no negative padding

  return std::string(padding_width, ' ') + numStr + std::string(padRight, ' ');
}

void FileComparator::printHardThresholdError(double rounded1, double rounded2,
                                             double diff_rounded,
                                             size_t columnIndex) const {
  std::cerr << "\033[1;31mLarge difference found at line " << counter.lineNumber
            << ", column " << columnIndex + 1 << "\033[0m" << std::endl;

  if (counter.lineNumber > 0) {
    std::cout << "   First " << counter.lineNumber - 1 << " lines match"
              << std::endl;
  }
  if (counter.elemNumber > 0) {
    std::cout << "   " << counter.elemNumber << " element";
    if (counter.elemNumber > 1) std::cout << "s";
    std::cout << " checked" << std::endl;
  }

  std::cout << counter.diff_print << " with differences between " << threshold
            << " and " << hard_threshold << std::endl;

  std::cout << "   File1: " << std::setw(7) << rounded1 << std::endl;
  std::cout << "   File2: " << std::setw(7) << rounded2 << std::endl;
  std::cout << "    diff: \033[1;31m" << std::setw(7) << diff_rounded
            << "\033[0m" << std::endl;
}

void FileComparator::printFormatInfo(int dp1, int dp2,
                                     size_t columnIndex) const {
#ifdef DEBUG2
  std::cout << "   NEW FORMAT" << std::endl;
#endif
// print the format
#ifdef DEBUG
  std::cout << "DEBUG : Line " << counter.lineNumber << ", Column "
            << columnIndex + 1 << std::endl;
  std::cout << "   FORMAT: number of decimal places file1: " << dp1
            << ", file2: " << dp2 << std::endl;
#endif
}

void printbar(int indent = 0) {
  for (int i = 0; i < indent; ++i) std::cout << "   ";
  std::cout << "---------------------------------------------------------"
            << std::endl;
}

void FileComparator::printSummary(const std::string& file1,
                                  const std::string& file2, int argc,
                                  char* argv[]) const {
  std::cout << "ARGUMENTS:" << std::endl;
  // print command line arguments
  std::cout << "   Input:";
  for (int i = 0; i < argc; ++i) {
    std::cout << " " << argv[i];
  }
  std::cout << std::endl;
  std::cout << "   File1: " << file1 << std::endl;
  std::cout << "   File2: " << file2 << std::endl;

  std::cout << "STATISTICS:" << std::endl;
  std::cout << "   Total lines compared: " << counter.lineNumber;

  // Check if all lines were compared
  size_t length1 = getFileLength(file1);
  if (length1 == counter.lineNumber) {
    std::cout << " (all)" << std::endl;
  } else {
    std::cout << " of " << length1 << std::endl;
    // print how many lines were not compared
    size_t missing_lines = length1 - counter.lineNumber;
    std::cout << "\033[1;31m   " << missing_lines
              << " lines were not compared\033[0m" << std::endl;
  }

  std::cout << "   Total elements checked: " << counter.elemNumber << std::endl;

  std::cout << "SUMMARY:" << std::endl;
  // Calculate the width for formatting
  auto fmt_wid = static_cast<int>(std::to_string(counter.elemNumber).length());

  // Diff-like differences
  // =========================================================
  if (counter.diff_non_zero == 0) {
    std::cout << "\033[1;32m   Files " << file1 << " and " << file2
              << " are identical\033[0m" << std::endl;
    return;
  }
  if (counter.elemNumber > counter.diff_non_zero) {
    const size_t zero_diff = counter.elemNumber - counter.diff_non_zero;
    if (zero_diff > 0) {
      std::cout << "   Exact matches        ( =" << 0.0
                << "): " << std::setw(fmt_wid) << zero_diff << std::endl;
    }
  }

  std::cout << "   Non-zero differences ( >" << 0.0
            << "): " << std::setw(fmt_wid) << counter.diff_non_zero
            << std::endl;

  std::cout << "\033[1;33m   Files " << file1 << " and " << file2
            << " are different\033[0m" << std::endl;

  if (counter.diff_print < counter.diff_non_zero) {
    std::cout << "   Printed differences  ( >" << print_threshold
              << "): " << std::setw(fmt_wid) << counter.diff_print << std::endl;
    size_t not_printed = counter.diff_non_zero - counter.diff_print;
    if (not_printed > 0) {
      std::cout << "   Not printed          (<=" << print_threshold
                << "): " << std::setw(fmt_wid) << not_printed << std::endl;
    }
  } else {
    std::cout << "   All non-zero "
                 "differences are printed."
              << std::endl;
  }

  if (differ.max > eps_zero) {
    std::cout << "   Maximum difference: " << differ.max << std::endl;
  }

  printbar(1);

  // Trivial differences
  // =========================================================
  if (counter.diff_prec == 0) {
    std::cout << "\033[1;32m   Files " << file1 << " and " << file2
              << " are identical with trivial differences due to "
                 "formatting\033[0m"
              << std::endl;
    return;
  }
  if (counter.diff_non_zero > counter.diff_prec) {
    size_t zero_diff = counter.diff_non_zero - counter.diff_prec;
    if (zero_diff > 0) {
      std::cout << "   Trivial differences     ( >" << 0.0
                << "): " << std::setw(fmt_wid) << zero_diff << std::endl;
    }
  }
  std::cout << "   Non-trivial differences          : " << std::setw(fmt_wid)
            << counter.diff_prec << std::endl;

  std::cout << "\033[1;33m   Files " << file1 << " and " << file2
            << " are non-trivially different\033[0m" << std::endl;

  if (counter.diff_print < counter.diff_prec) {
    std::cout << "   Printed differences     ( >" << print_threshold
              << "): " << std::setw(fmt_wid) << counter.diff_print << std::endl;
    size_t not_printed = counter.diff_prec - counter.diff_print;
    if (not_printed > 0) {
      std::cout << "   Not printed differences (<=" << print_threshold
                << "): " << std::setw(fmt_wid) << not_printed << std::endl;
    }
  } else {
    std::cout << "   All non-trivial "
                 "differences are printed."
              << std::endl;
  }

  std::cout << "   Maximum rounded difference: " << differ.max_rounded
            << std::endl;
  printbar(1);

  // User-defined threshold differences
  // =========================================================
  if (counter.diff_user == 0) {
    std::cout << "\033[1;32m   Files " << file1 << " and " << file2
              << " are identical within tolerance\033[0m" << std::endl;
    return;
  }
  std::cout << "   Significant differences   ( >" << threshold
            << "): " << std::setw(fmt_wid) << counter.diff_user << std::endl;
  if (counter.diff_prec > counter.diff_user) {
    size_t zero_diff = counter.diff_prec - counter.diff_user;
    if (zero_diff > 0) {
      std::cout << "   Insignificant differences (<=" << threshold
                << "): " << std::setw(fmt_wid) << zero_diff << std::endl;
    }
  }
  std::cout << "\033[1;31m   Files " << file1 << " and " << file2
            << " are significantly different\033[0m" << std::endl;

  if (counter.diff_print < counter.diff_user) {
    std::cout << "   Printed differences      ( >" << print_threshold
              << "): " << std::setw(fmt_wid) << counter.diff_print << std::endl;
    size_t not_printed_signif = counter.diff_user - counter.diff_print;
    if (not_printed_signif > 0) {
      std::cout << "\033[1;31m   Not printed differences  (<="
                << print_threshold << "): " << std::setw(fmt_wid)
                << not_printed_signif << "\033[0m" << std::endl;
    }
  } else {
    std::cout << "   All significant "
                 "differences are printed."
              << std::endl;
  }

  printbar(1);

  // Print theshold differences
  // =========================================================
  if (counter.diff_print == 0) {
    std::cout << "\033[1;32m   Files " << file1 << " and " << file2
              << " are identical within print threshold\033[0m" << std::endl;
    return;
  }
  std::cout << "   Printed differences      ( >" << print_threshold
            << "): " << std::setw(fmt_wid) << counter.diff_print << std::endl;

  if (threshold < print_threshold) {
    if (counter.diff_user > counter.diff_print) {
      size_t not_printed_signif = counter.diff_user - counter.diff_print;
      if (not_printed_signif > 0) {
        std::cout << "\033[1;31m   Not printed differences  (<="
                  << print_threshold << "): " << std::setw(fmt_wid)
                  << not_printed_signif << "\033[0m" << std::endl;
      }
    }
  } else {
    if (counter.diff_prec > counter.diff_print) {
      size_t not_printed = counter.diff_prec - counter.diff_print;
      if (not_printed > 0) {
        std::cout << "   Not printed differences  (<=" << print_threshold
                  << "): " << std::setw(fmt_wid) << not_printed << std::endl;
      }
    }
  }

  printbar(1);
  // Hard threshold differences
  // =========================================================
  if (counter.diff_hard == 0) {
  }

  if (counter.diff_hard > 0) {
    std::cout << "\033[1;31m   Differences exceeding hard threshold ("
              << hard_threshold << "): " << counter.diff_hard << "\033[0m"
              << std::endl;
  } else {
    std::cout << "   No differences exceeding hard threshold found."
              << std::endl;

    if (counter.diff_print == 0) {
      std::cout << "\033[1;32m   Files " << file1 << " and " << file2
                << " are identical.\033[0m" << std::endl;
    }
  }
}

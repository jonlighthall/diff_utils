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

  if (counter.diff_print > 0) {
    std::cout << "Total differences: " << counter.diff_print << " less than "
              << hard_threshold << std::endl;
  } else {
    is_same = true;
  }
  std::cout << "Max difference: " << differ.max << std::endl;

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

void FileComparator::printTable(size_t columnIndex, double line_threshold,
                                double rangeValue, double val1, int deci1,
                                double val2, int deci2, double diff_rounded) {
  // Print a row in the difference table
  // This function is called when a difference is found that exceeds the
  // threshold

#ifdef DEBUG2
  std::cout << "   DIFF: Difference at line " << counter.lineNumber
            << ", column " << columnIndex + 1 << ": " << diff_rounded
            << " (threshold: " << line_threshold << ")" << std::endl;
#endif

  int mxint = 4;                      // maximum integer width for range
  int mxdec = 5;                      // maximum decimal places for range
  int val_width = mxint + mxdec + 1;  // total width for value columns

  auto padLeft = [](const std::string& str, int width) {
    // if (str.empty()) {
    //   std::cerr << "Error: padLeft received an empty string" << std::endl;

    //   return std::string(width, ' ');
    // }

    // if (width < 0) {
    //   std::cerr << "Error: padLeft received negative width: " << width
    //             << std::endl;
    //   width = 0;
    // }

    if (static_cast<int>(str.length()) >= width) return str;
    return std::string(width - str.length(), ' ') + str;
  };

  if (counter.diff_print == 0) {
    // Print header if this is the first difference
    std::cout << " line  col    range " << padLeft("tl1", val_width) << " "
              << padLeft("tl2", val_width) << " | thres | diff" << std::endl;
    std::cout << "----------------------------------------+-------+------"
              << std::endl;
  }
  counter.diff_print++;

  /* PRINT DIFF TABLE ENTRY */
  // line
  std::cout << std::setw(5) << counter.lineNumber;
  // column
  std::cout << std::setw(5) << columnIndex + 1;

  // range (first value in the line)
  std::cout << std::fixed << std::setprecision(2) << std::setw(val_width)
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
  std::cout << std::setw(5) << std::setprecision(3) << line_threshold
            << "\033[0m | ";

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

  std::cout << std::setw(4) << diff_rounded << "\033[0m";

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
//     std::cerr << "Error: maxIntegerWidth must be positive in formatNumber: "
//               << maxIntegerWidth << std::endl;
//     maxIntegerWidth = 1;
//   }
//   if (maxDecimals < 0) {
//     std::cerr << "Error: maxDecimals must be non-negative in formatNumber: "
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

void FileComparator::updateCounters(double diff_rounded) {
  // Update counters

  // define epsilon when threshold is zero
  double eps_zero = pow(2, -23);  // equal to single precision epsilon
  if (diff_rounded > eps_zero) {
    counter.diff_non_zero++;
  }

  // define epsilon for user-defined threshold
  double eps_user = (threshold == 0) ? eps_zero : threshold * 0.1;
  if (diff_rounded > (threshold + eps_user)) {
    counter.diff_user++;
  }
}

double FileComparator::calculateThreshold(int ndp) {
  // determine the comparison threshold
  double dp_threshold = std::pow(10, -ndp);

  if (new_fmt) {
    if (this_fmt_line != last_fmt_line) {
      std::cout << "PRECISION: Line " << this_fmt_line;
      if (counter.lineNumber == 1) {
        std::cout << " (initial format)";
      } else {
        std::cout << " (change in format)";
      }
      std::cout << std::endl;
    }
    last_fmt_line = this_fmt_line;
#ifdef DEBUG
    // Set the width for line numbers based on the number of digits in
    // this_fmt_line
    auto line_num_width =
        static_cast<int>(std::to_string(this_line_ncols).length());
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

// Implementation moved outside the class definition

bool FileComparator::compareFileLengths(const std::string& file1,
                                        const std::string& file2) const {
  long unsigned int length1 = getFileLength(file1);

  if (long unsigned int length2 = getFileLength(file2); length1 != length2) {
    std::cerr << "\033[1;31mFiles have different lengths: " << file1 << " ("
              << length1 << " bytes) and " << file2 << " (" << length2
              << " bytes)\033[0m" << std::endl;

    std::cerr << "\033[1;31mFiles have different number of lines!\033[0m"
              << std::endl;
    if (counter.lineNumber != length1 || counter.lineNumber != length2) {
      std::cout << "   First " << counter.lineNumber << " lines match"
                << std::endl;
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

void FileComparator::checkMaxDiff(double value1, double value2) {
  // compare values (without rounding)
  if (double diff = std::abs(value1 - value2); diff > differ.max) {
    differ.max = diff;
  }
}

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
  checkMaxDiff(data1.values[columnIndex], data2.values[columnIndex]);

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

  // Calculate rounded values and process difference
  double rounded1 = round_to_decimals(data1.values[columnIndex], min_dp);
  double rounded2 = round_to_decimals(data2.values[columnIndex], min_dp);
  double diff_rounded = std::abs(rounded1 - rounded2);

  if (diff_rounded > differ.max_rounded) {
    differ.max_rounded = diff_rounded;
  }

  return processDifference(rounded1, rounded2, diff_rounded, columnIndex, dp1,
                           dp2, data1.values[0], min_dp);
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

bool FileComparator::initializeDecimalPlaces(int min_dp, size_t columnIndex,
                                             std::vector<int>& dp_per_col) {
  // initialize the dp_per_col vector with the minimum decimal places
  dp_per_col.push_back(min_dp);

  if (!ValidateDeciColumnSize(dp_per_col, columnIndex)) return false;

  // #ifdef DEBUG3
  std::cout << "FORMAT: Line " << counter.lineNumber << " initialization"
            << std::endl;
  std::cout << "   dp_per_col: ";
  for (const auto& d : dp_per_col) {
    std::cout << d << " ";
  }
  std::cout << std::endl;
  // #endif

  new_fmt = true;
  this_fmt_line = counter.lineNumber;
  this_fmt_column = columnIndex + 1;

  return true;
}

bool FileComparator::updateDecimalPlacesFormat(int min_dp, size_t columnIndex,
                                               std::vector<int>& dp_per_col) {
#ifdef DEBUG3
  std::cout << "not line 1" << std::endl;
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

bool FileComparator::ValidateDeciColumnSize(std::vector<int>& dp_per_col,
                                            size_t columnIndex) const {
  // #ifdef DEBUG3
  for (size_t j = 0; j < dp_per_col.size(); ++j) {
    std::cout << "   minimum decimal places in column " << j + 1 << " = "
              << dp_per_col[j] << std::endl;
  }
  // #endif

  std::cout << "   size of dp_per_col: " << dp_per_col.size();
  std::cout << ", columnIndex: " << columnIndex + 1 << std::endl;

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

bool FileComparator::processDifference(double rounded1, double rounded2,
                                       double diff_rounded, size_t columnIndex,
                                       int dp1, int dp2, double rangeValue,
                                       int min_dp) {
  updateCounters(diff_rounded);

  double ithreshold = calculateThreshold(min_dp);
  double ieps = ithreshold * 0.1;

  // Check precision threshold
  if (double thresh_prec = ithreshold + ieps; diff_rounded > thresh_prec) {
    counter.diff_prec++;
  }

  // Print differences if above plot threshold
  if (double thresh_plot = 0; diff_rounded > thresh_plot) {
    printTable(columnIndex, ithreshold, rangeValue, rounded1, dp1, rounded2,
               dp2, diff_rounded);
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

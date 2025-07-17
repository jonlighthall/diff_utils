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

std::tuple<double, double, int, int> readComplex(std::istringstream& stream,
                                                 Flags& flag) {
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
    flag.error_found = true;
    return {real, imag, -1, -1};  // Return -1 for decimal places on error
  }
}

auto round_to_decimals = [](double value, int precision) {
  double scale = std::pow(10.0, precision);
  return std::round(value * scale) / scale;
};

// ========================================================================
// Public Interface
// ========================================================================

bool FileComparator::compare_files(const std::string& file1,
                                   const std::string& file2) {
  std::ifstream infile1;
  std::ifstream infile2;
  if (!open_files(file1, file2, infile1, infile2)) {
    return false;
  }

  // individual line conents
  std::string line1;
  std::string line2;

  // track if the file format has changed
  size_t prev_n_col = 0;
  std::vector<int> dp_per_col;

  std::cout << "   Max TL: \033[1;34m" << thresh.ignore << "\033[0m"
            << std::endl;

  // Process files line by line
  while (std::getline(infile1, line1) && std::getline(infile2, line2)) {
    // increment the line number
    counter.line_number++;

    LineData data1 = parse_line(line1);
    LineData data2 = parse_line(line2);

    if (!process_line(data1, data2, dp_per_col, prev_n_col)) {
      return false;
    }
  }

  // Validate file lengths and return result
  if (!compare_file_lengths(file1, file2)) {
    return false;
  }
  return flag.files_are_close_enough && !flag.error_found;
}

LineData FileComparator::parse_line(const std::string& line) const {
  LineData result;
  std::istringstream stream(line);
  char ch;

  while (stream >> ch) {
    // check if the numbers are complex and read them accordingly
    // check if string starts with '('
    if (ch == '(') {
      // read the complex number
      auto [real, imag, dp_real, dp_imag] = readComplex(stream, flag);
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
        flag.error_found = true;
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

bool FileComparator::open_files(const std::string& file1,
                                const std::string& file2,
                                std::ifstream& infile1,
                                std::ifstream& infile2) const {
  infile1.open(file1);
  infile2.open(file2);

  if (!infile1.is_open()) {
    std::cerr << "\033[1;31mError opening file: " << file1 << "\033[0m"
              << std::endl;
    flag.error_found = true;
    return false;
  }
  if (!infile2.is_open()) {
    std::cerr << "\033[1;31mError opening file: " << file2 << "\033[0m"
              << std::endl;
    flag.error_found = true;
    return false;
  }
  return true;
}

size_t FileComparator::get_file_length(const std::string& file) const {
  std::ifstream infile(file);
  if (!infile.is_open()) {
    std::cerr << "\033[1;31mError opening file: " << file << "\033[0m"
              << std::endl;
    flag.error_found = true;
    return 0;
  }
  size_t length = 0;
  std::string line;
  while (std::getline(infile, line)) {
    ++length;
  }
  infile.close();
  return length;
}

bool FileComparator::compare_file_lengths(const std::string& file1,
                                          const std::string& file2) const {
  size_t length1 = get_file_length(file1);

  // check if the file lengths are equal
  // if the files have different lengths, this is a failure, not an error
  if (size_t length2 = get_file_length(file2); length1 != length2) {
    std::cerr << "\033[1;31mFiles have different lengths: " << file1 << " ("
              << length1 << " lines) and " << file2 << " (" << length2
              << " lines)\033[0m" << std::endl;

    std::cerr << "\033[1;31mFiles have different number of lines!\033[0m"
              << std::endl;
    if (counter.line_number != length1 || counter.line_number != length2) {
      std::cout << "   First " << counter.line_number << " lines ";
      if (counter.diff_significant > 0) {
        std::cout << "match" << std::endl;
      } else {
        std::cout << "checked" << std::endl;
      }
      std::cout << "   " << counter.elem_number << " elements checked"
                << std::endl;
    }
    std::cerr << "   File1 has " << length1 << " lines " << std::endl;
    std::cerr << "   File2 has " << length2 << " lines " << std::endl;

    return false;
  }
#ifdef DEBUG
  std::cout << "Files have the same number of lines: " << length1 << std::endl;
  std::cout << "Files have the same number of elements: " << counter.elem_number
            << std::endl;
#endif

  return true;
}

// ========================================================================
// Line/Column Processing
// ========================================================================
/* Main Workflow */

bool FileComparator::process_line(const LineData& data1, const LineData& data2,
                                  std::vector<int>& dp_per_col,
                                  size_t& prev_n_col) {
  // Get the number of columns in each line
  if (data1.values.empty() || data2.values.empty()) {
    std::cerr << "Line " << counter.line_number << " has no values to compare!"
              << std::endl;
    flag.error_found = true;
    return false;
  }
  size_t n_col1 = data1.values.size();
  size_t n_col2 = data2.values.size();

  // print debug information if DEBUG2 is defined
#ifdef DEBUG2
  std::cout << "DEBUG2: Line " << counter.line_number << std::endl;
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
  if (!validate_and_track_column_format(n_col1, n_col2, dp_per_col,
                                        prev_n_col)) {
    return false;
  }

  // Process each column
  for (size_t i = 0; i < n_col1; ++i) {
    if (!process_column(data1, data2, i, dp_per_col)) {
      return false;
    }
  }
  return true;
}

bool FileComparator::process_column(const LineData& data1,
                                    const LineData& data2, size_t column_index,
                                    std::vector<int>& dp_per_col) {
  process_raw_values(data1.values[column_index], data2.values[column_index]);

  // get the number of decimal places for each column
  int dp1 = data1.decimal_places[column_index];
  int dp2 = data2.decimal_places[column_index];

  if (!validate_decimal_places(dp1, dp2)) {
    return false;
  }

  // Calculate minimum difference
  int min_dp = std::min(dp1, dp2);

  // Handle decimal places initialization and updates
  if (counter.line_number == 1) {
    if (!initialize_decimal_places(min_dp, column_index, dp_per_col)) {
      return false;
    }
  } else {
    if (!update_decimal_places_format(min_dp, column_index, dp_per_col)) {
      return false;
    }
  }

  // now that the minimum decimal places are determined, we can
  // round the values to the minimum decimal places
  // and compare them

  ColumnValues column_data = extract_column_values(data1, data2, column_index);

  // Print format info if needed
  if (dp1 != dp2 && flag.new_fmt) {
    print_format_info(column_data, column_index);
  }

  return process_difference(column_data, column_index);
}

// ========================================================================
// Validation & Format Management
// ========================================================================
bool FileComparator::validate_and_track_column_format(
    size_t n_col1, size_t n_col2, std::vector<int>& dp_per_col,
    size_t& prev_n_col) {
  // Check if both lines have the same number of columns
  if (n_col1 != n_col2) {
    std::cerr << "Line " << counter.line_number
              << " has different number of columns!" << std::endl;
    return false;
  }

  this_line_ncols = n_col1;

  // Check if the number of columns has changed
  if (counter.line_number == 1) {
    prev_n_col = n_col1;  // initialize prev_n_col on first line
#ifdef DEBUG2
    std::cout << "   FORMAT: " << n_col1
              << " columns (both files) - initialized" << std::endl;
#endif
  }

  if (prev_n_col > 0 && n_col1 != prev_n_col) {
    std::cerr << "\033[1;31mNote: Number of columns changed at line "
              << counter.line_number << " (previous: " << prev_n_col
              << ", current: " << n_col1 << ")\033[0m" << std::endl;
    dp_per_col.clear();
    flag.new_fmt = true;
    this_fmt_line = counter.line_number;
    std::cout << this_fmt_line << ": FMT number of columns has changed"
              << std::endl;
    std::cout << "format has changed" << std::endl;
  } else {
    if (counter.line_number > 1) {
#ifdef DEBUG3
      std::cout << "Line " << counter.line_number << " same column format"
                << std::endl;
#endif
      flag.new_fmt = false;
    }
  }
  prev_n_col = n_col1;
  return true;
}

bool FileComparator::validate_decimal_places(int dp1, int dp2) const {
  if (dp1 < 0 || dp2 < 0) {
    std::cerr << "Line " << counter.line_number
              << " has negative number of decimal places!" << std::endl;
    flag.error_found = true;
    return false;
  }
  return true;
}

bool FileComparator::validate_decimal_column_size(
    const std::vector<int>& dp_per_col, size_t column_index) const {
#ifdef DEBUG3
  for (size_t j = 0; j < dp_per_col.size(); ++j) {
    std::cout << "   minimum decimal places in column " << j + 1 << " = "
              << dp_per_col[j] << std::endl;
  }
#endif

#ifdef DEBUG2
  std::cout << "   size of dp_per_col: " << dp_per_col.size();
  std::cout << ", column_index: " << column_index + 1 << std::endl;
#endif

  // Validate vector size
  if (dp_per_col.size() != column_index + 1) {
    std::cerr << "Warning: dp_per_col size mismatch at line "
              << counter.line_number << std::endl;
    flag.error_found = true;
    std::cerr << "Expected size: " << column_index + 1
              << ", Actual size: " << dp_per_col.size() << std::endl;
    std::cerr << "Please check the input files for consistency." << std::endl;
    return false;
  }

  if (dp_per_col.size() <= column_index) {
    std::cerr << "Warning: dp_per_col size (" << dp_per_col.size()
              << ") insufficient for column " << column_index + 1 << " at line "
              << counter.line_number << std::endl;
    flag.error_found = true;
    std::cerr << "Please check the input files for consistency." << std::endl;
    std::cerr << "Expected at least " << column_index + 1
              << " columns, but got " << dp_per_col.size() << std::endl;
    return false;
  }
  return true;
}

// ========================================================================
// Decimal Places Management
// ========================================================================
bool FileComparator::initialize_decimal_places(int min_dp, size_t column_index,
                                               std::vector<int>& dp_per_col) {
  // initialize the dp_per_col vector with the minimum decimal places
  dp_per_col.push_back(min_dp);

  if (!validate_decimal_column_size(dp_per_col, column_index)) return false;

#ifdef DEBUG2
  std::cout << "FORMAT: Line " << counter.line_number << " initialization"
            << std::endl;
  std::cout << "   dp_per_col: ";
  for (const auto& d : dp_per_col) {
    std::cout << d << " ";
  }
  std::cout << std::endl;
#endif

  // since this is an initialization, it is always a new format
  flag.new_fmt = true;
  this_fmt_line = counter.line_number;
  this_fmt_column = column_index + 1;
  return true;
}

bool FileComparator::update_decimal_places_format(
    int min_dp, size_t column_index, std::vector<int>& dp_per_col) {
#ifdef DEBUG3
  std::cout << "not first line" << std::endl;
#endif

  // check if the minimum decimal places for this column has changed
  if (dp_per_col[column_index] != min_dp) {
    // If the minimum decimal places for this column is different from the
// previous minimum decimal places, update it
#ifdef DEBUG3
    std::cout << "DEBUG3: different" << std::endl;
    std::cout << "DEBUG3: format has changed" << std::endl;
#endif
    dp_per_col[column_index] = min_dp;
    flag.new_fmt = true;
    this_fmt_line = counter.line_number;
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

double FileComparator::calculate_threshold(int ndp) {
  // determine the smallest possible difference, base on the number of
  // decimal places (equal to the minimum number of decimal places for the
  // current element)
  double dp_threshold = std::pow(10, -ndp);

  if (flag.new_fmt) {
    if (this_fmt_line != last_fmt_line) {
#ifdef DEBUG
      // group together all format specifications on the same line
      std::cout << "PRECISION: Line " << this_fmt_line;
      if (counter.line_number == 1) {
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
  if (thresh.significant < dp_threshold) {
#ifdef DEBUG
    if (flag.new_fmt) {
      std::cout << "   \033[1;33mNOTE: " << dp_threshold
                << " is greater than the specified threshold: "
                << thresholds.significant << "\033[0m" << std::endl;
    }
#endif
    return dp_threshold;
  }
  return thresh.significant;
}

// ========================================================================
// Difference Processing
// ========================================================================
bool FileComparator::process_difference(const ColumnValues& column_data,
                                        size_t column_index) {
  // Calculate rounded values and process difference
  double rounded1 = round_to_decimals(column_data.value1, column_data.min_dp);
  double rounded2 = round_to_decimals(column_data.value2, column_data.min_dp);
  double diff_rounded = std::abs(rounded1 - rounded2);

  process_rounded_values(diff_rounded, column_data.min_dp);

  double ithreshold = calculate_threshold(column_data.min_dp);

  // Print differences if above plot threshold
  if (diff_rounded > thresh.print) {
    print_table(column_data, column_index, ithreshold, diff_rounded);
    std::cout << std::endl;
  } else {
    counter.elem_number++;
#ifdef DEBUG2
    auto line_num_width =
        static_cast<int>(std::to_string(this_line_ncols).length());
    // std::cout << "ncols: " << this_line_ncols           << ", column_index: "
    // << column_index + 1 << std::endl;
    std::cout << "   DIFF: Values at line " << counter.line_number
              << ", column " << std::setw(line_num_width) << column_index + 1
              << " are equal: " << std::setprecision(column_data.min_dp)
              << rounded1 << std::endl;
#endif
  }

  // Check critical threshold
  if ((diff_rounded > thresh.critical) &&
      ((rounded1 <= thresh.ignore) && (rounded2 <= thresh.ignore))) {
    counter.diff_critical++;
    flag.has_critical_diff = true;
    print_hard_threshold_error(rounded1, rounded2, diff_rounded, column_index);
    return false;
  }
  return true;
}

void FileComparator::process_raw_values(double value1, double value2) {
  // compare values (without rounding)
  double diff = std::abs(value1 - value2);
  if (diff > differ.max) {
    differ.max = diff;
  }
  // track number of differences
  if (diff > thresh.zero) {
    counter.diff_non_zero++;
    flag.has_non_zero_diff = true;
  }
}

void FileComparator::process_rounded_values(double rounded_diff,
                                            int minimum_deci) {
  // compare values (with rounding)
  if (rounded_diff > differ.max_rounded) {
    differ.max_rounded = rounded_diff;
  }

  // Define the threshold for non-trivial differences
  //
  // The smallest non-zero difference between values with N decimal
  // places is 10^(-N). A difference less than that value is trivial and
  // effectively zero. A difference greater than that value is not just due to
  // rounding errors or numerical precision issues. This is a heuristic to avoid
  // counting trivial differences as significant.

  // set to half the threshold to avoid counting trivial differences
  if (double big_zero = pow(10, -minimum_deci) / 2; rounded_diff > big_zero) {
    counter.diff_non_trivial++;
    flag.has_non_trivial_diff = true;
  }

  if (rounded_diff > thresh.significant) {
    counter.diff_significant++;
    flag.has_significant_diff = true;
  }
}

// ========================================================================
// Output & Formatting
// ========================================================================
void FileComparator::print_table(const ColumnValues& column_data,
                                 size_t column_index, double line_threshold,
                                 double diff_rounded) {
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
  std::cout << "   DIFF: Difference at line " << counter.line_number
            << ", column " << column_index + 1 << ": " << diff_rounded
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
    if (thresh.significant < thresh.print) {
      std::cout << "\033[1;33mWarning: Threshold for printing (" << thresh.print
                << ") is less than the overall threshold ("
                << thresh.significant
                << "). Some significant differences may not be "
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
  flag.has_printed_diff = true;

  /* PRINT DIFF TABLE ENTRY */
  // Use the values directly from the struct instead of recalculating

  // line
  std::cout << std::setw(col_widths[0]) << counter.line_number;
  // column
  std::cout << std::setw(col_widths[1]) << column_index + 1;

  // range (first value in the line)
  std::cout << std::fixed << std::setprecision(2) << std::setw(col_widths[2])
            << column_data.range << " ";

  // values in file1
  if (column_data.value1 > thresh.ignore) {
    std::cout << "\033[1;34m";
  }
  std::cout << format_number(column_data.value1, column_data.dp1, mxint, mxdec);

  std::cout << "\033[0m ";

  // values in file2
  if (column_data.value2 > thresh.ignore) {
    std::cout << "\033[1;34m";
  }
  std::cout << format_number(column_data.value2, column_data.dp2, mxint, mxdec);
  std::cout << "\033[0m" << " | ";

  // threshold
  if (line_threshold > thresh.significant) {
    std::cout << "\033[1;33m";
  }
  std::cout << std::setw(col_widths[5]) << std::setprecision(3)
            << line_threshold << "\033[0m | ";

  double ieps = line_threshold * 0.1;
  double thresh_prec = (line_threshold + ieps);

  if (diff_rounded > thresh_prec) {
    counter.diff_non_trivial++;
  }

  // difference
  if (column_data.value1 > thresh.ignore ||
      column_data.value2 > thresh.ignore) {
    std::cout << "\033[1;34m";
  } else if (diff_rounded > thresh.significant && diff_rounded < thresh_prec) {
    std::cout << "\033[1;33m";
  } else if (diff_rounded > thresh.critical) {
    std::cout << "\033[1;31m";
    flag.error_found = true;
  } else {
    std::cout << "\033[0m";
  }

  std::cout << std::setw(col_widths[6]) << diff_rounded << "\033[0m";

  // std::cout << " > " << ithreshold << " : " << (diff_rounded -
  // ithreshold)
  //           << " -> " << fabs(diff_rounded - ithreshold) << " > " <<
  //           ieps;
}

std::string FileComparator::format_number(double value, int prec,
                                          int max_integer_width,
                                          int max_decimals) const {
  // convert the number to a string with the specified precision
  std::ostringstream oss;

  int iprec = prec;  // Use the provided precision
  if (prec > max_decimals) {
    iprec = max_decimals;  // Ensure precision is within bounds
  }

  oss << std::fixed << std::setprecision(iprec) << value;
  std::string numStr = oss.str();

  // Find position of decimal point
  size_t dotPos = numStr.find('.');

  // Calculate padding for integer part
  int intWidth = (dotPos != std::string::npos)
                     ? static_cast<int>(dotPos)
                     : static_cast<int>(numStr.length());

  int padding_width = max_integer_width - intWidth;
  // Prevent negative padding
  if (padding_width < 0) {
    std::cout << std::endl
              << "max_integer_width: " << max_integer_width << std::endl
              << "intWidth: " << intWidth << std::endl
              << "padding_width: " << padding_width << std::endl;
    std::cerr << "Error: Negative padding width in format_number: "
              << padding_width << std::endl;
    padding_width = 0;  // Prevent negative padding
  }

  int padRight = max_decimals - iprec;
  if (padRight < 0) padRight = 0;  // Ensure no negative padding

  return std::string(padding_width, ' ') + numStr + std::string(padRight, ' ');
}

void FileComparator::print_hard_threshold_error(double rounded1,
                                                double rounded2,
                                                double diff_rounded,
                                                size_t column_index) const {
  std::cerr << "\033[1;31mLarge difference found at line "
            << counter.line_number << ", column " << column_index + 1
            << "\033[0m" << std::endl;

  if (counter.line_number > 0) {
    std::cout << "   First " << counter.line_number - 1 << " lines match"
              << std::endl;
  }
  if (counter.elem_number > 0) {
    std::cout << "   " << counter.elem_number << " element";
    if (counter.elem_number > 1) std::cout << "s";
    std::cout << " checked" << std::endl;
  }

  std::cout << counter.diff_print << " with differences between "
            << thresh.significant << " and " << thresh.critical << std::endl;

  std::cout << "   File1: " << std::setw(7) << rounded1 << std::endl;
  std::cout << "   File2: " << std::setw(7) << rounded2 << std::endl;
  std::cout << "    diff: \033[1;31m" << std::setw(7) << diff_rounded
            << "\033[0m" << std::endl;
}

void FileComparator::print_format_info(const ColumnValues& column_data,
                                       size_t column_index) const {
#ifdef DEBUG2
  std::cout << "   NEW FORMAT" << std::endl;
#endif
  // print the format
  std::cout << "DEBUG : Line " << counter.line_number << ", Column "
            << column_index + 1 << std::endl;
  std::cout << "   FORMAT: number of decimal places file1: " << column_data.dp1
            << ", file2: " << column_data.dp2 << std::endl;
}

void printbar(int indent = 0) {
  for (int i = 0; i < indent; ++i) std::cout << "   ";
  std::cout << "---------------------------------------------------------"
            << std::endl;
}

void FileComparator::print_diff_like_summary(
    const SummaryParams& params) const {
  // Diff-like differences
  // =========================================================
  if (counter.diff_non_zero == 0) {
    std::cout << "\033[1;32m   Files " << params.file1 << " and "
              << params.file2 << " are identical\033[0m" << std::endl;
    return;
  }
  if (counter.elem_number > counter.diff_non_zero) {
    const size_t zero_diff = counter.elem_number - counter.diff_non_zero;
    if (zero_diff > 0) {
#ifdef DEBUG
      std::cout << "   Exact matches        ( =" << 0.0
                << "): " << std::setw(params.fmt_wid) << zero_diff << std::endl;
#endif
    }
  }

  std::cout << "   Non-zero differences ( >" << 0.0
            << "): " << std::setw(params.fmt_wid) << counter.diff_non_zero
            << std::endl;

  std::cout << "\033[1;33m   Files " << params.file1 << " and " << params.file2
            << " are different\033[0m" << std::endl;

  if (counter.diff_print < counter.diff_non_zero) {
#ifdef DEBUG
    std::cout << "   Printed differences  ( >" << thresh.print
              << "): " << std::setw(params.fmt_wid) << counter.diff_print
              << std::endl;
#endif
    size_t not_printed = counter.diff_non_zero - counter.diff_print;
    if (not_printed > 0) {
      std::cout << "   Not printed          (<=" << thresh.print
                << "): " << std::setw(params.fmt_wid) << not_printed
                << std::endl;
    }
  } else {
    std::cout << "   All non-zero "
                 "differences are printed."
              << std::endl;
  }

  if (differ.max > thresh.zero) {
    std::cout << "   Maximum difference: " << differ.max << std::endl;
  }

  printbar(1);
}

void FileComparator::print_rounded_summary(const SummaryParams& params) const {
  // Trivial differences
  // =========================================================
  if (counter.diff_non_trivial == 0) {
    std::cout << "\033[1;32m   Files " << params.file1 << " and "
              << params.file2
              << " are identical with trivial differences due to "
                 "formatting\033[0m"
              << std::endl;
    return;
  }
  if (counter.diff_non_zero > counter.diff_non_trivial) {
    size_t zero_diff = counter.diff_non_zero - counter.diff_non_trivial;
    if (zero_diff > 0) {
      std::cout << "   Trivial differences     ( >" << 0.0
                << "): " << std::setw(params.fmt_wid) << zero_diff << std::endl;
    }
  }
  std::cout << "   Non-trivial differences          : "
            << std::setw(params.fmt_wid) << counter.diff_non_trivial
            << std::endl;

  std::cout << "\033[1;33m   Files " << params.file1 << " and " << params.file2
            << " are non-trivially different\033[0m" << std::endl;

  if (counter.diff_print < counter.diff_non_trivial) {
    std::cout << "   Printed differences     ( >" << thresh.print
              << "): " << std::setw(params.fmt_wid) << counter.diff_print
              << std::endl;
    size_t not_printed = counter.diff_non_trivial - counter.diff_print;
    if (not_printed > 0) {
      std::cout << "   Not printed differences (<=" << thresh.print
                << "): " << std::setw(params.fmt_wid) << not_printed
                << std::endl;
    }
  } else {
    std::cout << "   All non-trivial "
                 "differences are printed."
              << std::endl;
  }

  std::cout << "   Maximum rounded difference: " << differ.max_rounded
            << std::endl;
  printbar(1);
}

void FileComparator::print_significant_summary(
    const SummaryParams& params) const {
  // User-defined threshold differences
  // =========================================================
  if (counter.diff_significant == 0) {
    std::cout << "\033[1;32m   Files " << params.file1 << " and "
              << params.file2 << " are identical within tolerance\033[0m"
              << std::endl;
    return;
  }
  std::cout << "   Significant differences   ( >" << thresh.significant
            << "): " << std::setw(params.fmt_wid) << counter.diff_significant
            << std::endl;
  if (counter.diff_non_trivial > counter.diff_significant) {
    size_t zero_diff = counter.diff_non_trivial - counter.diff_significant;
    if (zero_diff > 0) {
      std::cout << "   Insignificant differences (<=" << thresh.significant
                << "): " << std::setw(params.fmt_wid) << zero_diff << std::endl;
    }
  }
  std::cout << "\033[1;31m   Files " << params.file1 << " and " << params.file2
            << " are significantly different\033[0m" << std::endl;

  if (counter.diff_print < counter.diff_significant) {
    std::cout << "   Printed differences      ( >" << thresh.print
              << "): " << std::setw(params.fmt_wid) << counter.diff_print
              << std::endl;
    size_t not_printed_signif = counter.diff_significant - counter.diff_print;
    if (not_printed_signif > 0) {
      std::cout << "\033[1;31m   Not printed differences  (<=" << thresh.print
                << "): " << std::setw(params.fmt_wid) << not_printed_signif
                << "\033[0m" << std::endl;
    }
  } else {
    std::cout << "   All significant "
                 "differences are printed."
              << std::endl;
  }

  printbar(1);
}

void FileComparator::print_summary(const std::string& file1,
                                   const std::string& file2, int argc,
                                   char* argv[]) const {
#ifdef DEBUG
  std::cout << "ARGUMENTS:" << std::endl;
#endif
  // print command line arguments
  std::cout << "   Input:";
  for (int i = 0; i < argc; ++i) {
    std::cout << " " << argv[i];
  }
  std::cout << std::endl;
#ifdef DEBUG
  std::cout << "   File1: " << file1 << std::endl;
  std::cout << "   File2: " << file2 << std::endl;

  std::cout << "STATISTICS:" << std::endl;
  std::cout << "   Total lines compared: " << counter.line_number;

  // Check if all lines were compared
  size_t length1 = get_file_length(file1);
  if (length1 == counter.line_number) {
    std::cout << " (all)" << std::endl;
  } else {
    std::cout << " of " << length1 << std::endl;
    // print how many lines were not compared
    size_t missing_lines = length1 - counter.line_number;
    std::cout << "\033[1;31m   " << missing_lines
              << " lines were not compared\033[0m" << std::endl;
  }

  std::cout << "   Total elements checked: " << counter.elem_number
            << std::endl;
#endif
  std::cout << "SUMMARY:" << std::endl;

  // Calculate the width for formatting
  auto fmt_wid = static_cast<int>(std::to_string(counter.elem_number).length());
  SummaryParams params{file1, file2, fmt_wid};

  print_diff_like_summary(params);
  print_rounded_summary(params);
  print_significant_summary(params);

  // Print theshold differences
  // =========================================================
  if (counter.diff_print == 0) {
    std::cout << "\033[1;32m   Files " << params.file1 << " and "
              << params.file2 << " are identical within print threshold\033[0m"
              << std::endl;
    return;
  }
  std::cout << "   Printed differences      ( >" << thresh.print
            << "): " << std::setw(fmt_wid) << counter.diff_print << std::endl;

  if (thresh.significant < thresh.print) {
    if (counter.diff_significant > counter.diff_print) {
      size_t not_printed_signif = counter.diff_significant - counter.diff_print;
      if (not_printed_signif > 0) {
        std::cout << "\033[1;31m   Not printed differences  (<=" << thresh.print
                  << "): " << std::setw(fmt_wid) << not_printed_signif
                  << "\033[0m" << std::endl;
      }
    }
  } else {
    if (counter.diff_non_trivial > counter.diff_print) {
      size_t not_printed = counter.diff_non_trivial - counter.diff_print;
      if (not_printed > 0) {
        std::cout << "   Not printed differences  (<=" << thresh.print
                  << "): " << std::setw(fmt_wid) << not_printed << std::endl;
      }
    }
  }

  printbar(1);
  // Hard threshold differences
  // =========================================================
  if (counter.diff_critical == 0) {
    return;
  }

  if (counter.diff_critical > 0) {
    std::cout << "\033[1;31m   Differences exceeding critical threshold ("
              << thresh.critical << "): " << counter.diff_critical << "\033[0m"
              << std::endl;
  } else {
    std::cout << "   No differences exceeding critical threshold found."
              << std::endl;

    if (counter.diff_print == 0) {
      std::cout << "\033[1;32m   Files " << params.file1 << " and "
                << params.file2 << " are identical.\033[0m" << std::endl;
    }
  }
}

ColumnValues FileComparator::extract_column_values(const LineData& data1,
                                                   const LineData& data2,
                                                   size_t column_index) const {
  double val1 = data1.values[column_index];
  double val2 = data2.values[column_index];
  double rangeValue = data1.values[0];  // first value in the line

  // Get decimal places from the LineData structure
  int dp1 = data1.decimal_places[column_index];
  int dp2 = data2.decimal_places[column_index];
  int min_dp = std::min(dp1, dp2);

  return {val1, val2, rangeValue, dp1, dp2, min_dp};
}

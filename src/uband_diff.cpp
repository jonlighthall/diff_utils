/**
 * @file uband_diff.cpp
 * @brief Implementation of file comparison utilities for numerical data with
 * support for complex numbers.
 *
 * @details This file provides the implementation for comparing two files
 * containing numerical data, including support for complex numbers in the
 * format (real, imag). As opposed to the standard utility diff, which compares
 * text files line by line, this implementation focuses on the numerical values
 * within the files. The comparison is performed line by line and column by
 * column, with configurable thresholds for floating-point differences. The code
 * tracks and reports differences, handles changes in file format (such as
 * column count or precision), and prints detailed diagnostics for mismatches.
 *
 * @Features:
 * - Supports real and complex number formats.
 * - Automatically detects and adapts to changes in column count and decimal
 * precision.
 * - Configurable thresholds for reporting differences.
 * - Detailed output with color-coded diagnostics for easy identification of
 * issues.
 * - Debugging macros for step-by-step tracing.
 *
 * @author J.C. Lighthall
 * @date January 2025
 *
 * @note This program was developed from the Fortran code tldiff.f90 and it's
 * derivatives cpddiff.f90, prsdiff.f90, and tsdiff.f90 (August 2022). It is
 * intended to replace those utilities with a more modern C++ implementation.
 *
 * Development of this code was significantly assisted by GitHub Copilot (GPT-4,
 * Claude Sonnet 4, etc.) for code translation, refactoring, optimization, and
 * architectural improvements.
 *
 */
#include "uband_diff.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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

// truncate a double to a specified number of decimal places
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

  // strings to store individual line contents
  // the lines must be read in as strings to parse the decimal place format
  std::string line1;
  std::string line2;

  // track if the file format has changed
  size_t prev_n_col = 0;
  std::vector<int> dp_per_col;

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
  flag.file_end_reached = true;

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

  auto validate_decimal_places = [line_number = counter.line_number](
                                     const int ndp, Flags& flags) {
    if (ndp < 0 || ndp > 10) {  // Arbitrary limit for decimal places
      std::cerr << "Invalid number of decimal places found on line "
                << line_number << ": " << ndp << ". Must be between 0 and 10."
                << std::endl;
      flags.error_found = true;
      return false;
    }
    return true;
  };

  while (stream >> ch) {
    // check if the numbers are complex and read them accordingly
    // check if string starts with '('
    if (ch == '(') {
      // read the complex number
      auto [real, imag, dp_real, dp_imag] = readComplex(stream, flag);
      result.values.push_back(real);
      result.values.push_back(imag);
      // validate decimal places for real and imaginary parts
      if (validate_decimal_places(dp_real, flag)) {
        result.decimal_places.push_back(dp_real);
      }
      if (validate_decimal_places(dp_imag, flag)) {
        result.decimal_places.push_back(dp_imag);
      }
    } else {
      // if the character is not '(', it is a number
      stream.putback(ch);  // put back the character to the stream
      int dp = stream_countDecimalPlaces(
          stream);  // count the number of decimal places
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
  if (print.debug || print.level > 0) {
    std::cout << "Files have the same number of lines: " << length1
              << std::endl;
    std::cout << "Files have the same number of elements: "
              << counter.elem_number << std::endl;
  }

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

  // print line contents if DEBUG2 is defined
  if (print.debug2) {
    auto print_line_contents = [](const LineData& data, size_t n_col) {
      for (size_t i = 0; i < n_col; ++i) {
        int ndp = data.decimal_places[i];
        std::cout << " " << std::fixed << std::setprecision(ndp)
                  << data.values[i] << "(" << ndp << ")";
      }
      std::cout << std::endl;
    };

    std::cout << "DEBUG2: Line " << counter.line_number << std::endl;
    std::cout << "   CONTENTS:" << std::endl;
    std::cout << "      file1:";
    print_line_contents(data1, n_col1);
    std::cout << "      file2:";
    print_line_contents(data2, n_col2);
  }

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
  ColumnValues column_data = extract_column_values(data1, data2, column_index);
  process_raw_values(column_data);

  // Handle decimal places initialization and updates
  // Initialize if this is the first line OR if the format has changed (vector
  // was cleared)
  if (counter.line_number == 1 || column_index >= dp_per_col.size()) {
    if (!initialize_decimal_place_format(column_data.min_dp, column_index,
                                         dp_per_col)) {
      return false;
    }
  } else {
    if (!update_decimal_place_format(column_data.min_dp, column_index,
                                     dp_per_col)) {
      return false;
    }
  }

  // now that the minimum decimal places are determined, we can
  // round the values to the minimum decimal places
  // and compare them

  // Print format info if needed
  if (column_data.dp1 != column_data.dp2 && flag.new_fmt) {
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
    if (print.debug2) {
      std::cout << "   FORMAT: " << n_col1
                << " columns (both files) - initialized" << std::endl;
    }
  }

  if (prev_n_col > 0 && n_col1 != prev_n_col) {
    std::cout << "\033[1;33mNote: Number of columns changed at line "
              << counter.line_number << " (previous: " << prev_n_col
              << ", current: " << n_col1 << ")\033[0m" << std::endl;
    dp_per_col.clear();
    flag.new_fmt = true;
    this_fmt_line = counter.line_number;
    if (print.level > 0) {
      std::cout << this_fmt_line << ": FMT number of columns has changed"
                << std::endl;
      std::cout << "format has changed" << std::endl;
    }
  } else {
    if (counter.line_number > 1) {
      if (print.debug3) {
        std::cout << "Line " << counter.line_number << " same column format"
                  << std::endl;
      }
      flag.new_fmt = false;
    }
  }
  prev_n_col = n_col1;
  return true;
}

bool FileComparator::validate_decimal_column_size(
    const std::vector<int>& dp_per_col, size_t column_index) const {
  if (print.debug3) {
    for (size_t j = 0; j < dp_per_col.size(); ++j) {
      std::cout << "   minimum decimal places in column " << j + 1 << " = "
                << dp_per_col[j] << std::endl;
    }
  }

  if (print.debug2) {
    std::cout << "   size of dp_per_col: " << dp_per_col.size();
    std::cout << ", column_index: " << column_index + 1 << std::endl;
  }

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
bool FileComparator::initialize_decimal_place_format(
    const int min_dp, const size_t column_index, std::vector<int>& dp_per_col) {
  // initialize the dp_per_col vector with the minimum decimal places
  dp_per_col.push_back(min_dp);

  if (!validate_decimal_column_size(dp_per_col, column_index)) return false;

  if (print.debug2) {
    std::cout << "FORMAT: Line " << counter.line_number << " initialization"
              << std::endl;
    std::cout << "   dp_per_col: ";
    for (const auto& d : dp_per_col) {
      std::cout << d << " ";
    }
    std::cout << std::endl;
  }

  // since this is an initialization, it is always a new format
  flag.new_fmt = true;
  this_fmt_line = counter.line_number;
  this_fmt_column = column_index + 1;
  return true;
}

bool FileComparator::update_decimal_place_format(const int min_dp,
                                                 const size_t column_index,
                                                 std::vector<int>& dp_per_col) {
  if (print.debug3) {
    std::cout << "not first line" << std::endl;
  }

  // Safety check: ensure the vector is large enough for this column
  if (column_index >= dp_per_col.size()) {
    std::cerr << "Error: dp_per_col size (" << dp_per_col.size()
              << ") insufficient for column " << column_index + 1 << " at line "
              << counter.line_number << std::endl;
    flag.error_found = true;
    return false;
  }

  // check if the minimum decimal places for this column has changed
  if (dp_per_col[column_index] != min_dp) {
    // If the minimum decimal places for this column is different from the
    // previous minimum decimal places, update it
    if (print.debug3) {
      std::cout << "DEBUG3: different" << std::endl;
      std::cout << "DEBUG3: format has changed" << std::endl;
    }
    dp_per_col[column_index] = min_dp;
    flag.new_fmt = true;
    this_fmt_line = counter.line_number;
    if (print.debug) {
      std::cout << "FORMAT: Line " << this_fmt_line
                << ": number of decimal places has changed" << std::endl;
    }
  }
  if (print.debug3 && dp_per_col[column_index] == min_dp) {
    // If the minimum decimal places for this column is the same as the
    // previous minimum decimal places, do nothing
    std::cout << "DEBUG3: same" << std::endl;
  }
  return true;
}

double FileComparator::calculate_threshold(int ndp) {
  // determine the smallest possible difference, base on the number of
  // decimal places (equal to the minimum number of decimal places for the
  // current element)
  double dp_threshold = std::pow(10, -ndp);

  if (flag.new_fmt && print.debug && !print.diff_only) {
    if (this_fmt_line != last_fmt_line) {
      // group together all format specifications on the same line
      std::cout << "PRECISION: Line " << this_fmt_line;
      if (counter.line_number == 1) {
        std::cout << " (initial format)";
      } else {
        std::cout << " (change in format)";
      }
      std::cout << std::endl;
    }
    last_fmt_line = this_fmt_line;
    // Set the width for line numbers based on the number of digits in
    // this_fmt_line
    auto line_num_width =
        static_cast<int>(std::to_string(this_line_ncols).length());
    // indent the column format specification under the line number
    std::cout << "      Column " << std::setw(line_num_width) << this_fmt_column
              << ": ";
    std::cout << ndp << " decimal places or 10^(" << -ndp
              << ") = " << std::setprecision(ndp) << dp_threshold << std::endl;
    if (ndp > differ.ndp_single_precision) {
      std::cout << "\033[1;33m   Warning: Decimal places (" << ndp
                << ") exceed single precision (" << differ.ndp_single_precision
                << "). Results may be unreliable.\033[0m" << std::endl;
    }
  }
  if (ndp > differ.ndp_max) {
    differ.ndp_max = ndp;
    if (print.level > 0) {
      std::cout << "      Maximum decimal places so far: " << differ.ndp_max
                << std::endl;
    }
  }
  if (thresh.significant < dp_threshold) {
    if (print.debug && flag.new_fmt && thresh.significant > 0) {
      std::cout << "   \033[1;33mNOTE: minimum non-zero difference ("
                << dp_threshold << ") is greater than significant threshold ("
                << thresh.significant << ")\033[0m" << std::endl;
    }
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
  // compare values (with rounding)
  double diff_rounded = std::abs(rounded1 - rounded2);

  process_rounded_values(column_data, column_index, diff_rounded,
                         column_data.min_dp);

  double ithreshold = calculate_threshold(column_data.min_dp);

  counter.elem_number++;
  // Print differences if above plot threshold
  if (diff_rounded > thresh.print) {
    print_table(column_data, column_index, ithreshold, diff_rounded);
    std::cout << std::endl;
  } else {
    if (print.debug2) {
      auto line_num_width =
          static_cast<int>(std::to_string(this_line_ncols).length());
      if (print.debug3) {
        std::cout << "ncols: " << this_line_ncols
                  << ", column_index: " << column_index + 1 << std::endl;
      }
      std::cout << "   DIFF: Values at line " << counter.line_number
                << ", column " << std::setw(line_num_width) << column_index + 1
                << " are equal: " << std::setprecision(column_data.min_dp)
                << rounded1 << " (" << column_data.min_dp << ")" << std::endl;
    }
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

void FileComparator::process_raw_values(const ColumnValues& column_data) {
  // compare values (without rounding)
  double diff = std::abs(column_data.value1 - column_data.value2);

  // track the maximum difference
  if (diff > differ.max_non_zero) {
    differ.max_non_zero = diff;
    differ.ndp_non_zero = column_data.min_dp;
  }
  // track number of differences
  if (diff > thresh.zero) {
    counter.diff_non_zero++;
    flag.has_non_zero_diff = true;
    flag.files_are_same = false;
  }
}

void FileComparator::process_rounded_values(const ColumnValues& column_data,
                                            size_t column_index,
                                            double rounded_diff,
                                            int minimum_deci) {
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
    flag.files_have_same_values = false;

    // track the maximum non trivial difference
    if (rounded_diff > differ.max_non_trivial) {
      differ.max_non_trivial = rounded_diff;
      differ.ndp_non_trivial = column_data.min_dp;
    }
  }

  if (rounded_diff > thresh.significant && column_data.value1 < thresh.ignore &&
      column_data.value2 < thresh.ignore) {
    counter.diff_significant++;
    flag.has_significant_diff = true;
    flag.files_are_close_enough = false;

    if (column_data.value1 < thresh.marginal &&
        column_data.value2 < thresh.marginal) {
      counter.diff_marginal++;
      flag.has_marginal_diff = true;
    }

    // track the maximum significant difference
    if (rounded_diff > differ.max_significant) {
      differ.max_significant = rounded_diff;
      differ.ndp_significant = column_data.min_dp;
    }
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
  // print threshold

  if (print.debug2) {
    std::cout << "   DIFF: Difference at line " << counter.line_number
              << ", column " << column_index + 1 << ": " << diff_rounded
              << " (threshold: " << line_threshold << ")" << std::endl;
  }

  // define maximum display format for the values being printed
  int mxint = 5;  // maximum integer width
  int mxdec = 7;  // maximum decimal places

  if (mxdec < differ.ndp_max) {
    mxdec = differ.ndp_max;  // use the maximum decimal places found so far
  }

  int val_width = mxint + mxdec + 1;  // total width for value columns

  // define column widths
  //                     // line | col | range | file1 | file2 | thres | diff
  std::vector<int> col_widths = {5,         5,         val_width, val_width,
                                 val_width, val_width, val_width};

  auto padLeft = [](const std::string& str, int width) {
    if (static_cast<int>(str.length()) >= width) return str;
    return std::string(width - str.length(), ' ') + str;
  };

  if (counter.diff_print == 0) {
    std::cout << "DIFFERENCES:" << std::endl;
    if (thresh.significant < thresh.print) {
      std::cout << "\033[1;33mWarning: Threshold for printing (" << thresh.print
                << ") is greater than the significant difference threshold ("
                << thresh.significant
                << "). Some significant differences may not be "
                << "printed.\033[0m" << std::endl;
    }

    // Print header if this is the first difference
    std::cout << std::setw(col_widths[0]) << "line";
    std::cout << std::setw(col_widths[1]) << "col";
    std::cout << std::setw(col_widths[2]) << "range";
    std::cout << std::setw(col_widths[3] + 1) << "file1";
    std::cout << std::setw(col_widths[4] + 3) << "file2 |";
    std::cout << padLeft(" thres |", col_widths[5] + 3);
    std::cout << padLeft("diff", col_widths[6] + 1) << std::endl;

    // Print horizontal line matching header width
    int total_width = 0;
    for (int w : col_widths) total_width += w;
    // Add spaces for extra padding in header columns
    total_width += 1 + 3 + 3 + 1;  // file1(+1), file2(+3), thres(+3), diff(+1)
    std::cout << std::string(total_width, '-') << std::endl;
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

  // Lambda to print color based on value and thresholds
  auto print_value_color = [this](double value) {
    if (value > thresh.ignore) {
      std::cout << "\033[1;34m";  // Blue
    } else if (value > thresh.marginal) {
      std::cout << "\033[1;33m";  // Yellow
    } else {
      std::cout << "\033[0m";
    }
  };

  // values in file1
  print_value_color(column_data.value1);
  std::cout << format_number(column_data.value1, column_data.dp1, mxint, mxdec);

  std::cout << "\033[0m ";

  // values in file2
  print_value_color(column_data.value2);
  std::cout << format_number(column_data.value2, column_data.dp2, mxint, mxdec);
  std::cout << "\033[0m" << " | ";

  // threshold
  if (line_threshold > thresh.significant && thresh.significant > 0) {
    std::cout << "\033[1;35m";
    std::cout << format_number(thresh.significant, column_data.min_dp, mxint,
                               mxdec);
    std::cout << "\033[0m | ";
  } else {
    std::cout << format_number(line_threshold, column_data.min_dp, mxint,
                               mxdec);
    std::cout << " | ";
  }

  // Lambda to print color based on diff and thresholds
  auto print_diff_color = [this](double value1, double value2, double rdiff) {
    if (rdiff > thresh.significant) {
      std::cout << "\033[1;36m";  // Cyan for significant differences
    } else {
      std::cout << "\033[0m";  // Reset color for trivial differences
    }
    // Determine color based on thresholds
    if (double max_value = std::max(value1, value2);
        max_value > thresh.ignore) {
      std::cout << "\033[1;34m";  // Blue
    } else if (max_value > thresh.marginal) {
      std::cout << "\033[1;33m";  // Yellow
    }
    if (rdiff > thresh.critical) {
      std::cout << "\033[1;31m";  // Red for critical differences
      flag.error_found = true;
    }
  };

  // difference
  print_diff_color(column_data.value1, column_data.value2, diff_rounded);
  std::cout << format_number(diff_rounded, column_data.min_dp, mxint, mxdec);
  std::cout << "\033[0m";
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
    if (print.level > 0) {
      std::cout << std::endl
                << "max_integer_width: " << max_integer_width << std::endl
                << "intWidth: " << intWidth << std::endl
                << "padding_width: " << padding_width << std::endl;

      std::cerr << "Error: Negative padding width in format_number: "
                << padding_width << std::endl;
    }
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
  if (print.level < 0) {
    return;
  }
  std::cerr << "\033[1;31mLarge difference found at line "
            << counter.line_number << ", column " << column_index + 1
            << "\033[0m" << std::endl;
  flag.error_found = true;

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
  if (print.debug2) {
    std::cout << "   NEW FORMAT" << std::endl;
  }
  // print the format
  if (print.debug) {
    std::cout << "   DEBUG : Line " << counter.line_number << ", Column "
              << column_index + 1 << std::endl;
    std::cout << "      FORMAT: number of decimal places file1: "
              << column_data.dp1 << ", file2: " << column_data.dp2 << std::endl;
  }
}

void printbar(int indent = 0) {
  for (int i = 0; i < indent; ++i) std::cout << "   ";
  std::cout << "---------------------------------------------------------"
            << std::endl;
}

// Helper function to get color based on count values
std::string FileComparator::get_count_color(size_t count) const {
  if (count > 0) {
    return "\033[1;33m";  // Yellow for non-zero counts
  } else if (counter.diff_significant > 0) {
    return "\033[1;31m";  // Red if there are significant differences
  } else {
    return "\033[1;32m";  // Green otherwise
  }
}

// Helper function to print identical files message
void FileComparator::print_identical_files_message(
    const SummaryParams& params) const {
  if (print.level >= 0) {
    std::cout << "   ";
  }
  std::cout << "\033[1;32mFiles " << params.file1 << " and " << params.file2
            << " are identical\033[0m" << std::endl;
}

// Helper function to print exact matches information
void FileComparator::print_exact_matches_info(
    const SummaryParams& params) const {
  if (counter.elem_number <= counter.diff_non_zero) {
    return;  // No exact matches to report
  }

  const size_t zero_diff = counter.elem_number - counter.diff_non_zero;
  if (zero_diff > 0 && print.debug) {
    std::cout << "   Exact matches        ( =" << 0.0 << "): ";
    std::cout << get_count_color(zero_diff);
    std::cout << std::setw(params.fmt_wid) << zero_diff << "\033[0m"
              << std::endl;
  }
}

// Helper function to print non-zero differences information
void FileComparator::print_non_zero_differences_info(
    const SummaryParams& params) const {
  std::cout << "   Non-zero differences ( >" << thresh.zero << "): ";
  std::cout << get_count_color(counter.diff_non_zero);
  std::cout << std::setw(params.fmt_wid) << counter.diff_non_zero << "\033[0m"
            << std::endl;

  std::cout << "\033[1;33m   Files " << params.file1 << " and " << params.file2
            << " are different\033[0m" << std::endl;
}

// Helper function to print difference counts (printed vs not printed)
void FileComparator::print_difference_counts(
    const SummaryParams& params) const {
  if (counter.diff_print < counter.diff_non_zero) {
    if (print.debug) {
      std::cout << "   Printed differences  ( >" << thresh.print
                << "): " << std::setw(params.fmt_wid) << counter.diff_print
                << std::endl;
    }
    size_t not_printed = counter.diff_non_zero - counter.diff_print;
    if (not_printed > 0) {
      std::cout << "   Not printed          (<=" << thresh.print
                << "): " << std::setw(params.fmt_wid) << not_printed
                << std::endl;
    }
  } else {
    std::cout << "   All non-zero differences are printed." << std::endl;
  }
}

// Helper function to analyze and print maximum difference information
void FileComparator::print_maximum_difference_analysis(
    const SummaryParams& params) const {
  if (differ.max_non_zero <= thresh.zero) {
    return;  // No significant maximum difference to analyze
  }

  // Print maximum difference
  std::cout << "   Maximum difference: "
            << format_number(
                   differ.max_non_zero, differ.ndp_non_zero,
                   static_cast<int>(std::round(log10(differ.max_non_zero)) + 2),
                   differ.ndp_non_zero)
            << std::endl;

  // Analyze maximum difference relative to significant threshold
  if (differ.max_non_zero > thresh.significant) {
    std::string color =
        (counter.diff_significant > 0) ? "\033[1;31m" : "\033[1;33m";
    std::cout << color
              << "   Max diff is greater than the significant threshold: "
              << thresh.significant << "\033[0m" << std::endl;

    // Handle special case when no non-trivial differences exist
    if (counter.diff_non_trivial == 0) {
      printbar(1);
      if (differ.max_non_trivial <= thresh.significant) {
        std::cout << "   \033[4;35mMaximum rounded difference: "
                  << differ.max_non_trivial << "\033[0m" << std::endl;

        bool equal_to_threshold =
            fabs(differ.max_non_trivial - thresh.significant) < thresh.zero;
        std::string result_color =
            equal_to_threshold ? "\033[1;33m" : "\033[1;32m";
        std::string comparison = equal_to_threshold ? "equal to" : "less than";

        std::cout << result_color << "   Max diff is " << comparison
                  << " the significant threshold: " << thresh.significant
                  << "\033[0m" << std::endl;
      }
    } else {
      std::cout << "\033[1;32m   Max diff is less or equal to than the "
                   "significant threshold: "
                << thresh.significant << "\033[0m" << std::endl;
    }
  } else {
    // Maximum difference is within threshold
    bool equal_to_threshold =
        fabs(differ.max_non_zero - thresh.significant) < thresh.zero;
    std::string result_color = equal_to_threshold ? "\033[1;33m" : "\033[1;32m";
    std::string comparison = equal_to_threshold ? "equal to" : "less than";

    std::cout << result_color << "   Max diff is " << comparison
              << " the significant threshold: " << thresh.significant
              << "\033[0m" << std::endl;
  }
}

// Simplified main print_diff_like_summary function
void FileComparator::print_diff_like_summary(
    const SummaryParams& params) const {
  // Handle identical files case
  if (counter.diff_non_zero == 0) {
    print_identical_files_message(params);
    return;
  }

  // Early return for non-trivial differences at low print levels
  if (counter.diff_non_trivial > 0 && print.level < 1) {
    return;
  }

  // Print summary sections for files with differences
  print_exact_matches_info(params);
  print_non_zero_differences_info(params);
  print_difference_counts(params);
  print_maximum_difference_analysis(params);

  printbar(1);
}

void FileComparator::print_rounded_summary(const SummaryParams& params) const {
  // Trivial differences
  // =========================================================
  if (counter.diff_non_trivial == 0) {
    std::cout << "\033[1;32m   Files " << params.file1 << " and "
              << params.file2
              << " are equivalent"
              //   << " with trivial differences due to formatting"
              << "\033[0m" << std::endl;
    return;
  }
  if (counter.diff_significant > 0 && print.level < 1) {
    return;
  }
if (print.level > 0) {

  if (counter.diff_non_zero > counter.diff_non_trivial) {
    size_t zero_diff = counter.diff_non_zero - counter.diff_non_trivial;
    if (zero_diff > 0) {
      std::cout << "   Trivial differences     ( >" << 0.0 << "): ";
      if (zero_diff > 0) {
        std::cout << "\033[1;32m";
      } else if (counter.diff_significant > 0) {
        std::cout << "\033[1;31m";
      } else {
        std::cout << "\033[1;33m";
      }
      std::cout << std::setw(params.fmt_wid) << zero_diff << "\033[0m"
                << std::endl;
    }
  }
  std::cout << "   Non-trivial differences      : ";
  if (counter.diff_non_trivial > 0) {
    std::cout << "\033[1;33m";
  } else if (counter.diff_significant > 0) {
    std::cout << "\033[1;31m";
  } else {
    std::cout << "\033[1;32m";
  }

  std::cout << std::setw(params.fmt_wid) << counter.diff_non_trivial
            << "\033[0m" << std::endl;

  std::cout << "\033[1;33m   Files " << params.file1 << " and " << params.file2
            << " are non-trivially different\033[0m" << std::endl;
}

  std::cout << "   \033[4;35mMaximum rounded difference: "
            << format_number(
                   differ.max_non_trivial, differ.ndp_non_trivial,
                   static_cast<int>(
                       std::round(std::log10(differ.max_non_trivial)) + 2),
                   differ.ndp_non_trivial)
            << "\033[0m" << std::endl;
  if (counter.diff_print < counter.diff_non_trivial) {
    if (print.level>0) {
    std::cout << "   Printed differences     ( >" << thresh.print
              << "): " << std::setw(params.fmt_wid) << counter.diff_print
              << std::endl;
  }
    size_t not_printed = counter.diff_non_trivial - counter.diff_print;
    if (not_printed > 0) {
      std::cout << "   Not printed differences (" << thresh.significant << " < TL <= " << thresh.print
                << "): " << std::setw(params.fmt_wid) << not_printed
                << std::endl;
    }
  } else {
    std::cout << "   All non-trivial "
                 "differences are printed."
              << std::endl;
  }

  if (differ.max_non_trivial > thresh.significant) {
    std::cout
        << "\033[1;31m   Max diff is greater than the significant threshold: "
        << thresh.significant << "\033[0m" << std::endl;
  } else {
    if (fabs(differ.max_non_trivial - thresh.significant) < thresh.zero) {
      std::cout << "\033[1;33m   Max diff is equal to the "
                   "significant threshold: "
                << format_number(
                       thresh.significant, differ.ndp_non_trivial,
                       static_cast<int>(
                           std::round(std::log10(thresh.significant)) + 2),
                       differ.ndp_non_trivial)
                << "\033[0m" << std::endl;
    } else {
      std::cout << "\033[1;32m   Max diff is less than the "
                   "significant threshold: "
                << format_number(
                       thresh.significant, differ.ndp_non_trivial,
                       static_cast<int>(
                           std::round(std::log10(thresh.significant)) + 2),
                       differ.ndp_non_trivial)
                << "\033[0m" << std::endl;
    }
  }

  printbar(1);
}

void FileComparator::print_significant_summary(
    const SummaryParams& params) const {
  // User-defined threshold differences
  // =========================================================
  if (counter.diff_significant == 0) {
    std::cout << "\033[1;32mFiles " << params.file1 << " and "
              << params.file2 << " are equivalent within tolerance\033[0m"
              << std::endl;
    return;
  }

  if (print.level < 0) {
    return; // Early return for low print levels
  }

  print_significant_differences_count(params);
  print_insignificant_differences_count(params);
  print_maximum_significant_difference_analysis(params);
  print_file_comparison_result(params);
  print_significant_differences_printing_status(params);

  printbar(1);
}

void FileComparator::print_significant_differences_count(
    const SummaryParams& params) const {
  std::cout << "   Significant differences   ( >" << thresh.significant
            << "): ";

  std::cout << "\033[1;31m" << std::setw(params.fmt_wid)
            << counter.diff_significant << "\033[0m";

  if (counter.elem_number > 0) {
    print_significant_percentage();
  }
}

void FileComparator::print_significant_percentage() const {
  double percent = 100.0 * static_cast<double>(counter.diff_significant) /
                   static_cast<double>(counter.elem_number);
  std::cout << " (" << std::fixed << std::setw(5) << std::setprecision(2)
            << percent << "%)" << std::endl;

  // Check if probably OK
  constexpr double threshold_percent = 2.0;
  if (percent < threshold_percent) {
    std::cout << "   \033[1;33mProbably OK: error percent less than "
              << threshold_percent << "%  \033[0m" << std::endl;
    flag.files_are_close_enough = true;
    print_flag_status();
  }
}

void FileComparator::print_insignificant_differences_count(
    const SummaryParams& params) const {
  if (counter.diff_non_trivial <= counter.diff_significant) {
    return;
  }

  size_t insignificant_count = counter.diff_non_trivial - counter.diff_significant;
  if (insignificant_count > 0) {
    print_count_with_percent(params, "Insignificant differences (<=" +
                             std::to_string(thresh.significant) + ")",
                             insignificant_count);
  }
}

void FileComparator::print_maximum_significant_difference_analysis(
    const SummaryParams& params) const {
  if (differ.max_significant > thresh.significant) {
    print_maximum_significant_difference_details();
    print_max_diff_threshold_comparison_above();
  } else {
    print_max_diff_threshold_comparison_below();
  }
}

void FileComparator::print_maximum_significant_difference_details() const {
  std::cout << "   \033[4;35mMaximum significant difference: "
            << format_number(
                   differ.max_significant, differ.ndp_significant,
                   static_cast<int>(
                       std::round(std::log10(differ.max_significant)) + 2),
                   differ.ndp_significant)
            << "\033[0m" << std::endl;

  if (differ.ndp_significant > differ.ndp_single_precision) {
    std::cout << "   \033[1;33mProbably OK: single precision exceeded"
              << "\033[0m" << std::endl;
    // Set flag for files being close enough
    flag.files_are_close_enough = true;
    print_flag_status();
  }
}

void FileComparator::print_max_diff_threshold_comparison_above() const {
  std::cout << "\033[1;31m   Max diff is greater than the significant threshold: "
            << std::setprecision(differ.ndp_significant) << thresh.significant
            << "\033[0m" << std::endl;
}

void FileComparator::print_max_diff_threshold_comparison_below() const {
  std::cout << "\033[1;32m   Max diff is less than or equal to the "
               "significant threshold: "
            << format_number(
                   thresh.significant, differ.ndp_significant,
                   static_cast<int>(
                       std::round(std::log10(thresh.significant)) + 2),
                   differ.ndp_significant)
            << "\033[0m" << std::endl;
}

void FileComparator::print_file_comparison_result(
    const SummaryParams& params) const {
  std::cout << "   ";
  if (differ.ndp_significant > differ.ndp_single_precision) {
    std::cout << "\033[1;33mFiles " << params.file1 << " and " << params.file2
              << " are equivalent within the limits of single precision\033[0m" << std::endl;
  } else {
    std::cout << "\033[1;31mFiles " << params.file1 << " and " << params.file2
              << " are significantly different\033[0m" << std::endl;
  }
}

void FileComparator::print_significant_differences_printing_status(
    const SummaryParams& params) const {
  if (counter.diff_print < counter.diff_significant) {
    print_count_with_percent(params,
        "Printed differences       ( >" + std::to_string(thresh.print) + ")",
        counter.diff_print);

    size_t not_printed_signif = counter.diff_significant - counter.diff_print;
    if (not_printed_signif > 0) {
      std::cout << "\033[1;31m   Not printed differences   (<=" << thresh.print
                << "): " << std::setw(params.fmt_wid) << not_printed_signif
                << "\033[0m" << std::endl;
    }
  } else {
    std::cout << "   All significant differences are printed." << std::endl;
  }
}

void FileComparator::print_count_with_percent(const SummaryParams& params,
                                              const std::string& label,
                                              size_t count,
                                              const std::string& color) const {
  std::cout << "   " << label << ": ";
  if (!color.empty()) std::cout << color;
  std::cout << std::setw(params.fmt_wid) << count;
  if (!color.empty()) std::cout << "\033[0m";

  if (counter.elem_number > 0) {
    double percent = 100.0 * static_cast<double>(count) /
                     static_cast<double>(counter.elem_number);
    std::cout << " (" << std::fixed << std::setprecision(2) << percent
              << "%)";
  }
  std::cout << std::endl;
}

// Helper function for boolean formatting
std::string FileComparator::format_boolean_status(bool value, bool showStatus,
                                                  bool reversed,
                                                  bool soft = false) const {
  std::ostringstream oss;
  if (!showStatus) {
    oss << (value ? "TRUE" : "FALSE");
  } else {
    bool pass_condition = reversed ? !value : value;
    std::string status =
        soft ? "\033[1;33m(OK)\033[0m" : "\033[1;31m(FAIL)\033[0m";
    if (pass_condition) {
      oss << (value ? "TRUE " : "FALSE") << " \033[1;32m(PASS)\033[0m";
    } else {
      oss << (value ? "TRUE " : "FALSE") << " " << status;
    }
  }
  return oss.str();
}

// Helper function to print command line arguments and file info
void FileComparator::print_arguments_and_files(const std::string& file1,
                                               const std::string& file2,
                                               int argc, char* argv[]) const {
  if (print.level < 1) {
    return;
  }

  if (print.debug) {
    std::cout << "ARGUMENTS:" << std::endl;
  }
  // print command line arguments
  std::cout << "   Input:";
  for (int i = 0; i < argc; ++i) {
    std::cout << " " << argv[i];
  }
  std::cout << std::endl;
  if (print.debug) {
    std::cout << "   File1: " << file1 << std::endl;
    std::cout << "   File2: " << file2 << std::endl;
  }
}

// Helper function to print statistics
void FileComparator::print_statistics(const std::string& file1) const {
  if (print.level < 0) {
    return;
  }
  std::cout << "STATISTICS:" << std::endl;
  std::cout << "   Total lines compared: " << counter.line_number;

  // Check if all lines were compared
  if (size_t length1 = get_file_length(file1); length1 == counter.line_number) {
    std::cout << " (all)" << std::endl;
  } else {
    std::cout << " of " << length1 << std::endl;
    // print how many lines were not compared
    size_t missing_lines = length1 - counter.line_number;
    std::cout << "\033[1;31m   " << missing_lines
              << " lines were not compared\033[0m" << std::endl;
  }

  std::cout << "   Total elements checked: " << counter.elem_number;
  if (flag.file_end_reached) {
    std::cout << " (all)" << std::endl;
  } else {
    std::cout << " \033[1;31m(file end not reached)\033[0m" << std::endl;
  }
}

// Helper function to print flag status
void FileComparator::print_flag_status() const {
  if (print.level < 1) {
    return;
  }
  std::cout << "FLAGS:" << std::endl;
  std::cout << "   error_found: "
            << format_boolean_status(flag.error_found, true, true) << std::endl;

  if (counter.elem_number > 0) {
    std::cout << "   Pass/fail Status" << std::endl;
    std::cout << "      files_are_same        : "
              << format_boolean_status(flag.files_are_same, true, false)
              << std::endl;
    std::cout << "      files_have_same_values: "
              << format_boolean_status(flag.files_have_same_values, true, false)
              << std::endl;
    std::cout << "      files_are_close_enough: "
              << format_boolean_status(flag.files_are_close_enough, true, false)
              << std::endl;

    std::cout << "   Counter status:" << std::endl;
    std::cout << "      has_non_zero_diff   : "
              << format_boolean_status(flag.has_non_zero_diff, true, true,
                                       flag.files_are_close_enough)
              << std::endl;
    std::cout << "      has_non_trivial_diff: "
              << format_boolean_status(flag.has_non_trivial_diff, true, true,
                                       flag.files_are_close_enough)
              << std::endl;
    std::cout << "      has_significant_diff: "
              << format_boolean_status(flag.has_significant_diff, true, true)
              << std::endl;
    std::cout << "      has_critical_diff   : "
              << format_boolean_status(flag.has_critical_diff, true, true)
              << std::endl;
    std::cout << "      has_printed_diff    : "
              << format_boolean_status(flag.has_printed_diff, true, true,
                                       thresh.print <= thresh.significant)
              << std::endl;
    std::cout << "   new_fmt: "
              << format_boolean_status(flag.new_fmt, false, false) << std::endl;
  }
}

// Helper function to print counter information
void FileComparator::print_counter_info() const {
  if (print.level <= 0) {
    return;
  }
  std::cout << "COUNTERS:" << std::endl;

  if (counter.elem_number == 0) {
    std::cout << "   \033[1;31mNo elements were checked.\033[0m" << std::endl;
    return;
  }
  // Get the width of elem_number for alignment
  auto width = static_cast<int>(std::to_string(counter.elem_number).length());
  // Print all counters with aligned colons
  std::cout << "   line_number      : " << std::setw(width)
            << counter.line_number << std::endl;
  std::cout << "   elem_number      : " << std::setw(width)
            << counter.elem_number << std::endl;
  std::cout << "   diff_non_zero    : ";
  if (counter.diff_non_zero == 0) {
    std::cout << "\033[1;32m";
  }
  std::cout << std::setw(width) << counter.diff_non_zero << "\033[0m"
            << std::endl;
  if (counter.diff_non_zero == 0) {
    return;
  }
  std::cout << "   diff_non_trivial : ";
  if (counter.diff_non_trivial == 0) {
    std::cout << "\033[1;32m";
  }
  std::cout << std::setw(width) << counter.diff_non_trivial << "\033[0m"
            << std::endl;
  if (counter.diff_non_trivial == 0) {
    return;
  }
  std::cout << "   diff_significant : " << std::setw(width)
            << counter.diff_significant << std::endl;
  if (counter.diff_significant == 0) {
    return;
  }
  std::cout << "   diff_print       : " << std::setw(width)
            << counter.diff_print << std::endl;
  if (counter.diff_print == 0) {
    return;
  }
  std::cout << "   diff_critical    : " << std::setw(width)
            << counter.diff_critical << std::endl;
}

// Helper function to print detailed summary sections
void FileComparator::print_detailed_summary(const SummaryParams& params) const {
  // step through flags, printing only one summary section
  print_diff_like_summary(params);
  if (counter.diff_non_zero == 0) {
    return;  // No non-zero differences to print
  }
  print_rounded_summary(params);
  if (counter.diff_non_trivial == 0) {
    return;  // No non-trivial differences to print
  }
  print_significant_summary(params);
}

// Helper function to print additional difference information
void FileComparator::print_additional_diff_info(
    const SummaryParams& params) const {
  if (counter.diff_non_zero == 0) {
    return;  // No non-zero differences to print
  }
  if (counter.diff_non_trivial == 0) {
    return;  // No non-trivial differences to print
  }
  if (counter.diff_significant == 0) {
    return;  // No significant differences to print
  }

  // Handle case with no printed differences
  if (counter.diff_print == 0) {
    std::cout << "\033[1;32m   Files " << params.file1 << " and "
              << params.file2 << " are identical within print threshold\033[0m"
              << std::endl;
  }
}

// Helper function to print critical threshold information
void FileComparator::print_critical_threshold_info() const {
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
  }
}

// Simplified main print_summary function
void FileComparator::print_summary(const std::string& file1,
                                   const std::string& file2, int argc,
                                   char* argv[]) const {
  print_arguments_and_files(file1, file2, argc, argv);

  auto fmt_wid = static_cast<int>(std::to_string(counter.elem_number).length());
  SummaryParams params{file1, file2, fmt_wid};
  print_settings(params.file1, params.file2);
  print_statistics(params.file1);
  print_flag_status();
  print_counter_info();

  // Print summary header if needed
  if ((!print.diff_only || print.debug || flag.error_found) &&
      (print.level >= 0 || counter.diff_non_zero > 0)) {
    std::cout << "SUMMARY:" << std::endl;
  }

  // Early return for errors
  if (flag.error_found) {
    return;
  }

  print_detailed_summary(params);
  print_additional_diff_info(params);
  print_critical_threshold_info();
}

void FileComparator::print_settings(const std::string& file1,
                                    const std::string& file2) const {
  if (print.level < 0) {
    return;
  }
  std::cout << "SETTINGS: " << std::endl;
  if (print.debug || print.level > 0) {
    std::cout << "   Debug mode : " << (print.debug ? "ON" : "OFF")
              << std::endl;
    std::cout << "   Debug level: " << print.level << std::endl;
    std::cout << "   Print mode : " << (print.diff_only ? "DIFF" : "FULL")
              << std::endl;
    std::cout << "   File1: " << file1 << std::endl;
    std::cout << "   File2: " << file2 << std::endl;
  }

  std::cout << "   User-defined Thresholds " << std::endl;
  std::cout << "      Significant: \033[1;36m" << thresh.significant
            << "\033[0m (count)" << std::endl;
  std::cout << "      Critical   : \033[1;31m" << thresh.critical
            << "\033[0m (halt)" << std::endl;
  std::cout << "      Print      : " << thresh.print << " (print)" << std::endl;
  if (print.level > 0) {
    std::cout << "   Fixed Thresholds " << std::endl;
    std::cout << "      Zero       : " << thresh.zero << std::endl;
    std::cout << "      Marginal   : \033[1;33m" << thresh.marginal << "\033[0m"
              << std::endl;
    std::cout << "      Ignore     : \033[1;34m" << thresh.ignore
              << "\033[0m (maximum TL)" << std::endl;
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

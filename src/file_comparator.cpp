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
 * @date March 2025
 * Adapted from src/idiff.cpp
 *
 * @note This program was developed from the Fortran code tldiff.f90 and it's
 * derivatives cpddiff.f90, prsdiff.f90, and tsdiff.f90 (August 2022). It is
 * intended to replace those utilities with a more modern C++ implementation.
 * The initial attempt at converting a generalized version of the code was
 * idiff.cpp (January 2025). This program, originally uband_diff.cpp, was
 * adapted from idiff.cpp (March 2025).
 *
 * Development of this code was significantly assisted by GitHub Copilot (GPT-4,
 * Claude Sonnet 4, etc.) for code translation, refactoring, optimization, and
 * architectural improvements.
 *
 */
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "precision_info.h"
#include "uband_diff.h"

// Implementation moved outside the class definition

// Global utility functions

// Structure to hold precision information for both fixed and scientific
// notation Helper function to count significant figures in a numeric string
int count_significant_figures(const std::string& num_str) {
  std::string cleaned = num_str;

  // Remove any whitespace
  cleaned.erase(std::remove_if(cleaned.begin(), cleaned.end(), ::isspace),
                cleaned.end());

  // Handle negative sign
  if (!cleaned.empty() && cleaned[0] == '-') {
    cleaned = cleaned.substr(1);
  }

  // Handle empty or zero cases
  if (cleaned.empty() || cleaned == "0" || cleaned == "0.0") {
    return 1;  // Zero has 1 significant figure
  }

  int sig_figs = 0;
  bool started_counting = false;

  for (size_t i = 0; i < cleaned.size(); ++i) {
    char c = cleaned[i];

    if (c == '.') continue;  // Skip decimal point

    if (std::isdigit(c)) {
      if (c != '0') {
        started_counting = true;
        sig_figs++;
      } else if (started_counting) {
        // Zero counts as significant if we've already started counting
        sig_figs++;
      }
      // Leading zeros don't count as significant
    }
  }

  return sig_figs > 0 ? sig_figs : 1;  // Minimum 1 significant figure
}

// Enhanced function to analyze number precision in both fixed and scientific
// notation
PrecisionInfo stream_analyzePrecision(std::istringstream& stream) {
  // Peek ahead to determine the format of the number
  std::streampos pos = stream.tellg();
  std::string token;
  stream >> token;

  PrecisionInfo info;

  // Convert to lowercase for easier parsing
  std::string lower_token = token;
  std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(),
                 ::tolower);

  // Check if it's scientific notation (contains 'e' or 'd')
  size_t e_pos = lower_token.find('e');
  if (e_pos == std::string::npos) {
    e_pos = lower_token.find('d');  // Fortran double precision notation
  }

  if (e_pos != std::string::npos) {
    // Scientific notation
    info.is_scientific = true;

    // Extract mantissa and exponent
    std::string mantissa = token.substr(0, e_pos);
    std::string exp_str = token.substr(e_pos + 1);

    try {
      info.exponent = std::stoi(exp_str);
    } catch (const std::exception&) {
      info.exponent = 0;  // Default on parse error
    }

    // Count significant figures in mantissa
    info.significant_figures = count_significant_figures(mantissa);

    // Calculate effective precision
    info.effective_precision =
        std::pow(10.0, -(info.significant_figures - 1 - info.exponent));

#ifdef DEBUG3
    std::cout << "DEBUG3: Scientific notation " << token
              << " -> mantissa: " << mantissa << ", exponent: " << info.exponent
              << ", sig figs: " << info.significant_figures
              << ", effective decimal places: "
              << info.get_effective_decimal_places() << std::endl;
#endif
  } else {
    // Fixed notation
    info.is_scientific = false;

    // Count decimal places
    if (size_t decimal_pos = token.find('.');
        decimal_pos != std::string::npos) {
      info.decimal_places = static_cast<int>(token.size() - decimal_pos - 1);
    }

    // Also count significant figures for consistency checks
    info.significant_figures = count_significant_figures(token);

    // For fixed notation, effective precision is simply 10^(-decimal_places)
    info.effective_precision = std::pow(10.0, -info.decimal_places);

#ifdef DEBUG3
    std::cout << "DEBUG3: Fixed notation " << token
              << " -> decimal places: " << info.decimal_places
              << ", sig figs: " << info.significant_figures << std::endl;
#endif
  }

  // Parse the actual numerical value
  try {
    info.parsed_value = std::stod(token);
  } catch (const std::exception&) {
    info.parsed_value = 0.0;  // Default on parse error
  }

  // Check for single precision warning (6-7 significant figures)
  if (info.significant_figures >= 6 && info.significant_figures <= 7) {
    info.has_single_precision_warning = true;
  }

  // Reset stream to before reading token
  stream.clear();
  stream.seekg(pos);

  return info;
}

// Legacy function for backward compatibility with enhanced scientific notation
// support
int stream_countDecimalPlaces(std::istringstream& stream) {
  std::streampos pos = stream.tellg();
  std::string token;
  stream >> token;

  int ndp = 0;

  // Convert to lowercase for easier parsing
  std::string lower_token = token;
  std::transform(lower_token.begin(), lower_token.end(), lower_token.begin(),
                 ::tolower);

  // Check if it's scientific notation (contains 'e' or 'd')
  size_t e_pos = lower_token.find('e');
  if (e_pos == std::string::npos) {
    e_pos = lower_token.find('d');  // Fortran double precision notation
  }

  if (e_pos != std::string::npos) {
    // Scientific notation - calculate effective decimal places
    std::string mantissa = token.substr(0, e_pos);
    std::string exp_str = token.substr(e_pos + 1);

    int exponent = 0;
    try {
      exponent = std::stoi(exp_str);
    } catch (const std::exception&) {
      exponent = 0;  // Default on parse error
    }

    // Count significant figures in mantissa
    int sig_figs = count_significant_figures(mantissa);

    // Calculate effective decimal places: sig_figs - 1 - exponent
    // e.g., 1.23e-5 has 3 sig figs, so 3-1-(-5) = 7 effective decimal places
    // For large positive exponents (e.g., 1.23e10), this can be negative,
    // which means the number is effectively an integer with no decimal
    // precision
    ndp = sig_figs - 1 - exponent;

    // Clamp to reasonable bounds (0 to 10 decimal places)
    if (ndp < 0) {
      ndp = 0;  // Large numbers like 1.23e10 have no decimal precision
    } else if (ndp > 10) {
      ndp = 10;  // Cap at maximum reasonable decimal places
    }

#ifdef DEBUG3
    std::cout << "DEBUG3: Scientific notation " << token
              << " -> sig figs: " << sig_figs << ", exponent: " << exponent
              << ", effective decimal places: " << ndp << std::endl;
#endif
  } else {
    // Fixed notation - original logic
    if (size_t decimal_pos = token.find('.');
        decimal_pos != std::string::npos) {
      ndp = static_cast<int>(token.size() - decimal_pos - 1);
    }

#ifdef DEBUG3
    std::cout << "DEBUG3: Fixed notation " << token << " has ";
    std::cout << ndp << " decimal places" << std::endl;
#endif
  }

  // Reset stream to before reading token, so the original extraction works
  stream.clear();
  stream.seekg(pos);

  return ndp;
}

// Function to check if precision exceeds single precision limits
bool check_precision_warning(const PrecisionInfo& info,
                             int single_precision_limit = 7) {
  if (info.is_scientific) {
    // For scientific notation, check significant figures
    if (info.significant_figures > single_precision_limit) {
      std::cout << "\033[1;33mWarning: Scientific notation with "
                << info.significant_figures << " significant figures exceeds "
                << "single precision limit (" << single_precision_limit
                << "). Results may be unreliable.\033[0m" << std::endl;
      return true;
    }
  } else {
    // For fixed notation, check decimal places
    if (info.decimal_places > single_precision_limit) {
      std::cout << "\033[1;33mWarning: Fixed notation with "
                << info.decimal_places << " decimal places exceeds "
                << "single precision limit (" << single_precision_limit
                << "). Results may be unreliable.\033[0m" << std::endl;
      return true;
    }
  }
  return false;
};

/**
 * readComplex() - Enhanced complex number parser (v2.0)
 *
 * Returns the real and imaginary parts of a complex number along with
 * precise decimal place counts for each component.
 *
 * Format: Assumes complex numbers in the format "real, imag)" where:
 * - real and imag are floating point numbers
 * - comma is used as the separator
 * - the opening '(' has already been consumed by caller
 * - supports flexible whitespace: "1.0, 2.0)" or "1.0,2.0)" etc.
 *
 * Examples of supported input:
 * - "1.0, 2.0)" → real=1.0, imag=2.0, dp_real=1, dp_imag=1
 * - "3.14, 2.718)" → real=3.14, imag=2.718, dp_real=2, dp_imag=3
 * - "5, 7)" → real=5.0, imag=7.0, dp_real=0, dp_imag=0
 *
 * Key improvements from previous version:
 * - Accurate decimal place counting (was previously always 0)
 * - Robust string-based parsing (vs problematic stream repositioning)
 * - Better error handling and input validation
 * - Proper stream position management for sequential parsing
 */

/**
 * @brief Reads a complex number from an input stream and returns its components
 *
 * This function parses complex numbers in the format "real, imag)" where the
 * opening parenthesis '(' has already been consumed by the calling code. It
 * extracts both the numeric values and accurately counts decimal places for
 * precision tracking.
 *
 * @param stream Input string stream positioned after the opening '('
 * @param flag Reference to Flags struct for error reporting
 * @return std::tuple<double, double, int, int> containing:
 *         - real part (double)
 *         - imaginary part (double)
 *         - decimal places in real part (int)
 *         - decimal places in imaginary part (int)
 *
 * Expected input format: "1.5, 2.0)" or "3.14, 2.718)" etc.
 *
 * IMPLEMENTATION CHANGES (v2.0):
 * Previous version had several critical issues:
 * 1. Used complex stream repositioning that failed to work correctly
 * 2. Relied on stream >> operators which consumed original formatting info
 * 3. Had race conditions with stream position management
 * 4. Decimal place counting always returned 0 due to stream state issues
 *
 * New implementation improvements:
 * 1. Manual string parsing: Extracts remaining stream content as string for
 * parsing
 * 2. Direct substring extraction: Uses find() to locate comma and closing paren
 * 3. Preserved formatting: Keeps original text to accurately count decimal
 * places
 * 4. Robust error handling: Validates format before processing
 * 5. Proper stream advancement: Correctly positions stream for next token
 *
 * Error conditions:
 * - Missing comma separator
 * - Missing closing parenthesis
 * - Invalid number format
 * - Malformed input structure
 */
std::tuple<double, double, int, int> readComplex(std::istringstream& stream,
                                                 Flags& flag) {
  // Extract the remaining content from the current stream position
  // This preserves the original formatting needed for decimal place counting
  std::string content = stream.str();
  std::string remaining = content.substr(stream.tellg());

  // Locate key structural elements: comma separator and closing parenthesis
  size_t comma_pos = remaining.find(',');
  size_t paren_pos = remaining.find(')');

  // Validate the basic structure of the complex number format
  if (comma_pos == std::string::npos || paren_pos == std::string::npos ||
      comma_pos >= paren_pos) {
    std::cerr << "Error reading complex number";
    flag.error_found = true;
    return {0.0, 0.0, -1,
            -1};  // Return -1 for decimal places to indicate error
  }

  // Extract the real and imaginary parts as strings to preserve formatting
  // Real part: from start to comma position
  // Imaginary part: from after comma to closing parenthesis
  std::string real_str = remaining.substr(0, comma_pos);
  std::string imag_str =
      remaining.substr(comma_pos + 1, paren_pos - comma_pos - 1);

  // Remove leading and trailing whitespace from both parts
  // This handles variations like "1.5, 2.0" vs "1.5,2.0" vs " 1.5 , 2.0 "
  real_str.erase(0, real_str.find_first_not_of(" \t"));
  real_str.erase(real_str.find_last_not_of(" \t") + 1);
  imag_str.erase(0, imag_str.find_first_not_of(" \t"));
  imag_str.erase(imag_str.find_last_not_of(" \t") + 1);

  // Convert the cleaned strings to floating-point numbers
  // This will be done by the precision analysis below
  double real;
  double imag;

  // Analyze precision using the enhanced precision analysis
  // This handles both fixed-point and scientific notation
  std::istringstream real_stream(real_str);
  std::istringstream imag_stream(imag_str);

  PrecisionInfo real_precision = stream_analyzePrecision(real_stream);
  PrecisionInfo imag_precision = stream_analyzePrecision(imag_stream);

  // Use effective decimal places for consistent comparison
  int real_dp = real_precision.get_effective_decimal_places();
  int imag_dp = imag_precision.get_effective_decimal_places();

  // Use the parsed values from precision analysis (handles scientific notation)
  real = real_precision.parsed_value;
  imag = imag_precision.parsed_value;

  // Advance the original stream past the parsed content so the next
  // read operation starts at the correct position for subsequent tokens
  stream.seekg(stream.tellg() + static_cast<std::streampos>(paren_pos + 1));

  return {real, imag, real_dp, imag_dp};
}

/**
 * @brief Lambda function to round a double to a specified number of decimal
 * places
 * @param value The floating-point value to round
 * @param precision Number of decimal places to round to
 * @return Rounded value
 */
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

  if (!file_reader_->open_files(file1, file2, infile1, infile2)) {
    flag.error_found = true;
    return false;
  }

  // Perform column structure analysis before numerical comparison
  bool structures_compatible = true;
  if (print.level > 0 || print.debug) {
    // Full detailed analysis for debug mode
    std::cout << "\n\033[1;36m=== Column Structure Analysis ===\033[0m"
              << std::endl;
    structures_compatible =
        file_reader_->compare_column_structures(file1, file2);

    if (!structures_compatible) {
      std::cout << "\033[1;33mNote: Files have different column structures but "
                   "comparison will continue.\033[0m\n"
                << std::endl;
    } else {
      std::cout << "\033[1;32mColumn structures are compatible. Proceeding "
                   "with numerical comparison.\033[0m\n"
                << std::endl;
    }
  } else {
    // Quick structure check for normal operation
    ColumnStructure struct1 = file_reader_->analyze_column_structure(file1);
    ColumnStructure struct2 = file_reader_->analyze_column_structure(file2);

    if (struct1.groups.size() != struct2.groups.size() ||
        (struct1.groups.size() > 0 && struct2.groups.size() > 0 &&
         struct1.groups.back().column_count !=
             struct2.groups.back().column_count)) {
      structures_compatible = false;
      std::cout
          << "\033[1;33mStructure Note: Files have different column formats "
          << "(File1: "
          << (struct1.groups.empty() ? 0 : struct1.groups.back().column_count)
          << " cols, File2: "
          << (struct2.groups.empty() ? 0 : struct2.groups.back().column_count)
          << " cols)\033[0m" << std::endl;
    }
  }

  // Set structure compatibility flag
  flag.structures_compatible = structures_compatible;

  // If structures are incompatible, files cannot be considered identical or
  // close enough
  if (!structures_compatible) {
    flag.files_are_same = false;
    flag.files_have_same_values = false;
    flag.files_are_close_enough = false;
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
  if (!file_reader_->compare_file_lengths(file1, file2)) {
    return false;
  }
  return flag.files_are_close_enough && !flag.error_found;
}

LineData FileComparator::parse_line(const std::string& line) const {
  // Delegate to LineParser
  return line_parser_->parse_line(line, flag, counter.line_number);
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
  return format_tracker_->validate_and_track_column_format(
      n_col1, n_col2, dp_per_col, prev_n_col, counter.line_number, flag);
}

bool FileComparator::validate_decimal_column_size(
    const std::vector<int>& dp_per_col, size_t column_index) const {
  return format_tracker_->validate_decimal_column_size(dp_per_col, column_index,
                                                       counter.line_number);
}

// ========================================================================
// Decimal Places Management
// ========================================================================
bool FileComparator::initialize_decimal_place_format(
    const int min_dp, const size_t column_index, std::vector<int>& dp_per_col) {
  bool result = format_tracker_->initialize_decimal_place_format(
      min_dp, column_index, dp_per_col, counter.line_number, flag);
  if (result) {
    this_fmt_line = format_tracker_->get_format_line();
    this_fmt_column = format_tracker_->get_format_column();
  }
  return result;
}

bool FileComparator::update_decimal_place_format(const int min_dp,
                                                 const size_t column_index,
                                                 std::vector<int>& dp_per_col) {
  bool result = format_tracker_->update_decimal_place_format(
      min_dp, column_index, dp_per_col, counter.line_number, flag);
  if (result && flag.new_fmt) {
    this_fmt_line = format_tracker_->get_format_line();
  }
  return result;
}

double FileComparator::calculate_threshold(int ndp) {
  // Use FormatTracker for the core threshold calculation
  double dp_threshold =
      format_tracker_->calculate_threshold(ndp, thresh.significant);

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
  // Calculate threshold for this column
  double ithreshold = calculate_threshold(column_data.min_dp);

  // Process the difference using DifferenceAnalyzer
  bool result = difference_analyzer_->process_difference(
      column_data, column_index, ithreshold, counter, differ, flag);

  // Calculate unrounded and rounded differences for comparison
  double diff_unrounded = std::abs(column_data.value1 - column_data.value2);
  double rounded1 = DifferenceAnalyzer::round_to_decimals(column_data.value1,
                                                          column_data.min_dp);
  double rounded2 = DifferenceAnalyzer::round_to_decimals(column_data.value2,
                                                          column_data.min_dp);
  double diff_rounded = std::abs(rounded1 - rounded2);

  // For rigorous cross-platform validation, use unrounded differences for
  // thresholding but still display both for complete information
  double diff_for_threshold = diff_unrounded;

  // Print differences if above plot threshold
  if (diff_for_threshold > thresh.print) {
    print_table(column_data, column_index, ithreshold, diff_rounded,
                diff_unrounded);
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

  return result;
}

void FileComparator::process_raw_values(const ColumnValues& column_data) {
  difference_analyzer_->process_raw_values(column_data, counter, differ, flag);
}

void FileComparator::process_rounded_values(const ColumnValues& column_data,
                                            size_t column_index,
                                            double rounded_diff,
                                            int minimum_deci) {
  double ithreshold = calculate_threshold(column_data.min_dp);
  difference_analyzer_->process_rounded_values(
      column_data, column_index, rounded_diff, minimum_deci, ithreshold,
      counter, differ, flag);
}

// ========================================================================
// Output & Formatting
// ========================================================================
void FileComparator::print_table(const ColumnValues& column_data,
                                 size_t column_index, double line_threshold,
                                 double diff_rounded, double diff_unrounded) {
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
  //                     line | col | range | file1 |
  //                     file2 | thres |diff_rounded | diff_unrounded
  std::vector<int> col_widths = {5,         5,         val_width, val_width,
                                 val_width, val_width, val_width, val_width};

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
    std::cout << padLeft("diff_rnd |", col_widths[6] + 3);
    std::cout << padLeft("diff_raw", col_widths[7] + 1) << std::endl;

    // Print horizontal line matching header width
    int total_width = 0;
    for (int w : col_widths) total_width += w;
    // Add spaces for extra padding in header columns
    total_width +=
        1 + 3 + 3 + 3 +
        1;  // file1(+1), file2(+3), thres(+3), diff_rnd(+3), diff_raw(+1)
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

  // rounded difference
  print_diff_color(column_data.value1, column_data.value2, diff_rounded);
  std::cout << format_number(diff_rounded, column_data.min_dp, mxint, mxdec);
  std::cout << "\033[0m |";

  // unrounded difference (show with precision of the more precise input)
  print_diff_color(column_data.value1, column_data.value2, diff_unrounded);
  std::cout << format_number(diff_unrounded, column_data.max_dp, mxint, mxdec);
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
  difference_analyzer_->print_hard_threshold_error(
      rounded1, rounded2, diff_rounded, column_index, counter);
  flag.error_found = true;
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
  // Handle structure incompatibility first
  if (!flag.structures_compatible) {
    std::cout << "\033[1;31m   Files " << params.file1 << " and "
              << params.file2 << " have incompatible column structures\033[0m"
              << std::endl;
    return;
  }

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
  // Skip if structures are incompatible
  if (!flag.structures_compatible) {
    return;
  }

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
    if (counter.diff_trivial > 0) {
      std::cout << "   Trivial differences     ( >" << 0.0 << "): ";
      std::cout << "\033[1;32m";
      std::cout << std::setw(params.fmt_wid) << counter.diff_trivial
                << "\033[0m" << std::endl;
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

    std::cout << "\033[1;33m   Files " << params.file1 << " and "
              << params.file2 << " are non-trivially different\033[0m"
              << std::endl;
  }

  std::cout << "   \033[4;35mMaximum rounded difference: "
            << format_number(
                   differ.max_non_trivial, differ.ndp_non_trivial,
                   static_cast<int>(
                       std::round(std::log10(differ.max_non_trivial)) + 2),
                   differ.ndp_non_trivial)
            << "\033[0m" << std::endl;
  if (counter.diff_print < counter.diff_non_trivial) {
    if (print.level > 0) {
      std::cout << "   Printed differences     ( >" << thresh.print
                << "): " << std::setw(params.fmt_wid) << counter.diff_print
                << std::endl;
    }
    size_t not_printed = counter.diff_non_trivial - counter.diff_print;
    if (not_printed > 0) {
      std::cout << "   Not printed differences (" << thresh.significant
                << " < TL <= " << thresh.print
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
                           std::round(thresh.get_log10_significant()) + 2),
                       differ.ndp_non_trivial)
                << "\033[0m" << std::endl;
    } else {
      std::cout << "\033[1;32m   Max diff is less than the "
                   "significant threshold: "
                << format_number(
                       thresh.significant, differ.ndp_non_trivial,
                       static_cast<int>(
                           std::round(thresh.get_log10_significant()) + 2),
                       differ.ndp_non_trivial)
                << "\033[0m" << std::endl;
    }
  }

  printbar(1);
}

void FileComparator::print_significant_summary(
    const SummaryParams& params) const {
  // Skip if structures are incompatible
  if (!flag.structures_compatible) {
    return;
  }

  // User-defined threshold differences
  // =========================================================
  if (counter.diff_significant == 0) {
    std::cout << "\033[1;32mFiles " << params.file1 << " and " << params.file2
              << " are equivalent within tolerance\033[0m" << std::endl;
    return;
  }

  if (print.level < 0) {
    return;  // Early return for low print levels
  }

  print_significant_differences_count(params);
  print_insignificant_differences_count(params);

  // Print detailed breakdown of significant differences
  if (counter.diff_significant > 0 && print.level > 0) {
    size_t non_marginal_non_critical = counter.diff_significant -
                                       counter.diff_marginal -
                                       counter.diff_critical;
    if (counter.diff_marginal > 0) {
      print_count_with_percent(params, "Marginal differences",
                               counter.diff_marginal, "\033[1;33m");
    }
    if (counter.diff_critical > 0) {
      print_count_with_percent(params, "Critical differences",
                               counter.diff_critical, "\033[1;31m");
    }
    if (non_marginal_non_critical > 0) {
      print_count_with_percent(params, "Non-marginal, non-critical significant",
                               non_marginal_non_critical, "\033[1;36m");
    }
  }

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
    print_count_with_percent(params,
                             "\"Close enough\" matches (<=" +
                                 std::to_string(thresh.significant) + ")",
                             counter.elem_number - counter.diff_significant);
  }
}

void FileComparator::print_significant_percentage() const {
  double percent = 100.0 * static_cast<double>(counter.diff_significant) /
                   static_cast<double>(counter.elem_number);
  std::cout << " (" << std::fixed << std::setw(5) << std::setprecision(2)
            << percent << "%)" << std::endl;

  // Calculate non-marginal, non-critical, significant differences
  // These are the differences of real interest that cannot be attributed to:
  // - Model failure (critical)
  // - Being outside operational interest (marginal)
  // - Machine precision errors (insignificant)
  size_t non_marginal_non_critical_significant =
      counter.diff_significant - counter.diff_marginal - counter.diff_critical;

  double critical_percent =
      100.0 * static_cast<double>(non_marginal_non_critical_significant) /
      static_cast<double>(counter.elem_number);

  // Check the 2% failure threshold for non-marginal, non-critical, significant
  // differences
  constexpr double failure_threshold_percent = 2.0;
  if (critical_percent > failure_threshold_percent) {
    std::cout << "   \033[1;31mFAIL: Non-marginal, non-critical significant "
                 "differences ("
              << non_marginal_non_critical_significant << ", " << std::fixed
              << std::setprecision(2) << critical_percent << "%) exceed "
              << failure_threshold_percent << "% threshold\033[0m" << std::endl;
    flag.files_are_close_enough = false;
    flag.error_found = true;
  } else if (critical_percent > 0) {
    std::cout << "   \033[1;33mPASS: Non-marginal, non-critical significant "
                 "differences ("
              << non_marginal_non_critical_significant << ", " << std::fixed
              << std::setprecision(2) << critical_percent << "%) within "
              << failure_threshold_percent << "% tolerance\033[0m" << std::endl;
    flag.files_are_close_enough = true;
  } else {
    std::cout << "   \033[1;32mPASS: No non-marginal, non-critical significant "
                 "differences found\033[0m"
              << std::endl;
    flag.files_are_close_enough = true;
  }
}

void FileComparator::print_insignificant_differences_count(
    const SummaryParams& params) const {
  if (counter.diff_non_trivial <= counter.diff_significant) {
    return;
  }

  size_t insignificant_count =
      counter.diff_non_trivial - counter.diff_significant;
  if (insignificant_count > 0) {
    print_count_with_percent(params,
                             "Insignificant differences (<=" +
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
  std::cout
      << "\033[1;31m   Max diff is greater than the significant threshold: "
      << std::setprecision(differ.ndp_significant) << thresh.significant
      << "\033[0m" << std::endl;
}

void FileComparator::print_max_diff_threshold_comparison_below() const {
  std::cout << "\033[1;32m   Max diff is less than or equal to the "
               "significant threshold: "
            << format_number(
                   thresh.significant, differ.ndp_significant,
                   static_cast<int>(std::round(thresh.get_log10_significant()) +
                                    2),
                   differ.ndp_significant)
            << "\033[0m" << std::endl;
}

void FileComparator::print_file_comparison_result(
    const SummaryParams& params) const {
  std::cout << "   ";
  if (differ.ndp_significant > differ.ndp_single_precision) {
    std::cout << "\033[1;33mFiles " << params.file1 << " and " << params.file2
              << " are equivalent within the limits of single precision\033[0m"
              << std::endl;
  } else {
    std::cout << "\033[1;31mFiles " << params.file1 << " and " << params.file2
              << " are significantly different\033[0m" << std::endl;
  }
}

void FileComparator::print_significant_differences_printing_status(
    const SummaryParams& params) const {
  if (counter.diff_print < counter.diff_significant) {
    print_count_with_percent(
        params,
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
    std::cout << " (" << std::fixed << std::setprecision(2) << percent << "%)";
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
  if (size_t length1 = file_reader_->get_file_length(file1);
      length1 == counter.line_number) {
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
  std::cout << "   diff_trivial     : ";
  if (counter.diff_trivial == 0) {
    std::cout << "\033[1;32m";
  }
  std::cout << std::setw(width) << counter.diff_trivial << "\033[0m"
            << std::endl;
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
  // Skip if structures are incompatible
  if (!flag.structures_compatible) {
    return;
  }

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
  int max_dp = std::max(dp1, dp2);

  return {val1, val2, rangeValue, dp1, dp2, min_dp, max_dp};
}

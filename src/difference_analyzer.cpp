/**
 * @author J. Lighthall
 * @date July 2025
 * Refactored from src/uband_diff.cpp
 */

#include "difference_analyzer.h"

#include <cmath>
#include <iomanip>
#include <iostream>

#include "uband_diff.h"  // For data structures

DifferenceAnalyzer::DifferenceAnalyzer(const Thresholds& thresholds)
    : thresh(thresholds) {}

bool DifferenceAnalyzer::process_difference(
    const ColumnValues& column_data, size_t column_index, double threshold,
    CountStats& counter, DiffStats& differ, Flags& flags) const {
  // Calculate rounded values and process difference
  double rounded1 = round_to_decimals(column_data.value1, column_data.min_dp);
  double rounded2 = round_to_decimals(column_data.value2, column_data.min_dp);
  // compare values (with rounding)
  double diff_rounded = std::abs(rounded1 - rounded2);

  process_rounded_values(column_data, column_index, diff_rounded,
                         column_data.min_dp, threshold, counter, differ, flags);

  counter.elem_number++;

  // Check critical threshold
  if ((diff_rounded > thresh.critical) &&
      ((rounded1 <= thresh.ignore) && (rounded2 <= thresh.ignore))) {
    counter.diff_critical++;
    flags.has_critical_diff = true;
    print_hard_threshold_error(rounded1, rounded2, diff_rounded, column_index,
                               counter);
    return false;
  }
  return true;
}

void DifferenceAnalyzer::process_raw_values(const ColumnValues& column_data,
                                            CountStats& counter,
                                            DiffStats& differ,
                                            Flags& flags) const {
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
    flags.has_non_zero_diff = true;
    flags.files_are_same = false;
  }
}

void DifferenceAnalyzer::process_rounded_values(
    const ColumnValues& column_data, size_t column_index, double rounded_diff,
    int minimum_deci, double threshold, CountStats& counter, DiffStats& differ,
    Flags& flags) const {
  // Define the threshold for non-trivial differences
  //
  // The smallest non-zero difference between values with N decimal
  // places is 10^(-N). A difference less than that value is trivial and
  // effectively zero. A difference greater than that value is not just due to
  // rounding errors or numerical precision issues. This is a heuristic to avoid
  // counting trivial differences as significant.

  // set to half the threshold to avoid counting trivial differences
  if (double big_zero = std::pow(10, -minimum_deci) / 2;
      rounded_diff > big_zero) {
    counter.diff_non_trivial++;
    flags.has_non_trivial_diff = true;
    flags.files_have_same_values = false;

    // track the maximum non trivial difference
    if (rounded_diff > differ.max_non_trivial) {
      differ.max_non_trivial = rounded_diff;
      differ.ndp_non_trivial = column_data.min_dp;
    }
  }

  if (rounded_diff > threshold && column_data.value1 < thresh.ignore &&
      column_data.value2 < thresh.ignore) {
    counter.diff_significant++;
    flags.has_significant_diff = true;
    flags.files_are_close_enough = false;

    if (column_data.value1 < thresh.marginal &&
        column_data.value2 < thresh.marginal) {
      counter.diff_marginal++;
      flags.has_marginal_diff = true;
    }

    // track the maximum significant difference
    if (rounded_diff > differ.max_significant) {
      differ.max_significant = rounded_diff;
      differ.ndp_significant = column_data.min_dp;
    }
  }
}

double DifferenceAnalyzer::round_to_decimals(double value, int precision) {
  double scale = std::pow(10.0, precision);
  return std::round(value * scale) / scale;
}

void DifferenceAnalyzer::print_hard_threshold_error(
    double rounded1, double rounded2, double diff_rounded, size_t column_index,
    const CountStats& counter) const {
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

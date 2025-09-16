/**
 * @author J. Lighthall
 * @date July 2025
 * Refactored from src/uband_diff.cpp
 */

#include "difference_analyzer.h"

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>

#include "uband_diff.h"  // For data structures

DifferenceAnalyzer::DifferenceAnalyzer(const Thresholds& thresholds)
    : thresh(thresholds) {}

bool DifferenceAnalyzer::process_difference(
    const ColumnValues& column_data, size_t column_index, double threshold,
    CountStats& counter, DiffStats& differ, Flags& flags) const {
  // Write to file for debugging
  std::ofstream debug_file("/tmp/debug_diff.txt", std::ios::app);
  debug_file << "PROCESS_DIFFERENCE: v1=" << column_data.value1
             << " v2=" << column_data.value2 << " threshold=" << threshold
             << std::endl;
  debug_file.close();
  // Calculate rounded values and process difference
  double rounded1 = round_to_decimals(column_data.value1, column_data.min_dp);
  double rounded2 = round_to_decimals(column_data.value2, column_data.min_dp);
  // compare values (with rounding)
  double diff_rounded = std::abs(rounded1 - rounded2);

  process_rounded_values(column_data, column_index, diff_rounded,
                         column_data.min_dp, threshold, counter, differ, flags);

  counter.elem_number++;

  // Check critical threshold - only count if both values correspond to
  // meaningful pressure values (TL <= ignore threshold)
  // TL values > ignore threshold correspond to pressures < single precision
  // limit
  if ((diff_rounded > thresh.critical) &&
      ((rounded1 <= thresh.ignore) && (rounded2 <= thresh.ignore))) {
    // NOTE: Critical differences are counted in the hierarchy logic, not here
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
  // 6-LEVEL HIERARCHY IMPLEMENTATION
  // Each level divides one group into two, with one group being further
  // subdivided

  // LEVEL 2: non_zero = trivial + non_trivial (based on printed precision)
  double big_zero = std::pow(10, -minimum_deci) / 2;

  if (rounded_diff > big_zero) {
    // This is a NON-TRIVIAL difference
    counter.diff_non_trivial++;
    flags.has_non_trivial_diff = true;
    flags.files_have_same_values = false;

    // track the maximum non trivial difference
    if (rounded_diff > differ.max_non_trivial) {
      differ.max_non_trivial = rounded_diff;
      differ.ndp_non_trivial = column_data.min_dp;
    }

    // LEVEL 3: non_trivial = insignificant + significant (based on ignore
    // threshold ~138)
    if (column_data.value1 > thresh.ignore &&
        column_data.value2 > thresh.ignore) {
      // Both values > ignore threshold - this is an INSIGNIFICANT difference
      counter.diff_insignificant++;
    } else {
      // At least one value <= ignore threshold - this is a SIGNIFICANT
      // difference
      counter.diff_significant++;
      flags.has_significant_diff = true;
      flags.files_are_close_enough = false;

      // LEVEL 4: significant = marginal + non_marginal (based on marginal
      // threshold 110)
      if (column_data.value1 > thresh.marginal &&
          column_data.value1 < thresh.ignore &&
          column_data.value2 > thresh.marginal &&
          column_data.value2 < thresh.ignore) {
        // Both values in marginal range (110 < TL < 138) - this is a MARGINAL
        // difference Marginal differences are a separate category and not
        // further subdivided
        counter.diff_marginal++;
        flags.has_marginal_diff = true;
      } else {
        // This is a NON-MARGINAL significant difference

        // LEVEL 5: non_marginal = critical + non_critical (based on critical
        // threshold)
        if (rounded_diff > thresh.critical) {
          // This is a CRITICAL difference
          counter.diff_critical++;
          flags.has_critical_diff = true;
          std::cout << "HIERARCHY DEBUG: CRITICAL diff=" << rounded_diff
                    << " (total critical count now: " << counter.diff_critical
                    << ")" << std::endl;
        } else {
          // This is a NON-CRITICAL difference (within non-marginal category)

          // LEVEL 6: non_critical = error + non_error (based on user threshold)
          if (rounded_diff > thresh.significant) {
            // This is an ERROR difference (exceeds user threshold)
            counter.diff_error++;
            flags.has_error_diff = true;
          } else {
            // This is a NON-ERROR difference (within user threshold)
            counter.diff_non_error++;
            flags.has_non_error_diff = true;
          }
        }
      }

      // track the maximum significant difference
      if (rounded_diff > differ.max_significant) {
        differ.max_significant = rounded_diff;
        differ.ndp_significant = column_data.min_dp;
      }
    }
  } else if (rounded_diff > thresh.zero) {
    // This is a TRIVIAL difference - non-zero but within format precision
    counter.diff_trivial++;
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

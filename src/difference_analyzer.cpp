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

  // Check critical threshold - only annotate early if both values correspond
  // to meaningful pressure values (TL <= ignore threshold). We DO NOT abort
  // processing; instead we set flags and let the hierarchy logic count it.
  if ((diff_rounded > thresh.critical) && (rounded1 <= thresh.ignore) &&
      (rounded2 <= thresh.ignore)) {
    if (!flags.has_critical_diff) {
      // First critical difference encountered: print a concise notification
      print_hard_threshold_error(rounded1, rounded2, diff_rounded, column_index,
                                 counter);
    }
    flags.has_critical_diff = true;
    flags.error_found = true;  // ensure program exits non-zero
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
  // A raw non-zero difference is TRIVIAL if, after rounding to the minimum
  // printed precision, the two values are identical OR the rounded difference
  // is within half an LSB (big_zero). Otherwise it is NON-TRIVIAL.
  double raw_diff = std::abs(column_data.value1 - column_data.value2);
  double lsb = std::pow(10, -minimum_deci);    // one unit in last place
  double big_zero = lsb / 2.0;                 // half-ulp criterion
  bool raw_non_zero = raw_diff > thresh.zero;  // raw difference observed
  bool trivial_after_rounding = (rounded_diff <= big_zero);

  if (raw_non_zero) {
    if (trivial_after_rounding) {
      // TRIVIAL: raw difference exists but formatting precision hides it
      counter.diff_trivial++;
    } else {
      // NON-TRIVIAL difference
      counter.diff_non_trivial++;
      flags.has_non_trivial_diff = true;
      flags.files_have_same_values = false;

      // track maximum non-trivial difference
      if (rounded_diff > differ.max_non_trivial) {
        differ.max_non_trivial = rounded_diff;
        differ.ndp_non_trivial = column_data.min_dp;
      }

      // LEVEL 3: non_trivial = insignificant + significant (ignore threshold)
      if (column_data.value1 > thresh.ignore &&
          column_data.value2 > thresh.ignore) {
        // Both values above ignore threshold => INSIGNIFICANT
        counter.diff_insignificant++;
      } else {
        // SIGNIFICANT difference (at least one value meaningful)
        counter.diff_significant++;
        flags.has_significant_diff = true;
        flags.files_are_close_enough = false;

        // LEVEL 4: significant = marginal + non_marginal
        if (column_data.value1 > thresh.marginal &&
            column_data.value1 < thresh.ignore &&
            column_data.value2 > thresh.marginal &&
            column_data.value2 < thresh.ignore) {
          // MARGINAL (both in marginal band)
          counter.diff_marginal++;
          flags.has_marginal_diff = true;
        } else {
          // NON-MARGINAL
          // LEVEL 5: non_marginal = critical + non_critical
          if ((rounded_diff > thresh.critical) &&
              (column_data.value1 <= thresh.ignore) &&
              (column_data.value2 <= thresh.ignore)) {
            // CRITICAL
            counter.diff_critical++;
            flags.has_critical_diff = true;
            flags.error_found = true;  // ensure failure exit code
          } else {
            // NON-CRITICAL
            // LEVEL 6: non_critical = error + non_error (user threshold)
            if (rounded_diff > thresh.significant) {
              counter.diff_error++;
              flags.has_error_diff = true;
            } else {
              counter.diff_non_error++;
              flags.has_non_error_diff = true;
            }
          }
        }
        // track maximum significant difference
        if (rounded_diff > differ.max_significant) {
          differ.max_significant = rounded_diff;
          differ.ndp_significant = column_data.min_dp;
        }
      }
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

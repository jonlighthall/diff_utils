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
#include <limits>

#include "uband_diff.h"  // For data structures

DifferenceAnalyzer::DifferenceAnalyzer(const Thresholds& thresholds)
    : thresh(thresholds) {}

bool DifferenceAnalyzer::process_difference(
    const ColumnValues& column_data, size_t column_index, double threshold,
    CountStats& counter, DiffStats& differ, Flags& flags) const {
  // Calculate raw difference (used for Levels 2-6)
  double raw_diff = std::abs(column_data.value1 - column_data.value2);

  // Process through the 6-level hierarchy
  // NOTE: We pass raw_diff, not rounded_diff, for consistency.
  // Level 2 will calculate rounded values internally to determine triviality,
  // but Levels 3-6 use the actual raw difference for magnitude comparisons.
  process_hierarchy(column_data, column_index, raw_diff, column_data.min_dp,
                    threshold, counter, differ, flags);

  counter.elem_number++;

  // Early critical difference check (for fail-fast reporting)
  // Use raw_diff consistently with the hierarchy logic
  // Skip TL threshold checks for column 0 if it's range data.
  bool skip_tl_check = (column_index == 0 && flags.column1_is_range_data);
  if ((raw_diff > thresh.critical) && (column_data.value1 <= thresh.ignore) &&
      (column_data.value2 <= thresh.ignore) && !skip_tl_check) {
    if (!flags.has_critical_diff) {
      // First critical difference encountered: print a concise notification
      print_hard_threshold_error(column_data.value1, column_data.value2,
                                 raw_diff, column_index, counter);
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

void DifferenceAnalyzer::process_hierarchy(const ColumnValues& column_data,
                                           size_t column_index, double raw_diff,
                                           int minimum_deci, double threshold,
                                           CountStats& counter,
                                           DiffStats& differ,
                                           Flags& flags) const {
  // 6-LEVEL HIERARCHY IMPLEMENTATION
  // Each level divides one group into two, with one group being further
  // subdivided
  //
  // IMPORTANT: After Level 2, we use raw_diff (not rounded_diff) for all
  // magnitude comparisons. Level 2's job is to filter out differences that
  // are indistinguishable at the printed precision. Once a difference is
  // non-trivial, subsequent levels should compare against the actual
  // magnitude, not the inflated rounded difference.

  // LEVEL 2: non_zero = trivial + non_trivial (based on printed precision)
  // A raw non-zero difference is TRIVIAL if, after rounding to the minimum
  // printed precision, the two values are identical OR the raw difference
  // is within half an LSB (big_zero). Otherwise it is NON-TRIVIAL.
  //
  // SUB-LSB DETECTION:
  // The key insight is that differences smaller than the minimum representable
  // difference at the coarser precision (LSB) are indistinguishable and should
  // be treated as equivalent for cross-platform robustness.
  //
  // Example: 30.8 (1dp) vs 30.85 (2dp)
  //   - LSB = 0.1 (minimum step at 1 decimal place)
  //   - raw_diff = 0.05 <= LSB/2 = 0.05
  //   - This is a sub-LSB difference, classified as TRIVIAL
  //
  // We calculate rounded_diff only to check if values round to the same value,
  // but use raw_diff for the sub-LSB test and all subsequent comparisons.

  double lsb = std::pow(10, -minimum_deci);    // one unit in last place
  double big_zero = lsb / 2.0;                 // half-ulp criterion
  bool raw_non_zero = raw_diff > thresh.zero;  // raw difference observed

  // Calculate rounded values to check if they're identical
  double rounded1 = round_to_decimals(column_data.value1, minimum_deci);
  double rounded2 = round_to_decimals(column_data.value2, minimum_deci);
  double rounded_diff = std::abs(rounded1 - rounded2);

  // FLOATING POINT ROBUSTNESS: Use epsilon tolerance for sub-LSB comparison
  // to handle cases like 30.8 vs 30.85 where floating point arithmetic may
  // result in raw_diff slightly exceeding big_zero due to representation error
  constexpr double FP_TOLERANCE =
      1e-12;  // relative tolerance for FP comparison
  bool sub_lsb_diff =
      (raw_diff < big_zero) || (std::abs(raw_diff - big_zero) <
                                FP_TOLERANCE * std::max(raw_diff, big_zero));
  bool trivial_after_rounding = (rounded_diff == 0.0 || sub_lsb_diff);

  if (raw_non_zero) {
    if (trivial_after_rounding) {
      // TRIVIAL: raw difference exists but formatting precision hides it
      counter.diff_trivial++;
    } else {
      // NON-TRIVIAL difference
      counter.diff_non_trivial++;
      flags.has_non_trivial_diff = true;
      flags.files_have_same_values = false;

      // Track maximum non-trivial difference using raw_diff (actual magnitude)
      if (raw_diff > differ.max_non_trivial) {
        differ.max_non_trivial = raw_diff;
        differ.ndp_non_trivial = column_data.min_dp;
      }

      // Track percentage error for this non-trivial difference using raw_diff.
      // Use the second file as reference (column_data.value2).
      // If reference is effectively zero, set percent to +infinity as a
      // sentinel (do not update max_percent_error with inf).
      double ref = std::abs(column_data.value2);
      double pct = 0.0;
      if (ref > thresh.zero) {
        pct = 100.0 * raw_diff / ref;
        if (pct > differ.max_percent_error) {
          differ.max_percent_error = pct;
        }
      } else {
        // Reference effectively zero: set pct to a sentinel large value for
        // printing. Do not store INF in max_percent_error since it's not
        // meaningful numerically.
        pct = std::numeric_limits<double>::infinity();
      }

      // LEVEL 3: non_trivial = insignificant + significant
      // A non-trivial difference is considered INSIGNIFICANT if EITHER:
      //   (a) both TL values are above the ignore threshold (numerically
      //       meaningless), OR
      //   (b) the raw magnitude of the difference does NOT exceed the
      //       significance threshold
      // Otherwise it is SIGNIFICANT.
      //
      // NOTE: We now use raw_diff for threshold comparisons, not rounded_diff.
      // This provides consistent, intuitive behavior: threshold=0.1 means
      // actual difference of 0.1, not "rounds to 0.1 difference at precision p"
      //
      // Skip TL ignore threshold check for column 0 if it's range data.
      bool skip_tl_check = (column_index == 0 && flags.column1_is_range_data);
      bool both_above_ignore =
          !skip_tl_check && (column_data.value1 > thresh.ignore &&
                             column_data.value2 > thresh.ignore);

      // Determine significance exceed condition
      bool exceeds_significance = false;
      if (thresh.significant_is_percent) {
        // Percent-mode: compare raw_diff relative to the reference value
        // taken from the second file (column_data.value2). significant_percent
        // is stored as a fraction (e.g. 0.01 for 1%). If the reference is
        // effectively zero, any non-trivial difference counts as exceeding
        // the percent threshold (since relative percent is undefined).
        double ref = std::abs(column_data.value2);
        if (ref <= thresh.zero) {
          // Reference effectively zero -> treat any non-trivial difference as
          // exceeding the percent cutoff.
          exceeds_significance = (raw_diff > thresh.zero);
        } else {
          double frac = raw_diff / ref;  // fractional difference
          exceeds_significance = (frac > thresh.significant_percent);
        }
      } else if (thresh.significant == 0.0) {
        // User wants maximum sensitivity: treat all non-trivial differences
        // (below ignore) as significant.
        exceeds_significance = true;
      } else {
        // Standard behavior: compare raw_diff against threshold
        // (Keep '>' not '>=' to preserve original semantics)
        exceeds_significance = (raw_diff > threshold);
      }

      if (both_above_ignore || !exceeds_significance) {
        // INSIGNIFICANT
        counter.diff_insignificant++;
        if (both_above_ignore) {
          counter.diff_high_ignore++;  // track cause for semantic checks
        }
      } else {
        // SIGNIFICANT (difference large enough AND at least one value
        // meaningful)
        counter.diff_significant++;
        flags.has_significant_diff = true;
        flags.files_are_close_enough = false;

        // LEVEL 4: significant = marginal + non_marginal (based on TL range)
        // Skip TL marginal threshold check for column 0 if it's range data.
        bool skip_tl_check = (column_index == 0 && flags.column1_is_range_data);
        if (!skip_tl_check && column_data.value1 > thresh.marginal &&
            column_data.value1 < thresh.ignore &&
            column_data.value2 > thresh.marginal &&
            column_data.value2 < thresh.ignore) {
          // MARGINAL band (warning range)
          counter.diff_marginal++;
          flags.has_marginal_diff = true;
        } else {
          // NON-MARGINAL
          // LEVEL 5: non_marginal = critical + non_critical
          // Skip TL ignore threshold check for column 0 if it's range data.
          // Use raw_diff for critical threshold comparison
          if (!skip_tl_check && (raw_diff > thresh.critical) &&
              (column_data.value1 <= thresh.ignore) &&
              (column_data.value2 <= thresh.ignore)) {
            // CRITICAL
            counter.diff_critical++;
            flags.has_critical_diff = true;
            flags.error_found = true;  // ensure failure exit code
          } else {
            // NON-CRITICAL
            // LEVEL 6: non_critical = error + non_error (user threshold split)
            // Honor percent-mode if enabled. Use raw_diff for all comparisons.
            if (thresh.significant_is_percent) {
              double ref = std::abs(column_data.value2);
              bool percent_exceeds = false;
              if (ref <= thresh.zero) {
                percent_exceeds = (raw_diff > thresh.zero);
              } else {
                percent_exceeds =
                    ((raw_diff / ref) > thresh.significant_percent);
              }
              if (percent_exceeds) {
                counter.diff_error++;
                flags.has_error_diff = true;
              } else {
                counter.diff_non_error++;
                flags.has_non_error_diff = true;
              }
            } else {
              if (raw_diff > thresh.significant) {
                counter.diff_error++;
                flags.has_error_diff = true;
              } else {
                counter.diff_non_error++;
                flags.has_non_error_diff = true;
              }
            }
          }
        }
        // Track maximum significant difference using raw_diff
        if (raw_diff > differ.max_significant) {
          differ.max_significant = raw_diff;
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

  std::cout << counter.diff_print << " with differences between ";
  if (thresh.significant_is_percent) {
    std::cout << (thresh.significant_percent * 100.0) << "% and ";
  } else {
    std::cout << thresh.significant << " and ";
  }
  std::cout << thresh.critical << std::endl;

  std::cout << "   File1: " << std::setw(7) << rounded1 << std::endl;
  std::cout << "   File2: " << std::setw(7) << rounded2 << std::endl;
  std::cout << "    diff: \033[1;31m" << std::setw(7) << diff_rounded
            << "\033[0m" << std::endl;
}

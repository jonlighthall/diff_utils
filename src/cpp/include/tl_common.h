/**
 * @file tl_common.h
 * @brief Shared data structures and component includes for the tl_diff suite
 *
 * This header provides the common foundation used by tl_diff, tl_metric, and
 * tl_analysis. It contains threshold definitions, counters, flags, and
 * includes all shared component headers.
 *
 * @author J. Lighthall
 * @date February 2026
 * Extracted from tl_diff.h as part of three-program restructuring
 */

#ifndef TL_COMMON_H
#define TL_COMMON_H
#include <cmath>
#include <fstream>
#include <limits>
#include <memory>
#include <sstream>
#include <string>
#include <tuple>
#include <vector>

#include "difference_analyzer.h"
#include "file_reader.h"
#include "format_tracker.h"
#include "line_parser.h"
#include "print_level.h"

// Data structures

struct Thresholds {
  // user-defined thresholds
  // -------------------------------------------------------
  double significant;  // lower threshold for significant difference (fail)
  double critical;     // threshold for critical difference (exit)
  double print;        // threshold for printing entry in table (print)

  // fixed thresholds
  // -------------------------------------------------------

  // Single precision epsilon - calculate once to avoid duplicate computations
  static constexpr double SINGLE_PRECISION_EPSILON =
      std::pow(2.0, -23);  // ~1.19e-07

  // define epsilon when threshold is zero
  const double zero = SINGLE_PRECISION_EPSILON;

  // Define the maximum significant value of TL (Transmission Loss). This will
  // be referred to as the marginal threshold. It is equal to the upper upper
  // threshold given in the following reference. TL values above this threshold
  // are considered insignificant.
  //
  //   https://doi.org/10.23919/OCEANS.2009.5422312
  const double marginal =
      110;  // upper threshold for significant difference (warning)

  // Define the maximum numerically valid value for TL. The value given
  // below is based on the smallest value that can be represented in single
  // precision floating point format, corresponding to the smallest pressure
  // magnitude that can be represented.
  //
  // Values below this threshold are considered numerically "meaningless" and
  // will not trigger any action in the comparison.
  const double ignore =
      -20 * log10(SINGLE_PRECISION_EPSILON);  // threshold for meaningless
                                              // difference (no action)

  // Cached calculations for performance optimization
  mutable double cached_log10_significant = 0.0;
  mutable bool log10_significant_cached = false;

  // Percent-mode support: when true, interpret 'significant_percent' as a
  // fractional value (e.g. 0.01 for 1%). This overrides the usual
  // absolute 'significant' comparison for deciding significance.
  bool significant_is_percent = false;
  double significant_percent = 0.0;  // fractional (0.01 == 1%)

  // Helper method to get log10(significant) with caching
  double get_log10_significant() const {
    if (!log10_significant_cached || significant <= 0) {
      if (significant > 0) {
        cached_log10_significant = std::log10(significant);
        log10_significant_cached = true;
      } else {
        return 0.0;  // Handle edge case for zero/negative thresholds
      }
    }
    return cached_log10_significant;
  }

  // Method to invalidate cache when significant threshold changes
  void update_significant(double new_significant) {
    if (significant != new_significant) {
      significant = new_significant;
      log10_significant_cached = false;
    }
  }
};
struct CountStats {
  // number of...
  size_t line_number = 0;  // lines read
  size_t elem_number = 0;  // elements checked

  // non-zero differences found (independent of arguments)
  size_t diff_non_zero = 0;     // based on value and format (strict)
  size_t diff_trivial = 0;      // non-zero but within format precision
  size_t diff_non_trivial = 0;  // based on value only (format independent)

  // differences found greater than user defined...
  size_t diff_significant = 0;  // nominal threshold ("good enough")
  size_t diff_insignificant =
      0;                     // non-trivial but both values > ignore threshold
  size_t diff_marginal = 0;  // marginal threshold (pass and warn)
  size_t diff_critical = 0;  // critical threshold (fail and exit)

  // LEVEL 6 counters: non_critical = error + non_error (based on user
  // threshold). These are only populated for non-marginal, non-critical,
  // significant differences.
  size_t diff_error = 0;      // differences > user threshold (argument 3)
  size_t diff_non_error = 0;  // differences <= user threshold (argument 3)

  size_t diff_print = 0;  // print threshold (for difference table)
  // Count of non-trivial differences where BOTH values exceed ignore threshold
  size_t diff_high_ignore =
      0;  // aids semantic consistency checks for zero threshold
};
struct Flags {
  bool new_fmt = false;
  bool file_end_reached = false;  // Indicates if the end of file was reached
  bool error_found =
      false;  // Global error flag (file access or parse errors only)
  bool file_access_error = false;     // Specific flag for file access errors
  bool structures_compatible = true;  // Files have compatible column structures

  // Counter-associated flags (correspond to CountStats)
  bool has_non_zero_diff =
      false;  // Any non-zero difference found (format-dependent, like diff)
  bool has_non_trivial_diff = false;  // Difference exceeds format precision
                                      // threshold (format-independent)
  bool has_significant_diff =
      false;                       // Difference exceeds user-defined threshold
  bool has_marginal_diff = false;  // Difference exceeds marginal threshold
  bool has_critical_diff = false;  // Difference exceeds critical/hard threshold

  // LEVEL 5 flags: non_critical differences subdivision
  bool has_error_diff = false;      // Difference exceeds user threshold
  bool has_non_error_diff = false;  // Difference within user threshold

  bool has_printed_diff = false;  // Difference exceeds print threshold

  // Unit mismatch detection: if column 1 appears scaled by a factor
  // approximately equal to one nautical mile in meters (1852), set true.
  bool unit_mismatch = false;
  size_t unit_mismatch_line = 0;
  double unit_mismatch_ratio = 0.0;

  // Range data detection: if column 1 is monotonically increasing with fixed
  // delta, it's likely range data and TL thresholds don't apply
  bool column1_is_range_data = false;

  // Overall comparison state flags
  bool files_are_same = true;  // Files are identical
  bool files_have_same_values =
      true;  // Files have same values within precision
  bool files_are_close_enough =
      true;  // Files are same within user-defined threshold
};

struct DiffStats {
  // track the maximum difference found
  double max_non_zero = 0;
  double max_non_trivial = 0;
  double max_significant = 0;  // maximum significant difference

  // maximum percentage error observed for non-trivial differences (%),
  // calculated as 100 * |v1 - v2| / |v2| when |v2| > thresh.zero. If the
  // reference is effectively zero, this value may remain 0 (or be set to
  // a large sentinel by the analyzer).
  double max_percent_error = 0.0;

  int ndp_non_zero = 0;     // number of decimal places for non-zero values
  int ndp_non_trivial = 0;  // number of decimal places for non-trivial values
  int ndp_significant = 0;  // number of decimal places
  int ndp_max = 0;          // number of decimal places for maximum difference
  const int ndp_single_precision =
      7;  // number of decimal places for single precision

  // Full-field RMSE accumulators (TL columns only, skip range column)
  double sum_sq_diff = 0.0;  // sum of (v1 - v2)^2 for all TL elements
  size_t n_tl_elements = 0;  // count of TL elements compared
};

// NOTE: RMSEStats, TLMetrics, and ErrorAccumulationData have been moved to
// tl_metric and tl_analysis respectively. tl_diff is a point-by-point
// comparator and does not perform aggregate/curve-level analysis.

struct LineData {
  std::vector<double> values;
  std::vector<int> decimal_places;
};

struct ColumnValues {
  double value1;  // Value from first file at current column
  double value2;  // Value from second file at current column
  double range;   // First value in the line (used as range indicator)
  int dp1;        // Decimal places for file1 value
  int dp2;        // Decimal places for file2 value
  int min_dp;     // Minimum decimal places (for rounding)
  int max_dp;     // Maximum decimal places (for more precise output)
};

struct SummaryParams {
  std::string file1;  // Path to first file
  std::string file2;  // Path to second file
  int fmt_wid;        // Formatting width for output alignment
};

// PrintLevel definition moved to print_level.h

// Forward declarations for utility functions
std::tuple<double, double, int, int> readComplex(std::istringstream& stream,
                                                 Flags& flag);

#endif  // TL_COMMON_H

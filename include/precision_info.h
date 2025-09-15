/**
 * @author J. Lighthall
 * @date July 2025
 * Precision analysis structures and functions for numerical comparisons
 */

#ifndef PRECISION_INFO_H
#define PRECISION_INFO_H

#include <sstream>
#include <string>

/**
 * @brief Structure to hold precision analysis information for numerical values
 *
 * This structure supports both fixed-point and scientific notation analysis,
 * providing comprehensive precision metrics for numerical comparison.
 */
struct PrecisionInfo {
  int decimal_places = 0;  // For fixed notation: digits after decimal point
  int significant_figures =
      0;  // For scientific notation: total significant digits
  bool is_scientific = false;  // True if number is in scientific notation
  int exponent = 0;            // For scientific notation: the exponent value
  double effective_precision = 0.0;  // The actual precision this represents
  double parsed_value =
      0.0;  // The actual numerical value parsed from the string
  bool has_single_precision_warning =
      false;  // True if likely single precision (6-7 sig figs)

  /**
   * @brief Calculate effective decimal places for comparison purposes
   *
   * For scientific notation, this converts significant figures to an equivalent
   * decimal place count for consistent comparison with fixed notation.
   *
   * @return Effective decimal places for comparison
   */
  int get_effective_decimal_places() const {
    if (is_scientific) {
      // For scientific notation, effective decimal places = significant_figures
      // - 1 - exponent e.g., 1.23e-5 has 3 sig figs, so 3-1-(-5) = 7 effective
      // decimal places For large positive exponents (e.g., 1.23e10), this can
      // be negative, which means the number is effectively an integer with no
      // decimal precision
      int effective_dp = significant_figures - 1 - exponent;

      // Clamp to reasonable bounds (0 to 10 decimal places)
      if (effective_dp < 0) {
        return 0;  // Large numbers like 1.23e10 have no decimal precision
      } else if (effective_dp > 10) {
        return 10;  // Cap at maximum reasonable decimal places
      }
      return effective_dp;
    }
    return decimal_places;
  }
};

// Function declarations
int count_significant_figures(const std::string& num_str);
PrecisionInfo stream_analyzePrecision(std::istringstream& stream);
int stream_countDecimalPlaces(std::istringstream& stream);  // Legacy function

#endif  // PRECISION_INFO_H
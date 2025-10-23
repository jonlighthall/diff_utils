/**
 * @author J. Lighthall
 * @date July 2025
 * Refactored from src/uband_diff.cpp
 */

#include "line_parser.h"

#include <iostream>
#include <sstream>

#include "uband_diff.h"  // For LineData and Flags structures

// ========================================================================
// Line Parsing Implementation
// ========================================================================

LineData LineParser::parse_line(const std::string& line, Flags& flags,
                                size_t line_number) const {
  LineData result;
  std::istringstream stream(line);
  char ch;

  while (stream >> ch) {
    // check if the numbers are complex and read them accordingly
    // check if string starts with '('
    if (ch == '(') {
      // read the complex number
      auto [real, imag, dp_real, dp_imag] = readComplex(stream, flags);
      result.values.push_back(real);
      result.values.push_back(imag);
      // validate decimal places for real and imaginary parts
      if (validate_decimal_places(dp_real, line_number, flags)) {
        result.decimal_places.push_back(dp_real);
      }
      if (validate_decimal_places(dp_imag, line_number, flags)) {
        result.decimal_places.push_back(dp_imag);
      }
    } else {
      // if the character is not '(', it is a number
      stream.putback(ch);  // put back the character to the stream
      int dp = stream_countDecimalPlaces(
          stream);  // count the number of decimal places

      // validate decimal places before storing
      if (validate_decimal_places(dp, line_number, flags)) {
        result.decimal_places.push_back(
            dp);  // store the number of decimal places
      }

      double value;
      stream >> value;
      result.values.push_back(value);
    }
  }
  return result;
}

bool LineParser::validate_decimal_places(int ndp, size_t line_number,
                                         Flags& flags) const {
  // Double precision (IEEE 754) can represent ~15-17 significant digits
  // For values around 1-10, this translates to ~15-16 decimal places
  // We allow up to 17 to be conservative with edge cases
  constexpr int MAX_DECIMAL_PLACES = 17;

  if (ndp < 0 || ndp > MAX_DECIMAL_PLACES) {
    std::cerr << "Invalid number of decimal places found on line "
              << line_number << ": " << ndp << ". Must be between 0 and "
              << MAX_DECIMAL_PLACES << "." << std::endl;
    flags.error_found = true;
    return false;
  }
  return true;
}

/**
 * @author J. Lighthall
 * @date July 2025
 * Refactored from src/uband_diff.cpp
 */

#ifndef LINE_PARSER_H
#define LINE_PARSER_H

#include <sstream>
#include <string>
#include <vector>

// Forward declarations
struct LineData;
struct Flags;

// External utility functions that LineParser uses
int stream_countDecimalPlaces(std::istringstream& stream);
std::tuple<double, double, int, int> readComplex(std::istringstream& stream,
                                                 Flags& flag);

/**
 * @brief Handles parsing of individual lines and extraction of numerical data
 *
 * This class is responsible for:
 * - Parsing lines containing real and complex numbers
 * - Extracting decimal place information
 * - Converting strings to numerical values
 * - Handling complex number format parsing
 */
class LineParser {
 public:
  LineParser() = default;
  ~LineParser() = default;

  // Main parsing interface
  LineData parse_line(const std::string& line, Flags& flags,
                      size_t line_number) const;

 private:
  // Helper functions for parsing
  bool validate_decimal_places(int ndp, size_t line_number, Flags& flags) const;
};

#endif  // LINE_PARSER_H
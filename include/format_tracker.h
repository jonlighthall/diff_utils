/**
 * @author J. Lighthall
 * @date July 2025
 * Refactored from src/uband_diff.cpp
 */

#ifndef FORMAT_TRACKER_H
#define FORMAT_TRACKER_H

#include <cstddef>
#include <vector>

#include "print_level.h"

// Forward declarations
struct Flags;
// PrintLevel now provided by print_level.h

/**
 * @brief Manages decimal place format tracking and validation
 *
 * This class is responsible for:
 * - Tracking decimal places per column
 * - Validating format consistency
 * - Managing format changes between lines
 * - Calculating appropriate thresholds based on precision
 */
class FormatTracker {
 public:
  explicit FormatTracker(const PrintLevel& print_settings);
  ~FormatTracker() = default;

  // Format validation and tracking
  bool validate_and_track_column_format(size_t n_col1, size_t n_col2,
                                        std::vector<int>& dp_per_col,
                                        size_t& prev_n_col, size_t line_number,
                                        Flags& flags);

  bool validate_decimal_column_size(const std::vector<int>& dp_per_col,
                                    size_t column_index,
                                    size_t line_number) const;

  // Decimal place management
  bool initialize_decimal_place_format(int min_dp, size_t column_index,
                                       std::vector<int>& dp_per_col,
                                       size_t line_number, Flags& flags);

  bool update_decimal_place_format(int min_dp, size_t column_index,
                                   std::vector<int>& dp_per_col,
                                   size_t line_number, Flags& flags);

  double calculate_threshold(int decimal_places,
                             double significant_threshold) const;

  // Getters for format information
  size_t get_format_line() const { return this_fmt_line; }
  size_t get_format_column() const { return this_fmt_column; }
  size_t get_line_columns() const { return this_line_ncols; }
  size_t get_last_format_line() const { return last_fmt_line; }

  // Setters for format tracking
  void set_format_line(size_t line) { this_fmt_line = line; }
  void set_format_column(size_t column) { this_fmt_column = column; }
  void set_line_columns(size_t cols) { this_line_ncols = cols; }
  void set_last_format_line(size_t line) { last_fmt_line = line; }

 private:
  // Store by value to avoid dangling reference to a temporary PrintLevel
  PrintLevel print;

  // Format tracking state
  size_t this_fmt_line = 0;
  size_t this_fmt_column = 0;
  size_t last_fmt_line = 0;
  size_t this_line_ncols = 0;
};

#endif  // FORMAT_TRACKER_H

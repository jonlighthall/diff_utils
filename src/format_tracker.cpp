/** 
 * @author J. Lighthall
 * @date July 2025
 * Refactored from src/uband_diff.cpp
 */

#include "format_tracker.h"
#include "uband_diff.h"  // For Flags and other struct definitions
#include <iostream>
#include <iomanip>
#include <cmath>

FormatTracker::FormatTracker(const PrintLevel& print_settings)
    : print(print_settings) {}

bool FormatTracker::validate_and_track_column_format(size_t n_col1, size_t n_col2,
                                                     std::vector<int>& dp_per_col,
                                                     size_t& prev_n_col,
                                                     size_t line_number,
                                                     Flags& flags) {
    // Check if both lines have the same number of columns
    if (n_col1 != n_col2) {
        std::cerr << "Line " << line_number
                  << " has different number of columns!" << std::endl;
        return false;
    }

    this_line_ncols = n_col1;

    // Check if the number of columns has changed
    if (line_number == 1) {
        prev_n_col = n_col1;  // initialize prev_n_col on first line
        if (print.debug2) {
            std::cout << "   FORMAT: " << n_col1
                      << " columns (both files) - initialized" << std::endl;
        }
    }

    if (prev_n_col > 0 && n_col1 != prev_n_col) {
        std::cout << "\033[1;33mNote: Number of columns changed at line "
                  << line_number << " (previous: " << prev_n_col
                  << ", current: " << n_col1 << ")\033[0m" << std::endl;
        dp_per_col.clear();
        flags.new_fmt = true;
        this_fmt_line = line_number;
        if (print.level > 0) {
            std::cout << this_fmt_line << ": FMT number of columns has changed"
                      << std::endl;
            std::cout << "format has changed" << std::endl;
        }
    } else {
        if (line_number > 1) {
            if (print.debug3) {
                std::cout << "Line " << line_number << " same column format"
                          << std::endl;
            }
            flags.new_fmt = false;
        }
    }
    prev_n_col = n_col1;
    return true;
}

bool FormatTracker::validate_decimal_column_size(const std::vector<int>& dp_per_col,
                                                 size_t column_index,
                                                 size_t line_number) const {
    if (print.debug3) {
        for (size_t j = 0; j < dp_per_col.size(); ++j) {
            std::cout << "   minimum decimal places in column " << j + 1 << " = "
                      << dp_per_col[j] << std::endl;
        }
    }

    if (print.debug2) {
        std::cout << "   size of dp_per_col: " << dp_per_col.size();
        std::cout << ", column_index: " << column_index + 1 << std::endl;
    }

    // Validate vector size
    if (dp_per_col.size() != column_index + 1) {
        std::cerr << "Warning: dp_per_col size mismatch at line "
                  << line_number << std::endl;
        std::cerr << "Expected size: " << column_index + 1
                  << ", Actual size: " << dp_per_col.size() << std::endl;
        std::cerr << "Please check the input files for consistency." << std::endl;
        return false;
    }

    if (dp_per_col.size() <= column_index) {
        std::cerr << "Warning: dp_per_col size (" << dp_per_col.size()
                  << ") insufficient for column " << column_index + 1 << " at line "
                  << line_number << std::endl;
        std::cerr << "Please check the input files for consistency." << std::endl;
        std::cerr << "Expected at least " << column_index + 1
                  << " columns, but got " << dp_per_col.size() << std::endl;
        return false;
    }
    return true;
}

bool FormatTracker::initialize_decimal_place_format(int min_dp, size_t column_index,
                                                    std::vector<int>& dp_per_col,
                                                    size_t line_number,
                                                    Flags& flags) {
    // initialize the dp_per_col vector with the minimum decimal places
    dp_per_col.push_back(min_dp);

    if (!validate_decimal_column_size(dp_per_col, column_index, line_number)) {
        flags.error_found = true;
        return false;
    }

    if (print.debug2) {
        std::cout << "FORMAT: Line " << line_number << " initialization" << std::endl;
        std::cout << "   dp_per_col: ";
        for (const auto& d : dp_per_col) {
            std::cout << d << " ";
        }
        std::cout << std::endl;
    }

    // since this is an initialization, it is always a new format
    flags.new_fmt = true;
    this_fmt_line = line_number;
    this_fmt_column = column_index + 1;
    return true;
}

bool FormatTracker::update_decimal_place_format(int min_dp, size_t column_index,
                                                std::vector<int>& dp_per_col,
                                                size_t line_number,
                                                Flags& flags) {
    if (print.debug3) {
        std::cout << "not first line" << std::endl;
    }

    // Safety check: ensure the vector is large enough for this column
    if (column_index >= dp_per_col.size()) {
        std::cerr << "Error: dp_per_col size (" << dp_per_col.size()
                  << ") insufficient for column " << column_index + 1 << " at line "
                  << line_number << std::endl;
        flags.error_found = true;
        return false;
    }

    // check if the minimum decimal places for this column has changed
    if (dp_per_col[column_index] != min_dp) {
        // If the minimum decimal places for this column is different from the
        // previous minimum decimal places, update it
        if (print.debug3) {
            std::cout << "DEBUG3: different" << std::endl;
            std::cout << "DEBUG3: format has changed" << std::endl;
        }
        dp_per_col[column_index] = min_dp;
        flags.new_fmt = true;
        this_fmt_line = line_number;
        if (print.debug) {
            std::cout << "FORMAT: Line " << this_fmt_line
                      << ": number of decimal places has changed" << std::endl;
        }
    }
    if (print.debug3 && dp_per_col[column_index] == min_dp) {
        // If the minimum decimal places for this column is the same as the
        // previous minimum decimal places, do nothing
        std::cout << "DEBUG3: same" << std::endl;
    }
    return true;
}

double FormatTracker::calculate_threshold(int decimal_places, double significant_threshold) const {
    // determine the smallest possible difference, base on the number of
    // decimal places (equal to the minimum number of decimal places for the
    // current element)
    double dp_threshold = std::pow(10, -decimal_places);

    if (dp_threshold > significant_threshold) {
        return dp_threshold;
    }
    return significant_threshold;
}

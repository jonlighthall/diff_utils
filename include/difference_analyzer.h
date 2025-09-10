/**
 * @author J. Lighthall
 * @date July 2025
 * Refactored from src/uband_diff.cpp
 */ 
 
#ifndef DIFFERENCE_ANALYZER_H
#define DIFFERENCE_ANALYZER_H

#include <cstddef>

// Forward declarations
struct ColumnValues;
struct CountStats;
struct DiffStats;
struct Flags;
struct Thresholds;

/**
 * @brief Analyzes differences between numerical values from two files
 *
 * This class is responsible for:
 * - Comparing numerical values with different precision levels
 * - Tracking difference statistics
 * - Categorizing differences (trivial, significant, critical)
 * - Managing difference thresholds
 */
class DifferenceAnalyzer {
public:
    explicit DifferenceAnalyzer(const Thresholds& thresholds);
    ~DifferenceAnalyzer() = default;

    // Main difference processing
    bool process_difference(const ColumnValues& column_data,
                            size_t column_index,
                            double threshold,
                            CountStats& counter,
                            DiffStats& differ,
                            Flags& flags) const;

    // Raw value processing (without rounding)
    void process_raw_values(const ColumnValues& column_data,
                            CountStats& counter,
                            DiffStats& differ,
                            Flags& flags) const;

    // Rounded value processing (with precision rounding)
    void process_rounded_values(const ColumnValues& column_data,
                                size_t column_index,
                                double rounded_diff,
                                int minimum_decimal_places,
                                CountStats& counter,
                                DiffStats& differ,
                                Flags& flags) const;

    // Helper functions
    static double round_to_decimals(double value, int precision);

    // Error reporting
    void print_hard_threshold_error(double rounded1, double rounded2,
                                   double diff_rounded, size_t column_index,
                                   const CountStats& counter) const;

private:
    const Thresholds& thresh;
};

#endif // DIFFERENCE_ANALYZER_H

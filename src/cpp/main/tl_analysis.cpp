/**
 * @file tl_analysis.cpp
 * @brief Main program for TL error accumulation analysis
 *
 * Reads two TL data files, computes point-by-point differences, and performs
 * statistical analysis to classify the error pattern as systematic, random,
 * or mixed. Outputs interpretation and recommended action.
 *
 * Usage: tl_analysis <file1> <file2> [threshold] [--verify] [--rmse-limit <dB>]
 *   file1      - Reference TL data file (range TL columns)
 *   file2      - Test TL data file (range TL columns)
 *   threshold  - Significance threshold for error classification
 *                (default: 0.05)
 *   --verify   - Enable strict verification mode (three-criteria test)
 *   --rmse-limit - Max RMSE tolerance in dB for --verify (default: auto)
 *
 * Multi-column support:
 *   Files may contain multiple TL columns (e.g., multiple frequencies).
 *   Column 1 is always range; columns 2..N are independent TL curves.
 *   Each curve is analyzed separately. In --verify mode, all must pass.
 *
 * Auto-calibrating threshold:
 *   When --rmse-limit is not specified in --verify mode, the threshold is
 *   derived from the printed precision of the data files:
 *     sigma_floor = 10^(-d) / sqrt(6)     [quantization noise for 2 files]
 *     rmse_limit  = 3 * sigma_floor        [3-sigma tolerance]
 *   where d is the minimum decimal places observed across both files.
 *
 * @author J. Lighthall
 * @date February 2026
 */

#include "tl_analysis.h"

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

#include "tl_metric.h"

struct ProgramArgs {
  std::string file1;
  std::string file2;
  double threshold = 0.05;
  double verify_rmse_limit = -1.0;     // max RMSE (dB) for --verify pass
                                        // negative = auto-calibrate from data
  double verify_spike_factor = 10.0;   // max_error must be < factor * RMSE
  double quantization_step = 0.1;      // LSB = 10^(-min_decimals)
  bool verify_mode = false;
  bool rmse_limit_explicit = false;    // true if user provided --rmse-limit
  int min_decimals = 1;                // detected printed precision
};

void print_usage(const char* program_name) {
  std::cout << "tl_analysis - TL Error Accumulation Analysis\n" << std::endl;
  std::cout << "USAGE:" << std::endl;
  std::cout << "  " << program_name << " <file1> <file2> [threshold] [--verify] [--rmse-limit <dB>]"
            << std::endl;
  std::cout << "\nARGUMENTS:" << std::endl;
  std::cout << "  file1       Reference TL data file (range TL columns)"
            << std::endl;
  std::cout << "  file2       Test TL data file (range TL columns)"
            << std::endl;
  std::cout << "  threshold   Significance threshold for error classification"
            << std::endl;
  std::cout << "              (default: 0.05)" << std::endl;
  std::cout << "  --verify    Enable strict verification mode" << std::endl;
  std::cout << "              Three-criteria test (all must pass):" << std::endl;
  std::cout << "              1. RMSE < threshold (default: auto from data)"
            << std::endl;
  std::cout << "              2. max_error < 10 x RMSE (no anomalous spikes)"
            << std::endl;
  std::cout << "              3. No systematic trend (slope p > 0.05)"
            << std::endl;
  std::cout << "  --rmse-limit <dB>   Set RMSE tolerance (default: auto)"
            << std::endl;
  std::cout << "              Auto mode derives threshold from printed"
            << std::endl;
  std::cout << "              precision: 3 * 10^(-d) / sqrt(6)" << std::endl;
  std::cout << "\nOUTPUT:" << std::endl;
  std::cout << "  Error pattern classification:" << std::endl;
  std::cout << "    SYSTEMATIC_GROWTH  Error increases with range" << std::endl;
  std::cout << "    SYSTEMATIC_BIAS    Fixed offset (calibration issue)"
            << std::endl;
  std::cout << "    RANDOM_NOISE       Uncorrelated (benign)" << std::endl;
  std::cout << "    NULL_POINT_NOISE   Small errors at null points"
            << std::endl;
  std::cout << "    TRANSIENT_SPIKES   Isolated large errors" << std::endl;
  std::cout << "\n  Statistical metrics:" << std::endl;
  std::cout << "    Linear regression (slope, R^2, p-value)" << std::endl;
  std::cout << "    Autocorrelation (lag-1)" << std::endl;
  std::cout << "    Wald-Wolfowitz run test" << std::endl;
  std::cout << "    RMSE, mean error, max error" << std::endl;
}

/**
 * @brief Count decimal places in a numeric string
 * @param s Numeric string (e.g., "45.41", "52.0", "55")
 * @return Number of decimal places (0 if no decimal point)
 */
int count_decimal_places(const std::string& s) {
  size_t dot = s.find('.');
  if (dot == std::string::npos) return 0;
  // Count digits after decimal point (excluding trailing whitespace)
  int count = 0;
  for (size_t i = dot + 1; i < s.size() && std::isdigit(s[i]); ++i) {
    count++;
  }
  return count;
}

/**
 * @brief Compute RMSE threshold from printed precision
 *
 * For values printed to d decimal places, the quantization step is 10^(-d).
 * Maximum rounding error is half that: epsilon = 0.5 * 10^(-d).
 * For uniform distribution on [-epsilon, epsilon], RMS = epsilon / sqrt(3).
 * For two independently-rounded files, errors add in quadrature:
 *   sigma_floor = sqrt(2) * epsilon / sqrt(3) = 10^(-d) / sqrt(6)
 *
 * The RMSE threshold is set to k * sigma_floor where k = 3 (3-sigma).
 *
 * @param min_decimals Minimum decimal places across both files
 * @return RMSE threshold in dB
 */
double compute_quantization_rmse_limit(int min_decimals) {
  double step = std::pow(10.0, -min_decimals);
  double sigma_floor = step / std::sqrt(6.0);
  return 3.0 * sigma_floor;  // 3-sigma tolerance
}

/**
 * @brief Read a single TL file into multi-column range-TL pairs
 *
 * Column 1 is range; columns 2..N are independent TL curves.
 * Also detects the minimum printed precision across all TL values.
 *
 * @param filename Path to TL data file
 * @param curves Output: one vector<RangeTLPair> per TL column
 * @param min_decimals Output: minimum decimal places observed in TL values
 * @return true if file was read successfully
 */
bool read_tl_file(const std::string& filename,
                  std::vector<std::vector<RangeTLPair>>& curves,
                  int& min_decimals) {
  std::ifstream f(filename);
  if (!f.is_open()) {
    std::cerr << "\033[1;31mERROR:\033[0m Cannot open file: " << filename
              << std::endl;
    return false;
  }

  min_decimals = 99;  // will be reduced as we parse
  std::string line;
  size_t skipped = 0;
  size_t n_cols = 0;  // detected from first valid line

  while (std::getline(f, line)) {
    if (line.empty() || line[0] == '#' || line[0] == '!') continue;

    // Tokenize the line
    std::istringstream iss(line);
    std::vector<std::string> tokens;
    std::string token;
    while (iss >> token) {
      tokens.push_back(token);
    }

    if (tokens.size() < 2) {
      skipped++;
      continue;
    }

    // Parse range from first column
    double range;
    try {
      range = std::stod(tokens[0]);
    } catch (...) {
      skipped++;
      continue;
    }

    // First valid line: detect number of TL columns
    if (n_cols == 0) {
      n_cols = tokens.size() - 1;  // subtract range column
      curves.resize(n_cols);
    }

    // Parse TL columns (handle variable column counts gracefully)
    size_t cols_this_row = std::min(tokens.size() - 1, n_cols);
    for (size_t c = 0; c < cols_this_row; ++c) {
      double tl;
      try {
        tl = std::stod(tokens[c + 1]);
      } catch (...) {
        continue;
      }
      curves[c].push_back({range, tl});

      // Track printed precision
      int dec = count_decimal_places(tokens[c + 1]);
      if (dec < min_decimals) {
        min_decimals = dec;
      }
    }
  }

  if (curves.empty() || curves[0].empty()) {
    std::cerr << "\033[1;31mERROR:\033[0m No valid data in: " << filename
              << std::endl;
    return false;
  }

  if (skipped > 0) {
    std::cerr << "\033[1;33mWARNING:\033[0m Skipped " << skipped
              << " non-parseable lines in " << filename << std::endl;
  }

  if (min_decimals == 99) min_decimals = 1;  // fallback

  return true;
}

/**
 * @brief Linearly interpolate a TL value at a given range
 * @param curve Sorted range-TL pairs
 * @param r Target range
 * @return Interpolated TL value
 */
double interpolate_tl(const std::vector<RangeTLPair>& curve, double r) {
  if (r <= curve.front().range) return curve.front().tl;
  if (r >= curve.back().range) return curve.back().tl;

  for (size_t i = 0; i < curve.size() - 1; ++i) {
    if (r >= curve[i].range && r <= curve[i + 1].range) {
      double t = (r - curve[i].range) / (curve[i + 1].range - curve[i].range);
      return curve[i].tl + t * (curve[i + 1].tl - curve[i].tl);
    }
  }
  return curve.back().tl;
}

/**
 * @brief Read two TL files, interpolate to common grid, compute differences
 *        for each TL column independently.
 *
 * @param file1 Reference file path
 * @param file2 Test file path
 * @param threshold Significance threshold
 * @param datasets Output: one ErrorAccumulationData per TL column
 * @param grid_mismatch Output: true if files had different grid sizes
 * @param n_points1 Output: number of rows in file 1
 * @param n_points2 Output: number of rows in file 2
 * @param n_columns Output: number of TL columns
 * @param min_decimals Output: minimum decimal places across both files
 * @return true if files were read successfully
 */
bool read_and_diff(const std::string& file1, const std::string& file2,
                   double threshold,
                   std::vector<ErrorAccumulationData>& datasets,
                   bool& grid_mismatch, size_t& n_points1, size_t& n_points2,
                   size_t& n_columns, int& min_decimals) {
  std::vector<std::vector<RangeTLPair>> curves1, curves2;
  int dec1 = 99, dec2 = 99;

  if (!read_tl_file(file1, curves1, dec1)) return false;
  if (!read_tl_file(file2, curves2, dec2)) return false;

  min_decimals = std::min(dec1, dec2);

  // Use the minimum column count between files
  n_columns = std::min(curves1.size(), curves2.size());
  if (n_columns == 0) {
    std::cerr << "\033[1;31mERROR:\033[0m No TL columns found." << std::endl;
    return false;
  }

  if (curves1.size() != curves2.size()) {
    std::cerr << "\033[1;33mNOTE:\033[0m Column count differs ("
              << curves1.size() << " vs " << curves2.size()
              << "). Using first " << n_columns << " columns." << std::endl;
  }

  n_points1 = curves1[0].size();
  n_points2 = curves2[0].size();
  grid_mismatch = (n_points1 != n_points2);

  if (grid_mismatch) {
    std::cerr << "\033[1;33mNOTE:\033[0m Grid sizes differ (" << n_points1
              << " vs " << n_points2 << "). Interpolating to common grid."
              << std::endl;
  }

  datasets.resize(n_columns);

  for (size_t col = 0; col < n_columns; ++col) {
    const auto& c1 = curves1[col];
    const auto& c2 = curves2[col];

    double maxRange = std::min(c1.back().range, c2.back().range);
    double minRange = std::max(c1.front().range, c2.front().range);
    size_t numPoints = std::max(c1.size(), c2.size());

    for (size_t i = 0; i < numPoints; ++i) {
      double r = minRange + (maxRange - minRange) * static_cast<double>(i) /
                                static_cast<double>(numPoints - 1);
      double tl1 = interpolate_tl(c1, r);
      double tl2 = interpolate_tl(c2, r);
      double error = tl1 - tl2;
      bool significant = std::abs(error) >= threshold;
      datasets[col].add_point(r, error, tl1, tl2, significant);
    }
  }

  if (datasets[0].n_points == 0) {
    std::cerr << "\033[1;31mERROR:\033[0m No valid comparison data found."
              << std::endl;
    return false;
  }

  return true;
}

/**
 * @brief Print analysis results for a single column
 */
void print_column_results(const AccumulationMetrics& metrics,
                          const ErrorAccumulationData& data,
                          size_t col_index, size_t n_columns) {
  if (n_columns > 1) {
    std::cout << "\n----- Column " << (col_index + 1) << " of " << n_columns
              << " -----" << std::endl;
  }

  // Count significant errors
  size_t n_significant = 0;
  for (bool sig : data.is_significant) {
    if (sig) n_significant++;
  }
  double pct_significant =
      100.0 * static_cast<double>(n_significant) / data.n_points;

  std::cout << "Significant errors: " << n_significant << " / " << data.n_points
            << " (" << std::fixed << std::setprecision(1) << pct_significant
            << "%)" << std::endl;
  std::cout << "RMSE:               " << std::scientific << std::setprecision(4)
            << metrics.rmse << std::endl;
  std::cout << "Mean error:         " << metrics.mean_error << std::endl;
  std::cout << "Max error:          " << metrics.max_error << std::endl;

  std::cout << "\nLinear regression:" << std::endl;
  std::cout << "  Slope:            " << std::scientific << metrics.slope
            << std::endl;
  std::cout << "  R^2:              " << std::fixed << std::setprecision(4)
            << metrics.r_squared << std::endl;
  std::cout << "  p-value:          " << std::scientific
            << metrics.p_value_slope << std::endl;

  std::cout << "Autocorrelation:" << std::endl;
  std::cout << "  Lag-1:            " << std::fixed << std::setprecision(4)
            << metrics.autocorr_lag1 << std::endl;
  std::cout << "  Correlated:       " << (metrics.is_correlated ? "YES" : "no")
            << std::endl;

  std::cout << "Run test:" << std::endl;
  std::cout << "  Runs:             " << metrics.n_runs << " (expected "
            << metrics.expected_runs << ")" << std::endl;
  std::cout << "  Z-score:          " << std::fixed << std::setprecision(3)
            << metrics.run_test_z_score << std::endl;
  std::cout << "  Random:           " << (metrics.is_random ? "YES" : "no")
            << std::endl;

  std::cout << "Pattern: "
            << ErrorAccumulationAnalyzer::get_pattern_name(metrics.pattern)
            << std::endl;
  std::cout << "  " << metrics.interpretation << std::endl;
}

/**
 * @brief Run verification tests for a single column
 *
 * All three criteria are informed by the quantization floor:
 * - Test 1: RMSE < threshold (auto-calibrated from precision)
 * - Test 2: max_error < max(factor * RMSE, LSB) — single-LSB not a spike
 * - Test 3: total drift < LSB OR p > 0.05 — sub-LSB trend not meaningful
 *
 * @return true if all three criteria pass
 */
bool run_verify_column(const AccumulationMetrics& metrics,
                       const ErrorAccumulationData& data,
                       const ProgramArgs& args, size_t col_index,
                       size_t n_columns) {
  if (n_columns > 1) {
    std::cout << "  Column " << (col_index + 1) << ":" << std::endl;
  }

  double lsb = args.quantization_step;

  // Test 1: RMSE below tolerance
  bool rmse_pass = metrics.rmse < args.verify_rmse_limit;
  std::cout << "  Test 1 - RMSE:       " << std::scientific
            << std::setprecision(4) << metrics.rmse;
  if (rmse_pass) {
    std::cout << " < " << args.verify_rmse_limit
              << "  \033[1;32mPASS\033[0m" << std::endl;
  } else {
    std::cout << " >= " << args.verify_rmse_limit
              << "  \033[1;31mFAIL\033[0m" << std::endl;
  }

  // Test 2: no anomalous spikes
  // A "spike" must exceed both factor * RMSE AND the RMSE tolerance.
  // If max_error is below the RMSE threshold, it can't be a meaningful anomaly.
  double spike_limit = std::max(args.verify_spike_factor * metrics.rmse,
                                args.verify_rmse_limit);
  bool spike_pass = (metrics.rmse == 0.0) ||
                    (metrics.max_error < spike_limit);
  std::cout << "  Test 2 - Max spike:  " << std::scientific
            << std::setprecision(4) << metrics.max_error;
  if (metrics.rmse > 0.0) {
    std::cout << " (" << std::fixed << std::setprecision(1)
              << (metrics.max_error / metrics.rmse) << "x RMSE)";
  }
  if (metrics.max_error <= lsb && !spike_pass) {
    // Would fail ratio but max is within LSB — override
    spike_pass = true;
    std::cout << "  \033[1;32mPASS\033[0m (within LSB)" << std::endl;
  } else if (spike_pass) {
    std::cout << "  \033[1;32mPASS\033[0m" << std::endl;
  } else {
    std::cout << "  \033[1;31mFAIL\033[0m (limit: "
              << std::scientific << std::setprecision(2) << spike_limit
              << ")" << std::endl;
  }

  // Test 3: no systematic trend with range
  // A trend is only meaningful if the total predicted drift exceeds the LSB.
  double range_extent = data.range_max - data.range_min;
  double total_drift = std::abs(metrics.slope) * range_extent;
  bool trend_pass = metrics.p_value_slope > 0.05 ||
                    std::isnan(metrics.p_value_slope) ||
                    total_drift < lsb;
  std::cout << "  Test 3 - Trend:      slope p-value = " << std::scientific
            << std::setprecision(4) << metrics.p_value_slope;
  if (trend_pass) {
    if (metrics.p_value_slope <= 0.05 && total_drift < lsb) {
      std::cout << "  \033[1;32mPASS\033[0m (drift " << std::scientific
                << std::setprecision(2) << total_drift << " < LSB)"
                << std::endl;
    } else {
      std::cout << "  \033[1;32mPASS\033[0m" << std::endl;
    }
  } else {
    std::cout << "  \033[1;31mFAIL\033[0m (drift " << std::scientific
              << std::setprecision(2) << total_drift << " dB over range)"
              << std::endl;
  }

  return rmse_pass && spike_pass && trend_pass;
}

/**
 * @brief Print full analysis results for all columns
 */
void print_results(const std::vector<AccumulationMetrics>& all_metrics,
                   const std::vector<ErrorAccumulationData>& datasets,
                   const ProgramArgs& args,
                   bool grid_mismatch, size_t n_points1, size_t n_points2,
                   size_t n_columns, int min_decimals) {
  std::cout << "\n===== TL Error Accumulation Analysis =====" << std::endl;
  std::cout << "Reference: " << args.file1 << std::endl;
  std::cout << "Test:      " << args.file2 << std::endl;
  std::cout << "Threshold: " << args.threshold << std::endl;
  std::cout << "Points:    " << datasets[0].n_points;
  if (grid_mismatch) {
    std::cout << " (interpolated from " << n_points1 << " / " << n_points2
              << ")";
  }
  std::cout << std::endl;
  if (n_columns > 1) {
    std::cout << "Columns:   " << n_columns << " TL curves" << std::endl;
  }
  std::cout << "Precision: " << min_decimals << " decimal places (LSB = "
            << std::pow(10.0, -min_decimals) << " dB)" << std::endl;
  std::cout << "Range:     " << datasets[0].range_min << " to "
            << datasets[0].range_max << std::endl;

  std::cout << "\n----- Error Summary -----" << std::endl;

  // Print per-column results
  for (size_t col = 0; col < n_columns; ++col) {
    print_column_results(all_metrics[col], datasets[col], col, n_columns);
  }

  // If multi-column, print pooled RMSE
  if (n_columns > 1) {
    double sum_sq = 0.0;
    size_t total_points = 0;
    for (size_t col = 0; col < n_columns; ++col) {
      sum_sq += all_metrics[col].rmse * all_metrics[col].rmse *
                datasets[col].n_points;
      total_points += datasets[col].n_points;
    }
    double pooled_rmse = std::sqrt(sum_sq / total_points);
    std::cout << "\n----- Pooled Summary -----" << std::endl;
    std::cout << "Pooled RMSE:        " << std::scientific
              << std::setprecision(4) << pooled_rmse << " dB ("
              << n_columns << " columns, " << total_points
              << " total points)" << std::endl;
  }

  // Verify mode: three-criteria statistical test
  if (args.verify_mode) {
    std::cout << "\n----- Verification Mode -----" << std::endl;

    // Report threshold source
    double sigma_floor = std::pow(10.0, -min_decimals) / std::sqrt(6.0);
    if (!args.rmse_limit_explicit) {
      std::cout << "RMSE limit: " << std::scientific << std::setprecision(4)
                << args.verify_rmse_limit << " dB (auto: 3 * "
                << sigma_floor << " quantization floor)" << std::endl;
    } else {
      std::cout << "RMSE limit: " << std::scientific << std::setprecision(4)
                << args.verify_rmse_limit << " dB (user-specified)"
                << std::endl;
    }

    bool all_pass = true;
    for (size_t col = 0; col < n_columns; ++col) {
      bool col_pass = run_verify_column(all_metrics[col], datasets[col],
                                        args, col, n_columns);
      if (!col_pass) all_pass = false;
    }

    // Overall verdict
    std::cout << "\nVerdict: ";
    if (all_pass) {
      std::cout << "\033[1;32mPASS\033[0m — curves are statistically "
                   "equivalent" << std::endl;
    } else {
      std::cout << "\033[1;31mFAIL\033[0m — meaningful divergence "
                   "detected" << std::endl;
    }
  }

  std::cout << "=========================================\n" << std::endl;
}

int main(int argc, char* argv[]) {
  // Help
  if (argc >= 2 &&
      (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help" ||
       std::string(argv[1]) == "help")) {
    print_usage(argv[0]);
    return 0;
  }

  // Validate arguments
  if (argc < 3) {
    std::cerr << "\033[1;33mWARNING:\033[0m Expected at least 2 arguments."
              << std::endl;
    std::cerr << "Usage: " << argv[0]
              << " <file1> <file2> [threshold] [--verify]" << std::endl;
    std::cerr << "Use '" << argv[0] << " --help' for detailed usage."
              << std::endl;
    return 1;
  }

  ProgramArgs args;
  args.file1 = argv[1];
  args.file2 = argv[2];

  // Parse optional arguments
  for (int i = 3; i < argc; i++) {
    std::string arg = argv[i];
    if (arg == "--verify") {
      args.verify_mode = true;
    } else if (arg == "--rmse-limit" && i + 1 < argc) {
      char* end;
      double val = std::strtod(argv[++i], &end);
      if (*end == '\0' && val > 0.0) {
        args.verify_rmse_limit = val;
        args.rmse_limit_explicit = true;
      } else {
        std::cerr << "\033[1;31mERROR:\033[0m Invalid --rmse-limit value: "
                  << argv[i] << std::endl;
        return 1;
      }
    } else {
      // Try parsing as threshold
      char* end;
      double val = std::strtod(argv[i], &end);
      if (*end == '\0' && val >= 0.0) {
        args.threshold = val;
      } else {
        std::cerr << "\033[1;31mERROR:\033[0m Unknown argument: " << arg
                  << std::endl;
        return 1;
      }
    }
  }

  // Read files, interpolate to common grid, and compute differences
  std::vector<ErrorAccumulationData> datasets;
  bool grid_mismatch = false;
  size_t n_points1 = 0, n_points2 = 0, n_columns = 0;
  int min_decimals = 1;
  if (!read_and_diff(args.file1, args.file2, args.threshold, datasets,
                     grid_mismatch, n_points1, n_points2, n_columns,
                     min_decimals)) {
    return 1;
  }

  // Auto-calibrate RMSE limit from printed precision if not explicit
  if (args.verify_mode && !args.rmse_limit_explicit) {
    args.verify_rmse_limit = compute_quantization_rmse_limit(min_decimals);
  }
  args.min_decimals = min_decimals;
  args.quantization_step = std::pow(10.0, -min_decimals);

  std::cout << "Read " << datasets[0].n_points << " comparison points";
  if (n_columns > 1) {
    std::cout << " across " << n_columns << " TL columns";
  }
  std::cout << "." << std::endl;

  // Run analysis on each column
  ErrorAccumulationAnalyzer analyzer;
  std::vector<AccumulationMetrics> all_metrics;
  for (size_t col = 0; col < n_columns; ++col) {
    all_metrics.push_back(analyzer.analyze(datasets[col]));
  }

  // Print results
  print_results(all_metrics, datasets, args, grid_mismatch,
                n_points1, n_points2, n_columns, min_decimals);

  // Exit code: in verify mode, fail if any criterion in any column fails
  if (args.verify_mode) {
    double lsb = args.quantization_step;
    for (size_t col = 0; col < n_columns; ++col) {
      const auto& m = all_metrics[col];
      const auto& d = datasets[col];
      bool rmse_pass = m.rmse < args.verify_rmse_limit;
      double spike_limit = std::max(args.verify_spike_factor * m.rmse,
                                    args.verify_rmse_limit);
      bool spike_pass = (m.rmse == 0.0) || (m.max_error < spike_limit);
      double range_extent = d.range_max - d.range_min;
      double total_drift = std::abs(m.slope) * range_extent;
      bool trend_pass = m.p_value_slope > 0.05 ||
                        std::isnan(m.p_value_slope) || total_drift < lsb;
      if (!(rmse_pass && spike_pass && trend_pass)) return 1;
    }
  }

  return 0;
}

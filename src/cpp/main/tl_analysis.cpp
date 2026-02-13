/**
 * @file tl_analysis.cpp
 * @brief Main program for TL error accumulation analysis
 *
 * Reads two TL data files, computes point-by-point differences, and performs
 * statistical analysis to classify the error pattern as systematic, random,
 * or mixed. Outputs interpretation and recommended action.
 *
 * Usage: tl_analysis <file1> <file2> [threshold] [--verify]
 *   file1      - Reference TL data file (range TL columns)
 *   file2      - Test TL data file (range TL columns)
 *   threshold  - Significance threshold for error classification
 *                (default: 0.05)
 *   --verify   - Enable strict verification mode (applies 2% aggregate rule)
 *
 * @author J. Lighthall
 * @date February 2026
 */

#include "tl_analysis.h"

#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

struct ProgramArgs {
  std::string file1;
  std::string file2;
  double threshold = 0.05;
  bool verify_mode = false;  // --verify enables 2% aggregate rule
};

void print_usage(const char* program_name) {
  std::cout << "tl_analysis - TL Error Accumulation Analysis\n" << std::endl;
  std::cout << "USAGE:" << std::endl;
  std::cout << "  " << program_name << " <file1> <file2> [threshold] [--verify]"
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
  std::cout << "              Applies 2% aggregate significance rule"
            << std::endl;
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
 * @brief Read two TL files and compute differences
 * @param file1 Reference file path
 * @param file2 Test file path
 * @param threshold Significance threshold
 * @param data Output error accumulation data
 * @return true if files were read successfully
 */
bool read_and_diff(const std::string& file1, const std::string& file2,
                   double threshold, ErrorAccumulationData& data) {
  std::ifstream f1(file1);
  std::ifstream f2(file2);

  if (!f1.is_open()) {
    std::cerr << "\033[1;31mERROR:\033[0m Cannot open file: " << file1
              << std::endl;
    return false;
  }
  if (!f2.is_open()) {
    std::cerr << "\033[1;31mERROR:\033[0m Cannot open file: " << file2
              << std::endl;
    return false;
  }

  std::string line1, line2;
  size_t line_number = 0;
  size_t skipped = 0;

  while (std::getline(f1, line1) && std::getline(f2, line2)) {
    line_number++;

    // Skip empty lines and comments
    if (line1.empty() || line1[0] == '#' || line1[0] == '!') continue;
    if (line2.empty() || line2[0] == '#' || line2[0] == '!') continue;

    std::istringstream iss1(line1);
    std::istringstream iss2(line2);
    double range1, tl1, range2, tl2;

    if ((iss1 >> range1 >> tl1) && (iss2 >> range2 >> tl2)) {
      // Use range from file 1
      double error = tl1 - tl2;
      bool significant = std::abs(error) >= threshold;
      data.add_point(range1, error, tl1, tl2, significant);
    } else {
      skipped++;
    }
  }

  if (data.n_points == 0) {
    std::cerr << "\033[1;31mERROR:\033[0m No valid comparison data found."
              << std::endl;
    return false;
  }

  if (skipped > 0) {
    std::cerr << "\033[1;33mWARNING:\033[0m Skipped " << skipped
              << " non-parseable line pairs." << std::endl;
  }

  return true;
}

/**
 * @brief Print analysis results
 */
void print_results(const AccumulationMetrics& metrics,
                   const ErrorAccumulationData& data, const ProgramArgs& args) {
  std::cout << "\n===== TL Error Accumulation Analysis =====" << std::endl;
  std::cout << "Reference: " << args.file1 << std::endl;
  std::cout << "Test:      " << args.file2 << std::endl;
  std::cout << "Threshold: " << args.threshold << std::endl;
  std::cout << "Points:    " << data.n_points << std::endl;
  std::cout << "Range:     " << data.range_min << " to " << data.range_max
            << std::endl;

  // Count significant errors
  size_t n_significant = 0;
  for (bool sig : data.is_significant) {
    if (sig) n_significant++;
  }
  double pct_significant =
      100.0 * static_cast<double>(n_significant) / data.n_points;

  std::cout << "\n----- Error Summary -----" << std::endl;
  std::cout << "Significant errors: " << n_significant << " / " << data.n_points
            << " (" << std::fixed << std::setprecision(1) << pct_significant
            << "%)" << std::endl;
  std::cout << "RMSE:               " << std::scientific << std::setprecision(4)
            << metrics.rmse << std::endl;
  std::cout << "Mean error:         " << metrics.mean_error << std::endl;
  std::cout << "Max error:          " << metrics.max_error << std::endl;

  std::cout << "\n----- Statistical Tests -----" << std::endl;
  std::cout << "Linear regression:" << std::endl;
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

  std::cout << "\n----- Classification -----" << std::endl;
  std::cout << "Pattern: "
            << ErrorAccumulationAnalyzer::get_pattern_name(metrics.pattern)
            << std::endl;
  std::cout << "\nInterpretation:" << std::endl;
  std::cout << "  " << metrics.interpretation << std::endl;
  std::cout << "\nRecommendation:" << std::endl;
  std::cout << "  " << metrics.recommendation << std::endl;

  // Verify mode: apply 2% aggregate rule
  if (args.verify_mode) {
    std::cout << "\n----- Verification Mode -----" << std::endl;
    if (pct_significant > 2.0) {
      std::cout << "\033[1;31mFAIL:\033[0m " << std::fixed
                << std::setprecision(1) << pct_significant
                << "% of comparisons exceed threshold (limit: 2%)" << std::endl;
    } else {
      std::cout << "\033[1;32mPASS:\033[0m " << std::fixed
                << std::setprecision(1) << pct_significant
                << "% of comparisons exceed threshold (limit: 2%)" << std::endl;
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

  // Read files and compute differences
  ErrorAccumulationData data;
  if (!read_and_diff(args.file1, args.file2, args.threshold, data)) {
    return 1;
  }

  std::cout << "Read " << data.n_points << " comparison points." << std::endl;

  // Run analysis
  ErrorAccumulationAnalyzer analyzer;
  AccumulationMetrics metrics = analyzer.analyze(data);

  // Print results
  print_results(metrics, data, args);

  // Exit code: in verify mode, fail if >2% significant
  if (args.verify_mode) {
    size_t n_significant = 0;
    for (bool sig : data.is_significant) {
      if (sig) n_significant++;
    }
    double pct = 100.0 * static_cast<double>(n_significant) / data.n_points;
    if (pct > 2.0) return 1;
  }

  return 0;
}

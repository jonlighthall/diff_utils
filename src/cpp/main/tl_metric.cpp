/**
 * @file tl_metric.cpp
 * @brief Main program for computing Fabre TL curve comparison metrics
 *
 * Reads two TL data files and computes the five Fabre metric components
 * (M1-M5) plus composite scores (M_curve, M_total).
 *
 * Usage: tl_metric <file1> <file2> [fom]
 *   file1  - Reference TL data file (range TL columns)
 *   file2  - Test TL data file (range TL columns)
 *   fom    - Optional Figure of Merit in dB (enables M4/M5)
 *
 * Input format: Each line contains "range TL" values, whitespace-separated.
 * Lines with inconsistent column counts or non-numeric data are skipped.
 *
 * @author J. Lighthall
 * @date February 2026
 */

#include "tl_metric.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

// Forward declarations
struct ProgramArgs {
  std::string file1;
  std::string file2;
  double fom = 0.0;  // Figure of Merit (0 = not used)
};

void print_usage(const char* program_name) {
  std::cout << "tl_metric - TL Curve Comparison Metrics (Fabre M1-M5)\n"
            << std::endl;
  std::cout << "USAGE:" << std::endl;
  std::cout << "  " << program_name << " <file1> <file2> [fom]" << std::endl;
  std::cout << "\nARGUMENTS:" << std::endl;
  std::cout << "  file1   Reference TL data file (range TL columns)"
            << std::endl;
  std::cout << "  file2   Test TL data file (range TL columns)" << std::endl;
  std::cout << "  fom     Optional Figure of Merit in dB (enables M4/M5)"
            << std::endl;
  std::cout << "\nINPUT FORMAT:" << std::endl;
  std::cout << "  Each line: range_value  TL_value" << std::endl;
  std::cout << "  Whitespace-separated, two columns minimum" << std::endl;
  std::cout << "\nMETRICS:" << std::endl;
  std::cout << "  M1  Weighted mean absolute difference (dB)" << std::endl;
  std::cout << "  M2  Mean difference over last 4% of range (dB)" << std::endl;
  std::cout << "  M3  Correlation coefficient" << std::endl;
  std::cout << "  M4  Range coverage energy difference (requires FOM)"
            << std::endl;
  std::cout << "  M5  Detection range difference (requires FOM)" << std::endl;
  std::cout << "\n  M_curve = mean(M1, M2, M3)" << std::endl;
  std::cout << "  M_total = mean(M1, M2, M3, M4, M5)" << std::endl;
  std::cout << "\nREFERENCE:" << std::endl;
  std::cout << "  Fabre & Zingarelli, IEEE OCEANS 2009" << std::endl;
  std::cout << "  doi:10.23919/OCEANS.2009.5422312" << std::endl;
}

/**
 * @brief Read a two-column (range, TL) file into RangeTLPair vector
 * @param filename Path to the input file
 * @param curve Output vector of range-TL pairs
 * @return true if file was read successfully
 */
bool read_tl_file(const std::string& filename,
                  std::vector<RangeTLPair>& curve) {
  std::ifstream file(filename);
  if (!file.is_open()) {
    std::cerr << "\033[1;31mERROR:\033[0m Cannot open file: " << filename
              << std::endl;
    return false;
  }

  std::string line;
  size_t line_number = 0;
  size_t skipped = 0;

  while (std::getline(file, line)) {
    line_number++;

    // Skip empty lines and comment lines
    if (line.empty() || line[0] == '#' || line[0] == '!') continue;

    std::istringstream iss(line);
    double range, tl;

    if (iss >> range >> tl) {
      curve.push_back({range, tl});
    } else {
      skipped++;
    }
  }

  if (curve.empty()) {
    std::cerr << "\033[1;31mERROR:\033[0m No valid data found in: " << filename
              << std::endl;
    return false;
  }

  if (skipped > 0) {
    std::cerr << "\033[1;33mWARNING:\033[0m Skipped " << skipped
              << " non-parseable lines in: " << filename << std::endl;
  }

  return true;
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
  if (argc < 3 || argc > 4) {
    std::cerr << "\033[1;33mWARNING:\033[0m Expected 2-3 arguments."
              << std::endl;
    std::cerr << "Usage: " << argv[0] << " <file1> <file2> [fom]" << std::endl;
    std::cerr << "Use '" << argv[0] << " --help' for detailed usage."
              << std::endl;
    return 1;
  }

  ProgramArgs args;
  args.file1 = argv[1];
  args.file2 = argv[2];

  if (argc == 4) {
    char* end;
    args.fom = std::strtod(argv[3], &end);
    if (*end != '\0' || args.fom <= 0.0) {
      std::cerr << "\033[1;31mERROR:\033[0m FOM must be a positive number: "
                << argv[3] << std::endl;
      return 1;
    }
  }

  // Read input files
  std::vector<RangeTLPair> curve1, curve2;

  if (!read_tl_file(args.file1, curve1)) return 1;
  if (!read_tl_file(args.file2, curve2)) return 1;

  std::cout << "Read " << curve1.size() << " points from " << args.file1
            << std::endl;
  std::cout << "Read " << curve2.size() << " points from " << args.file2
            << std::endl;

  // Compute metrics
  try {
    TLMetrics metrics(curve1, curve2, args.fom);
    metrics.printResults();
    metrics.printStats();
  } catch (const std::exception& e) {
    std::cerr << "\033[1;31mERROR:\033[0m " << e.what() << std::endl;
    return 1;
  }

  return 0;
}

/**
 * Pi Precision Test - C++ Version
 *
 * Tests different rounding behaviors across languages.
 * C++ uses std::fixed and std::setprecision which rounds to nearest,
 * ties to even (banker's rounding) in recent implementations.
 */

#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <string>

void write_precision_file(const std::string& filename, double value,
                          int start_dp, int end_dp, int step) {
  /**
   * Write value with varying decimal precision to file
   *
   * Args:
   *   filename: output file name
   *   value: the number to write (e.g., pi, sqrt(2), etc.)
   *   start_dp: starting decimal places
   *   end_dp: ending decimal places
   *   step: increment (+1 for ascending, -1 for descending)
   */
  std::ofstream outfile(filename);

  if (!outfile.is_open()) {
    std::cerr << "Error: Could not open file " << filename << std::endl;
    return;
  }

  // Loop over decimal places and include an index column (mimics range/time)
  int line_no = 1;
  if (step > 0) {
    for (int i = start_dp; i <= end_dp; i += step) {
      outfile << line_no << "  ";
      if (i == 0) {
        outfile << static_cast<int>(value) << std::endl;
      } else {
        outfile << std::fixed << std::setprecision(i) << value << std::endl;
      }
      ++line_no;
    }
  } else {
    for (int i = start_dp; i >= end_dp; i += step) {
      outfile << line_no << "  ";
      if (i == 0) {
        outfile << static_cast<int>(value) << std::endl;
      } else {
        outfile << std::fixed << std::setprecision(i) << value << std::endl;
      }
      ++line_no;
    }
  }

  outfile.close();
}

int main(int argc, char* argv[]) {
  // Calculate pi
  double pi = 4.0 * std::atan(1.0);

  // Get machine epsilon
  double epsilon = std::numeric_limits<double>::epsilon();

  // Calculate max decimal places (conservative)
  int max_decimal_places = static_cast<int>(-std::log10(epsilon)) - 1;
  if (max_decimal_places > 15) {
    max_decimal_places = 15;
  }

  // Get base filename from command line or use default
  std::string base_filename = "pi_output.txt";
  if (argc > 1) {
    base_filename = argv[1];
  }

  // Normalize base filename by stripping a trailing .txt if present
  std::string base_no_ext = base_filename;
  if (base_no_ext.size() > 4 &&
      base_no_ext.substr(base_no_ext.size() - 4) == ".txt") {
    base_no_ext = base_no_ext.substr(0, base_no_ext.size() - 4);
  }

  // Program identifier used in output filenames (standardized)
  const std::string prog = "pi_gen_cpp";

  // Generate ascending / descending precision files with standardized names
  // Use explicit language tag so executables across languages don't clobber
  // each other's output: pi_<language>_asc.txt / pi_<language>_desc.txt
  // Use standard fixed filenames so scripts can rely on consistent names
  const std::string asc_name = "pi_cpp_asc.txt";
  const std::string desc_name = "pi_cpp_desc.txt";

  // Include an index column (mimics a range/time column): "index value"
  write_precision_file(asc_name, pi, 0, max_decimal_places, 1);
  write_precision_file(desc_name, pi, max_decimal_places, 0, -1);

  // Print summary to console
  std::cout << "Pi Precision Test Program (C++)" << std::endl;
  std::cout << "================================" << std::endl;
  std::cout << std::fixed << std::setprecision(15);
  std::cout << "Calculated pi:           " << pi << std::endl;
  std::cout << std::scientific << std::setprecision(5);
  std::cout << "Machine epsilon:         " << epsilon << std::endl;
  std::cout << "Max valid decimal places: " << max_decimal_places << std::endl;
  std::cout << std::endl;
  std::cout << "Ascending file:  " << asc_name << std::endl;
  std::cout << "Descending file: " << desc_name << std::endl;
  std::cout << "Each contains " << (max_decimal_places + 1) << " lines"
            << std::endl;
  std::cout << std::endl;
  std::cout
      << "Rounding mode: Round to nearest, ties to even (banker's rounding)"
      << std::endl;

  return 0;
}

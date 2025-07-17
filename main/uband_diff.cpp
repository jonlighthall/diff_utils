/**
 * @file uband_diff.cpp
 * @brief Main program for comparing numerical data files with
 * precision-aware difference thresholds. Supports complex numbers and variable
 * precision.
 *
 * @author J. Lighthall
 * @date January 2025
 * Adapted from tldiff.f90 (Aug 2022)
 *
 * Usage: uband_diff <file1> <file2> [threshold] [hard_threshold] [print_level]
 *   file1, file2    - Input files to compare
 *   threshold       - Soft difference threshold (default: 0.05)
 *   hard_threshold  - Hard difference threshold for failure (default: 10.0)
 *   print_level     - Print verbosity level (default: 1.0)
 */

#include "uband_diff.h"

#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
  /* ARGUMENTS */
  // argv[0] is the program name
  // argv[1] is the first file name
  // argv[2] is the second file name
  // argv[3] is the threshold for differences (default: 0.05)
  // argv[4] is the hard threshold for differences (default: 1.0)
  //

  /* DEFAULTS */
  std::string file1 = "file1.txt";
  std::string file2 = "file2.txt";
  double count_level = 0.05;
  double hard_level = 10;
  double print_level = 1;  // print level for differences

#ifdef DEBUG2
  std::cout << "Debug mode 2 is ON" << std::endl;
#endif

  if (argc < 3) {
    std::cout << "WARNING: " << std::endl;
    std::cout << "   Two file names not provided." << std::endl;
    std::cout << "   Usage: " << argv[0]
              << " <file1> <file2> [threshold] [hard_threshold] [print_level]"
              << std::endl;
  }

#ifdef DEBUG
  std::cout << "Debug mode is ON" << std::endl;
  // Print the path of this source file
  std::cout << "SOURCE: " << std::endl;
  std::cout << "   " << __FILE__ << std::endl;
  std::cout << "ARGUMENTS: " << std::endl;
  std::cout << "   argc   : " << argc << std::endl;
  for (int i = 0; i < argc; i++) {
    std::cout << "   argv[" << i << "]: " << argv[i] << std::endl;
  }
#endif

  // check the number of arguments
  // remember that argv[0] is the program name
  // so we expect at least 2 arguments (file1 and file2)
  // and at most 6 arguments (file1, file2, threshold, hard_threshold,
  // print_level) if the number of arguments is not in this range, print an
  // error

  // if the number of arguments is zero, use the default file names and print a
  // warning

  if (argc > 6) {
    std::cerr << "Usage: " << argv[0]
              << " <file1> <file2> [threshold] [hard_threshold] [print_level]"
              << std::endl;
    return 1;
  }

  std::cout << "SETTINGS: " << std::endl;

  // Check if the user provided file names as arguments
  if (argc >= 3) {
    file1 = argv[1];  // first file name
    file2 = argv[2];  // second file name
  } else {
#ifdef DEBUG
    std::cout << "   Using default file names:" << std::endl;
#endif
  }
#ifdef DEBUG
  std::cout << "   File1: " << file1 << std::endl;
  std::cout << "   File2: " << file2 << std::endl;
#endif

  std::cout << "   Diff threshold : " << std::fixed << std::setprecision(3)
            << std::setw(7) << std::right;
  if (argc >= 4) {
    count_level = std::stof(argv[3]);
    std::cout << count_level << std::endl;
  } else {
    std::cout << count_level << " (default)" << std::endl;
  }

  std::cout << "   High threshold : \033[1;31m" << std::fixed
            << std::setprecision(3) << std::setw(7) << std::right;
  if (argc >= 5) {
    hard_level = std::stof(argv[4]);
    std::cout << hard_level << "\033[0m" << std::endl;
  } else {
    std::cout << hard_level << "\033[0m (default)" << std::endl;
  }

  std::cout << "   Print threshold: " << std::fixed << std::setprecision(3)
            << std::setw(7) << std::right;
  if (argc >= 6) {
    print_level = std::stof(argv[5]);
    std::cout << print_level << "\033[0m" << std::endl;
  } else {
    std::cout << print_level << "\033[0m (default)" << std::endl;
  }

  FileComparator comparator(count_level, hard_level, print_level);
  bool result = comparator.compareFiles(file1, file2);
  comparator.printSummary(file1, file2, argc, argv);

  if (comparator.getFlag().isERROR) {
    std::cout << "   \033[1;31mError found.\033[0m" << std::endl;
    return 1;
  }

  if (result) {
    std::cout << "Files are identical within tolerance." << std::endl;
    return 0;
  } else {
    std::cout << "Files differ significantly." << std::endl;
    return 1;
  }
}
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

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc, char* argv[]) {
  /* ARGUMENTS */
  // argv[0] is the program name
  // argv[1] is the first file name
  // argv[2] is the second file name
  // argv[3] is the threshold for significant differences (default: 0.05)
  // argv[4] is the threshold for critical differences (default: 1.0)
  // argv[5] is the threshold for printing entries in the differences table
  // (default: 1.0) argv[6] is the debug level (default: 0)

  /* DEFAULTS */
  std::string file1 = "file1.txt";
  std::string file2 = "file2.txt";
  double count_level = 0.05;
  double hard_level = 10;
  double print_level = 1;  // print level for differences
  int debug_level = 0;     // debug level

#ifdef DEBUG2
  std::cout << "Debug mode 2 is ON" << std::endl;
#endif

  // Check for help request
  if (argc >= 2 && (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help" || std::string(argv[1]) == "help")) {
    std::cout << "uband_diff - Numerical File Comparison Tool\n" << std::endl;
    std::cout << "USAGE:" << std::endl;
    std::cout << "  " << argv[0] << " <file1> <file2> [threshold] [hard_threshold] [print_level] [debug_level]" << std::endl;
    std::cout << "\nARGUMENTS:" << std::endl;
    std::cout << "  file1           First input file to compare" << std::endl;
    std::cout << "  file2           Second input file to compare" << std::endl;
    std::cout << "  threshold       Soft difference threshold for counting differences" << std::endl;
    std::cout << "                  (default: 0.05, must be ≥ 0)" << std::endl;
    std::cout << "  hard_threshold  Hard difference threshold for failure detection" << std::endl;
    std::cout << "                  (default: 10.0, must be ≥ 0, typically > threshold)" << std::endl;
    std::cout << "  print_level     Print verbosity level for difference table" << std::endl;
    std::cout << "                  (default: 1.0, must be ≥ 0)" << std::endl;
    std::cout << "  debug_level     Debug output level" << std::endl;
    std::cout << "                  (default: 0, typically 0-3)" << std::endl;
    std::cout << "\nEXAMPLES:" << std::endl;
    std::cout << "  " << argv[0] << " data1.txt data2.txt" << std::endl;
    std::cout << "  " << argv[0] << " file1.dat file2.dat 0.01" << std::endl;
    std::cout << "  " << argv[0] << " test1.txt test2.txt 0.05 1.0 0.1 2" << std::endl;
    std::cout << "\nFEATURES:" << std::endl;
    std::cout << "  - Precision-aware numerical comparison" << std::endl;
    std::cout << "  - Complex number support" << std::endl;
    std::cout << "  - Configurable difference thresholds" << std::endl;
    std::cout << "  - Detailed difference reporting" << std::endl;
    return 0;
  }

  if (argc < 3) {
    std::cout << "\033[1;33mWARNING:\033[0m Two file names not provided." << std::endl;
    std::cout << "   Usage: " << argv[0]
              << " <file1> <file2> [threshold] [hard_threshold] [print_level] [debug_level]"
              << std::endl;
    std::cout << "   Use '" << argv[0] << " --help' for detailed usage information." << std::endl;
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

  if (argc > 8) {  // Updated to allow up to 7 arguments (including program name)
    std::cerr << "\033[1;31mERROR:\033[0m Too many arguments provided." << std::endl;
    std::cerr << "Usage: " << argv[0]
              << " <file1> <file2> [threshold] [hard_threshold] [print_level] [debug_level]"
              << std::endl;
    std::cerr << "Use '" << argv[0] << " --help' for detailed usage information." << std::endl;
    return 1;
  }

  // Check if the user provided file names as arguments
  if (argc >= 3) {
    file1 = argv[1];  // first file name
    file2 = argv[2];  // second file name

    // Validate file arguments
    if (file1.empty()) {
      std::cerr << "\033[1;31mERROR:\033[0m First file name cannot be empty." << std::endl;
      return 1;
    }
    if (file2.empty()) {
      std::cerr << "\033[1;31mERROR:\033[0m Second file name cannot be empty." << std::endl;
      return 1;
    }
    if (file1 == file2) {
      std::cerr << "\033[1;33mWARNING:\033[0m Both files have the same name: '" << file1 << "'" << std::endl;
      std::cerr << "         This will compare the file with itself." << std::endl;
    }

    // Check if files exist and are readable (basic validation)
    std::ifstream test_file1(file1);
    std::ifstream test_file2(file2);

    if (!test_file1.good()) {
      std::cerr << "\033[1;33mWARNING:\033[0m Cannot access first file: '" << file1 << "'" << std::endl;
      std::cerr << "         The file may not exist or is not readable." << std::endl;
      std::cerr << "         Will attempt to proceed (error will be reported by comparator)." << std::endl;
    }
    if (!test_file2.good()) {
      std::cerr << "\033[1;33mWARNING:\033[0m Cannot access second file: '" << file2 << "'" << std::endl;
      std::cerr << "         The file may not exist or is not readable." << std::endl;
      std::cerr << "         Will attempt to proceed (error will be reported by comparator)." << std::endl;
    }

    test_file1.close();
    test_file2.close();
  } else {
#ifdef DEBUG
    std::cout << "   Using default file names:" << std::endl;
#endif
  }
#ifdef DEBUG
  std::cout << "   File1: " << file1 << std::endl;
  std::cout << "   File2: " << file2 << std::endl;
#endif

  // Parse and validate threshold arguments with error handling
  if (argc >= 4) {
    try {
      count_level = std::stof(argv[3]);
      if (count_level < 0) {
        std::cerr << "\n\033[1;31mERROR:\033[0m Diff threshold must be non-negative." << std::endl;
        std::cerr << "       Got: " << argv[3] << " (parsed as " << count_level << ")" << std::endl;
        std::cerr << "       Valid range: [0, ∞)" << std::endl;
        return 1;
      }
    } catch (const std::invalid_argument& e) {
      std::cerr << "\n\033[1;31mERROR:\033[0m Invalid diff threshold format." << std::endl;
      std::cerr << "       Expected: floating-point number (e.g., 0.05, 1.5, 10)" << std::endl;
      std::cerr << "       Got: '" << argv[3] << "' (" << e.what() << ")" << std::endl;
      return 1;
    } catch (const std::out_of_range& e) {
      std::cerr << "\n\033[1;31mERROR:\033[0m Diff threshold value out of range." << std::endl;
      std::cerr << "       Got: '" << argv[3] << "' (" << e.what() << ")" << std::endl;
      return 1;
    }
  }

  if (argc >= 5) {
    try {
      hard_level = std::stof(argv[4]);
      if (hard_level < 0) {
        std::cerr << "\n\033[1;31mERROR:\033[0m High threshold must be non-negative." << std::endl;
        std::cerr << "       Got: " << argv[4] << " (parsed as " << hard_level << ")" << std::endl;
        std::cerr << "       Valid range: [0, ∞)" << std::endl;
        return 1;
      }
      if (hard_level < count_level) {
        std::cerr << "\n\033[1;33mWARNING:\033[0m High threshold (" << hard_level
                  << ") is less than diff threshold (" << count_level << ")." << std::endl;
        std::cerr << "         This may lead to unexpected behavior." << std::endl;
      }
    } catch (const std::invalid_argument& e) {
      std::cerr << "\n\033[1;31mERROR:\033[0m Invalid high threshold format." << std::endl;
      std::cerr << "       Expected: floating-point number (e.g., 1.0, 10.0, 100)" << std::endl;
      std::cerr << "       Got: '" << argv[4] << "' (" << e.what() << ")" << std::endl;
      return 1;
    } catch (const std::out_of_range& e) {
      std::cerr << "\n\033[1;31mERROR:\033[0m High threshold value out of range." << std::endl;
      std::cerr << "       Got: '" << argv[4] << "' (" << e.what() << ")" << std::endl;
      return 1;
    }
  }

  if (argc >= 6) {
    try {
      print_level = std::stof(argv[5]);
      if (print_level < 0) {
        std::cerr << "\n\033[1;31mERROR:\033[0m Print threshold must be non-negative." << std::endl;
        std::cerr << "       Got: " << argv[5] << " (parsed as " << print_level << ")" << std::endl;
        std::cerr << "       Valid range: [0, ∞)" << std::endl;
        return 1;
      }

    } catch (const std::invalid_argument& e) {
      std::cerr << "\n\033[1;31mERROR:\033[0m Invalid print threshold format." << std::endl;
      std::cerr << "       Expected: floating-point number (e.g., 0.1, 1.0, 5.0)" << std::endl;
      std::cerr << "       Got: '" << argv[5] << "' (" << e.what() << ")" << std::endl;
      return 1;
    } catch (const std::out_of_range& e) {
      std::cerr << "\n\033[1;31mERROR:\033[0m Print threshold value out of range." << std::endl;
      std::cerr << "       Got: '" << argv[5] << "' (" << e.what() << ")" << std::endl;
      return 1;
    }
  }

  if (argc >= 7) {
    try {
      debug_level = std::stoi(argv[6]);
      if (debug_level < -1) {
        std::cerr << "\n\033[1;31mERROR:\033[0m Debug level must be greater than or equal to -1." << std::endl;
        std::cerr << "       Got: " << argv[6] << " (parsed as " << debug_level << ")" << std::endl;
        std::cerr << "       Valid range: [-1, 10] (typical values: 0-3)" << std::endl;
        return 1;
      }
      if (debug_level > 10) {
        std::cerr << "\n\033[1;33mWARNING:\033[0m Debug level (" << debug_level
                  << ") is unusually high." << std::endl;
        std::cerr << "         Typical range: [0, 3]. Proceeding anyway..." << std::endl;
      }
    } catch (const std::invalid_argument& e) {
      std::cerr << "\n\033[1;31mERROR:\033[0m Invalid debug level format." << std::endl;
      std::cerr << "       Expected: integer (e.g., 0, 1, 2, 3)" << std::endl;
      std::cerr << "       Got: '" << argv[6] << "' (" << e.what() << ")" << std::endl;
      return 1;
    } catch (const std::out_of_range& e) {
      std::cerr << "\n\033[1;31mERROR:\033[0m Debug level value out of range." << std::endl;
      std::cerr << "       Got: '" << argv[6] << "' (" << e.what() << ")" << std::endl;
      return 1;
    }
  }

  FileComparator comparator(count_level, hard_level, print_level, debug_level);
  bool result = comparator.compare_files(file1, file2);
  comparator.print_summary(file1, file2, argc, argv);

  if (comparator.getFlag().error_found) {
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
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
 * Usage: uband_diff <file1> <file2> [sig_thresh] [crit_thresh] [print_thresh]
 *   file1, file2 - Input files to compare
 *   sig_thresh   - difference threshold for counting significant errors (default: 0.05)
 *   crit_thresh  - difference threshold for interruption (default: 10.0)
 *   print_thresh - difference threshold for printing table (default: 1.0)
 */

#include "uband_diff.h"

#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

// Forward declarations
struct ProgramArgs {
  std::string file1 = "file1.txt";
  std::string file2 = "file2.txt";
  double count_level = 0.05;
  double stop_level = 10.0;
  double print_level = 1.0;
  int debug_level = 0;
};

bool show_help_if_requested(int argc, char* argv[]);
bool validate_argument_count(int argc, char* argv[]);
bool parse_file_arguments(int argc, char* argv[], ProgramArgs& args);
bool parse_numeric_arguments(int argc, char* argv[], ProgramArgs& args);
bool parse_threshold_argument(const char* arg, double& value,
                              const std::string& name);
bool parse_debug_level_argument(const char* arg, int& value);

bool show_help_if_requested(int argc, char* argv[]) {
  if (argc >= 2 &&
      (std::string(argv[1]) == "-h" || std::string(argv[1]) == "--help" ||
       std::string(argv[1]) == "help")) {
    std::cout << "uband_diff - Numerical File Comparison Tool\n" << std::endl;
    std::cout << "USAGE:" << std::endl;
    std::cout << "  " << argv[0]
              << " <file1> <file2> [threshold] [hard_threshold] [print_level] "
                 "[debug_level]"
              << std::endl;
    std::cout << "\nARGUMENTS:" << std::endl;
    std::cout << "  file1           First input file to compare" << std::endl;
    std::cout << "  file2           Second input file to compare" << std::endl;
    std::cout << "  threshold       Soft difference threshold for counting "
                 "differences"
              << std::endl;
    std::cout << "                  (default: 0.05, must be ≥ 0)" << std::endl;
    std::cout
        << "  hard_threshold  Hard difference threshold for failure detection"
        << std::endl;
    std::cout << "                  (default: 10.0, must be ≥ 0, typically > "
                 "threshold)"
              << std::endl;
    std::cout << "  print_level     Print verbosity level for difference table"
              << std::endl;
    std::cout << "                  (default: 1.0, must be ≥ 0)" << std::endl;
    std::cout << "  debug_level     Debug output level" << std::endl;
    std::cout << "                  (default: 0, typically 0-3)" << std::endl;
    std::cout << "\nEXAMPLES:" << std::endl;
    std::cout << "  " << argv[0] << " data1.txt data2.txt" << std::endl;
    std::cout << "  " << argv[0] << " file1.dat file2.dat 0.01" << std::endl;
    std::cout << "  " << argv[0] << " test1.txt test2.txt 0.05 1.0 0.1 2"
              << std::endl;
    std::cout << "\nFEATURES:" << std::endl;
    std::cout << "  - Precision-aware numerical comparison" << std::endl;
    std::cout << "  - Complex number support" << std::endl;
    std::cout << "  - Configurable difference thresholds" << std::endl;
    std::cout << "  - Detailed difference reporting" << std::endl;
    return true;
  }
  return false;
}

bool validate_argument_count(int argc, char* argv[]) {
  if (argc < 3) {
    std::cout << "\033[1;33mWARNING:\033[0m Two file names not provided."
              << std::endl;
    std::cout << "   Usage: " << argv[0]
              << " <file1> <file2> [threshold] [hard_threshold] [print_level] "
                 "[debug_level]"
              << std::endl;
    std::cout << "   Use '" << argv[0]
              << " --help' for detailed usage information." << std::endl;
    return false;
  }

  if (argc > 8) {
    std::cerr << "\033[1;31mERROR:\033[0m Too many arguments provided."
              << std::endl;
    std::cerr << "Usage: " << argv[0]
              << " <file1> <file2> [threshold] [hard_threshold] [print_level] "
                 "[debug_level]"
              << std::endl;
    std::cerr << "Use '" << argv[0]
              << " --help' for detailed usage information." << std::endl;
    return false;
  }
  return true;
}

bool parse_file_arguments(int argc, char* argv[], ProgramArgs& args) {
  if (argc >= 3) {
    args.file1 = argv[1];
    args.file2 = argv[2];

    // Validate file arguments
    if (args.file1.empty()) {
      std::cerr << "\033[1;31mERROR:\033[0m First file name cannot be empty."
                << std::endl;
      return false;
    }
    if (args.file2.empty()) {
      std::cerr << "\033[1;31mERROR:\033[0m Second file name cannot be empty."
                << std::endl;
      return false;
    }
    if (args.file1 == args.file2) {
      std::cerr << "\033[1;33mWARNING:\033[0m Both files have the same name: '"
                << args.file1 << "'" << std::endl;
      std::cerr << "         This will compare the file with itself."
                << std::endl;
    }

    // Check if files exist and are readable
    std::ifstream test_file1(args.file1);
    std::ifstream test_file2(args.file2);

    if (!test_file1.good()) {
      std::cerr << "\033[1;33mWARNING:\033[0m Cannot access first file: '"
                << args.file1 << "'" << std::endl;
      std::cerr << "         The file may not exist or is not readable."
                << std::endl;
      std::cerr << "         Will attempt to proceed (error will be reported "
                   "by comparator)."
                << std::endl;
    }
    if (!test_file2.good()) {
      std::cerr << "\033[1;33mWARNING:\033[0m Cannot access second file: '"
                << args.file2 << "'" << std::endl;
      std::cerr << "         The file may not exist or is not readable."
                << std::endl;
      std::cerr << "         Will attempt to proceed (error will be reported "
                   "by comparator)."
                << std::endl;
    }
  }
  return true;
}

bool parse_threshold_argument(const char* arg, double& value,
                              const std::string& name) {
  try {
    value = std::stof(arg);
    if (value < 0) {
      std::cerr << "\n\033[1;31mERROR:\033[0m " << name
                << " must be non-negative." << std::endl;
      std::cerr << "       Got: " << arg << " (parsed as " << value << ")"
                << std::endl;
      std::cerr << "       Valid range: [0, ∞)" << std::endl;
      return false;
    }
    return true;
  } catch (const std::invalid_argument&) {
    std::cerr << "\n\033[1;31mERROR:\033[0m Invalid " << name << " format."
              << std::endl;
    std::cerr << "       Expected: floating-point number (e.g., 0.05, 1.5, 10)"
              << std::endl;
    std::cerr << "       Got: '" << arg << "'" << std::endl;
    return false;
  } catch (const std::out_of_range&) {
    std::cerr << "\n\033[1;31mERROR:\033[0m " << name << " value out of range."
              << std::endl;
    std::cerr << "       Got: '" << arg << "'" << std::endl;
    return false;
  }
}

bool parse_debug_level_argument(const char* arg, int& value) {
  try {
    value = std::stoi(arg);
    if (value < -1) {
      std::cerr << "\n\033[1;31mERROR:\033[0m Debug level must be greater than "
                   "or equal to -1."
                << std::endl;
      std::cerr << "       Got: " << arg << " (parsed as " << value << ")"
                << std::endl;
      std::cerr << "       Valid range: [-1, 10] (typical values: 0-3)"
                << std::endl;
      return false;
    }
    if (value > 10) {
      std::cerr << "\n\033[1;33mWARNING:\033[0m Debug level (" << value
                << ") is unusually high." << std::endl;
      std::cerr << "         Typical range: [0, 3]. Proceeding anyway..."
                << std::endl;
    }
    return true;
  } catch (const std::invalid_argument&) {
    std::cerr << "\n\033[1;31mERROR:\033[0m Invalid debug level format."
              << std::endl;
    std::cerr << "       Expected: integer (e.g., 0, 1, 2, 3)" << std::endl;
    std::cerr << "       Got: '" << arg << "'" << std::endl;
    return false;
  } catch (const std::out_of_range&) {
    std::cerr << "\n\033[1;31mERROR:\033[0m Debug level value out of range."
              << std::endl;
    std::cerr << "       Got: '" << arg << "'" << std::endl;
    return false;
  }
}

bool parse_numeric_arguments(int argc, char* argv[], ProgramArgs& args) {
  // Parse threshold arguments
  if (argc >= 4 &&
      !parse_threshold_argument(argv[3], args.count_level, "Diff threshold")) {
    return false;
  }

  if (argc >= 5) {
    if (!parse_threshold_argument(argv[4], args.stop_level, "High threshold")) {
      return false;
    }
    if (args.stop_level < args.count_level) {
      std::cerr << "\033[1;33mWARNING:\033[0m Critical threshold ("
                << "\033[1;31m" << args.stop_level << "\033[0m"
                << ") is less than significant threshold ("
                << "\033[1;36m" << args.count_level << "\033[0m"
                << ")." << std::endl;
      std::cerr << "         Difference table will not be printed."
                << std::endl;
    }
  }

  if (argc >= 6 &&
      !parse_threshold_argument(argv[5], args.print_level, "Print threshold")) {
    return false;
  }

  if (argc >= 7 && !parse_debug_level_argument(argv[6], args.debug_level)) {
    return false;
  }
  return true;
}

int main(int argc, char* argv[]) {
  // Handle help request first
  if (show_help_if_requested(argc, argv)) {
    return 0;
  }

  // Validate argument count
  if (!validate_argument_count(argc, argv)) {
    return argc < 3 ? 0 : 1;  // Warning for too few, error for too many
  }

#ifdef DEBUG
  std::cout << "Debug mode is ON" << std::endl;
  std::cout << "SOURCE: " << __FILE__ << std::endl;
  std::cout << "ARGUMENTS: argc = " << argc << std::endl;
  for (int i = 0; i < argc; i++) {
    std::cout << "   argv[" << i << "]: " << argv[i] << std::endl;
  }
#endif

#ifdef DEBUG2
  std::cout << "Debug mode 2 is ON" << std::endl;
#endif

  // Parse all arguments
  ProgramArgs args;
  if (!parse_file_arguments(argc, argv, args) ||
      !parse_numeric_arguments(argc, argv, args)) {
    return 1;
  }

#ifdef DEBUG
  std::cout << "   File1: " << args.file1 << std::endl;
  std::cout << "   File2: " << args.file2 << std::endl;
#endif

  // Create comparator and run comparison
  FileComparator comparator(args.count_level, args.stop_level, args.print_level,
                            args.debug_level);
  bool result = comparator.compare_files(args.file1, args.file2);
  comparator.print_summary(args.file1, args.file2, argc, argv);

  // Check for errors and return appropriate exit code
  if (comparator.getFlag().error_found) {
    std::cout << "   \033[1;31mError found.\033[0m" << std::endl;
    return 1;
  }

  if (args.debug_level > 0) {
    std::cout << "   Close enough flag: " << std::boolalpha
              << comparator.getFlag().files_are_close_enough << std::endl;
  }

  if (result) {
    return 0;
  } else {
    if (args.debug_level >= 0) {
      std::cout << "\033[1;31mFiles differ significantly.\033[0m" << std::endl;

      if (comparator.getFlag().files_are_close_enough) {
        std::cout << "\033[1;33mFiles are probably close enough (within "
                     "tolerance).\033[0m"
                  << std::endl;
        return 0;
      }
    }
    return 1;
  }
}
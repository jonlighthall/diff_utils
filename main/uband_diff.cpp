#include "uband_diff.h"

#include <iostream>
#include <iomanip>
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
  float count_level = 0.05;
  float hard_level = 10;
  float print_level = 1;  // print level for differences

#ifdef DEBUG2
  std::cout << "Debug mode 2 is ON" << std::endl;
#endif

#ifdef DEBUG
  std::cout << "Debug mode is ON" << std::endl;
  std::cout << "ARGUMENTS: " << std::endl;
  std::cout << "   argc   : " << argc << std::endl;
  for (int i = 0; i < argc; i++) {
    std::cout << "   argv[" << i << "]: " << argv[i] << std::endl;
  }
#endif

  std::cout << "SETTINGS: " << std::endl;
  // Check if the user provided file names as arguments
  if (argc >= 3) {
    file1 = argv[1];  // first file name
    file2 = argv[2];  // second file name
  } else {
    std::cout << "   Using default file names:" << std::endl;
  }
  std::cout << "   File1: " << file1 << std::endl;
  std::cout << "   File2: " << file2 << std::endl;

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

  isERROR = false;
  FileComparator comparator(count_level, hard_level);
  if (comparator.compareFiles(file1, file2)) {
    std::cout << "Files " << file1 << " and " << file2 << " are identical"
              << std::endl;
  } else {
    std::cout << "File1: " << file1 << std::endl;
    std::cout << "File2: " << file2 << std::endl;
    if (isERROR) {
      std::cout << "\033[1;31mError found.\033[0m" << std::endl;
    } else {
      std::cout << "\033[1;31mFiles are different.\033[0m" << std::endl;
      return 1;
    }
  }

  return 0;
}

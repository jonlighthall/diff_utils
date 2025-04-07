#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef DEBUG2  // print status of every line
#ifndef DEBUG
#define DEBUG
#endif
#endif

bool isERROR = false;

bool compareFiles(const std::string& file1, const std::string& file2,
                  double threshold, double hard_threshold) {
  std::ifstream infile1(file1);
  std::ifstream infile2(file2);

  if (!infile1.is_open()) {
    std::cerr << "\033[1;31mError opening file: " << file1 << "\033[0m"
              << std::endl;
    isERROR = true;
    return false;
  }
  if (!infile2.is_open()) {
    std::cerr << "\033[1;31mError opening file: " << file2 << "\033[0m"
              << std::endl;
    isERROR = true;
    return false;
  }

  std::string line1, line2;
  int lineNumber = 0;
  int elemNumber = 0;

  int count = 0;
  double max_diff = 0;
  double diff = 0;
  bool is_same = false;

  const double max_TL = -20 * log10(pow(2, -23));
  std::cout << "Max TL: " << max_TL << std::endl;

  // read the files line by line
  while (std::getline(infile1, line1) && std::getline(infile2, line2)) {
    // increment the line number
    lineNumber++;
    // create a string stream for each line
    std::istringstream stream1(line1);
    std::istringstream stream2(line2);

    // create vectors to store the values in each line
    std::vector<double> values1, values2;
    double value;
    char ch;

    // check if the numbers are complex and read them accordingly
    while (stream1 >> ch) {
      // check if string starts with '('
      if (ch == '(') {
        double real, imag;
        char comma, closeParen;
        // read the complex number in the format (real,imag)
        stream1 >> real >> comma >> imag >> closeParen;
        // check if the complex number is in the correct format
        if (comma == ',' && closeParen == ')') {
          // store the real and imaginary parts in the vector
          values1.push_back(real);
          values1.push_back(imag);
        } else {
          std::cerr << "Error reading complex number in file1 at line "
                    << lineNumber << std::endl;
          isERROR = true;
          return false;
        }
      } else {
        // put the character back to the stream
        stream1.putback(ch);
        // read the value
        stream1 >> value;
        // store the value in the vector
        values1.push_back(value);
      }
    }  // end check complex 1

    while (stream2 >> ch) {
      // check if string starts with '('
      if (ch == '(') {
        double real, imag;
        char comma, closeParen;
        // read the complex number in the format (real,imag)
        stream2 >> real >> comma >> imag >> closeParen;
        // check if the complex number is in the correct format
        if (comma == ',' && closeParen == ')') {
          // store the real and imaginary parts in the vector
          values2.push_back(real);
          values2.push_back(imag);
        } else {
          std::cerr << "Error reading complex number in file2 at line "
                    << lineNumber << std::endl;
          isERROR = true;
          return false;
        }
      } else {
        // put the character back to the stream
        stream2.putback(ch);
        // read the value
        stream2 >> value;
        // store the value in the vector
        values2.push_back(value);
      }
    }  // end check complex 2

    // check if both lines have the same number of columns
    if (values1.size() != values2.size()) {
      std::cerr << "Line " << lineNumber << " has different number of columns!"
                << std::endl;
      return false;
    } else {
#ifdef DEBUG2
      std::cout << "Line " << lineNumber << " has " << values1.size()
                << " columns." << std::endl;
#endif
    }  // end check number of columns

    // loop over columns
    for (size_t i = 0; i < values1.size(); ++i) {
      // compare values
      diff = std::abs(values1[i] - values2[i]);
      if (diff > max_diff) {
        max_diff = diff;
      }
      if (diff > threshold) {
        if (count == 0) {
          is_same = false;
          // print table header on first difference
          std::cout << " line col      tl1      tl2 |    diff" << std::endl;
        }
        count++;
        if (count < 10) {
          std::cout << std::fixed << std::setprecision(3) << std::setw(5)
                    << lineNumber << "  " << std::setw(2) << i + 1 << "  "
                    << std::setw(7) << values1[i] << "  " << std::setw(7)
                    << values2[i] << " | " << std::setw(7) << diff << std::endl;
        }
      }
      if ((diff > hard_threshold) && (values1[i] >= max_TL)) {
        is_same = false;
        std::cerr << "\033[1;31mDifference found at line " << lineNumber
                  << ", column " << i + 1 << "\033[0m" << std::endl;

        if (lineNumber > 0) {
          std::cout << "   First " << lineNumber - 1 << " lines match"
                    << std::endl;
        }
        if (elemNumber > 0) {
          std::cout << "   " << elemNumber << " element";
          if (elemNumber > 1) std::cout << "s";
          std::cout << " checked" << std::endl;
        }
        std::cout << count << " with differnces between " << threshold
                  << " and " << hard_threshold << std::endl;

        std::cout << "   File1: " << std::setw(7) << values1[i] << std::endl;
        std::cout << "   File2: " << std::setw(7) << values2[i] << std::endl;
        std::cout << "    diff: \033[1;31m" << std::setw(7) << diff << "\033[0m"
                  << std::endl;
        return false;
      } else {
        elemNumber++;
#ifdef DEBUG2
        std::cout << "   Values at line " << lineNumber << ", column " << i + 1
                  << " are equal: " << values1[i] << std::endl;
#endif
      }
    }  // end check values in line
  }  // end read in file

  // check if both files have the same number of lines
  int linesFile1 = 0, linesFile2 = 0;
  std::ifstream countFile1(file1), countFile2(file2);
  std::string temp;

  while (std::getline(countFile1, temp)) linesFile1++;
  while (std::getline(countFile2, temp)) linesFile2++;
  if (linesFile1 != linesFile2) {
    std::cerr << "\033[1;31mFiles have different number of lines!\033[0m"
              << std::endl;
    if (lineNumber != linesFile1 || lineNumber != linesFile2) {
      std::cout << "   First " << lineNumber << " lines match" << std::endl;
      std::cout << "   " << elemNumber << " elements checked" << std::endl;
    }
    std::cerr << "   File1 has " << linesFile1 << " lines " << std::endl;
    std::cerr << "   File2 has " << linesFile2 << " lines " << std::endl;

    return false;
  } else {
#ifdef DEBUG
    std::cout << "Files have the same number of lines: " << linesFile1
              << std::endl;
    std::cout << "Files have the same number of elements: " << elemNumber
              << std::endl;
#endif
  }

  if (count > 0) {
    std::cout << "Total differences: " << count << " less than "
              << hard_threshold << std::endl;
  } else {
    is_same = true;
  }
  std::cout << "Max difference: " << max_diff << std::endl;

  return is_same;
}

int main(int argc, char* argv[]) {
  std::string file1 = "file1.txt";
  std::string file2 = "file2.txt";

  std::cout << "argc: " << argc << std::endl;
  for (int i = 0; i < argc; i++) {
    std::cout << "argv[" << i << "]: " << argv[i] << std::endl;

    /* code */
  }

  if (argc >= 3) {
    file1 = argv[1];
    file2 = argv[2];
  } else {
    std::cout << "Using default file names:" << std::endl;
  }
  float count_level = 0;
  float hard_level = 1;

  if (argc >= 4) {
    count_level = std::stof(argv[3]);
  } else {
    count_level = 0.05;
    std::cout << "Using default threshold: " << count_level << std::endl;
  }

  if (argc == 5) {
    hard_level = std::stof(argv[4]);
  } else {
    hard_level = 1;
    std::cout << "Using default high threshold: " << count_level << std::endl;
  }

  isERROR = false;
  if (compareFiles(file1, file2, count_level, hard_level)) {
    std::cout << "Files " << file1 << " and " << file2 << " are identical"
              << std::endl;
  } else {
    std::cout << "File1: " << file1 << std::endl;
    std::cout << "File2: " << file2 << std::endl;
    if (isERROR) {
      std::cout << "\033[1;31mError found.\033[0m" << std::endl;
    } else {
      std::cout << "\033[1;31mFiles are different.\033[0m" << std::endl;
    }
  }

  return 0;
}

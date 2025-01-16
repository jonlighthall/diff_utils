#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

bool compareFiles(const std::string& file1, const std::string& file2) {
  std::ifstream infile1(file1);
  std::ifstream infile2(file2);

  if (!infile1.is_open() || !infile2.is_open()) {
    std::cerr << "Error opening files!" << std::endl;
    return false;
  }

  std::string line1, line2;
  int lineNumber = 0;

  while (std::getline(infile1, line1) && std::getline(infile2, line2)) {
    lineNumber++;
    std::istringstream stream1(line1);
    std::istringstream stream2(line2);

    std::vector<double> values1, values2;
    double value;
    char ch;

    while (stream1 >> ch) {
      if (ch == '(') {
        double real, imag;
        char comma, closeParen;
        stream1 >> real >> comma >> imag >> closeParen;
        if (comma == ',' && closeParen == ')') {
          values1.push_back(real);
          values1.push_back(imag);
        } else {
          std::cerr << "Error reading complex number in file1 at line "
                    << lineNumber << std::endl;
          return false;
        }
      } else {
        stream1.putback(ch);
        stream1 >> value;
        values1.push_back(value);
      }
    }

    while (stream2 >> ch) {
      if (ch == '(') {
        double real, imag;
        char comma, closeParen;
        stream2 >> real >> comma >> imag >> closeParen;
        if (comma == ',' && closeParen == ')') {
          values2.push_back(real);
          values2.push_back(imag);
        } else {
          std::cerr << "Error reading complex number in file2 at line "
                    << lineNumber << std::endl;
          return false;
        }
      } else {
        stream2.putback(ch);
        stream2 >> value;
        values2.push_back(value);
      }
    }

    if (values1.size() != values2.size()) {
      std::cerr << "Line " << lineNumber << " has different number of columns!"
                << std::endl;
      return false;
    } else {
#ifdef DEBUG
      std::cout << "Line " << lineNumber << " has " << values1.size()
                << " columns." << std::endl;
#endif
    }

    for (size_t i = 0; i < values1.size(); ++i) {
      if (values1[i] != values2[i]) {
        std::cerr << "Difference found at line " << lineNumber << ", column "
                  << i + 1 << std::endl;

        std::cout << "   File1: " << values1[i] << std::endl;
        std::cout << "   File2: " << values2[i] << std::endl;
        return false;
      } else {
#ifdef DEBUG
        std::cout << "   Values at line " << lineNumber << ", column " << i + 1
                  << " are equal: " << values1[i] << std::endl;
#endif
      }
    }
  }

  int linesFile1 = 0, linesFile2 = 0;
  std::ifstream countFile1(file1), countFile2(file2);
  std::string temp;

  while (std::getline(countFile1, temp)) linesFile1++;
  while (std::getline(countFile2, temp)) linesFile2++;
#ifdef DEBUG
  std::cerr << "File1 has " << linesFile1 << " lines " << std::endl;
  std::cerr << "File2 has " << linesFile2 << " lines " << std::endl;
#endif
  if (linesFile1 != linesFile2) {
    std::cerr << "Files have different number of lines!" << std::endl;
    return false;
  } else {
#ifdef DEBUG
    std::cout << "Files have the same number of lines: " << linesFile1
              << std::endl;
#endif
  }

  return true;
}

int main(int argc, char* argv[]) {
  std::string file1 = "file1.txt";
  std::string file2 = "file2.txt";

  if (argc == 3) {
    file1 = argv[1];
    file2 = argv[2];
  } else {
    std::cout << "Using default file names:" << std::endl;
  }
  std::cout << "File1: " << file1 << std::endl;
  std::cout << "File2: " << file2 << std::endl;

  if (compareFiles(file1, file2)) {
    std::cout << "Files are identical." << std::endl;
  } else {
    std::cout << "Files are different." << std::endl;
  }

  return 0;
}
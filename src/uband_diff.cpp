#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#ifdef DEBUG3  // print each step of the process
#ifndef DEBUG2
#define DEBUG2
#endif
#endif

#ifdef DEBUG2  // print status of every line
#ifndef DEBUG
#define DEBUG
#endif
#endif

bool isERROR = false;

// Function to count decimal places in a string token
auto stream_countDecimalPlaces = [](std::istringstream& stream) {
  // Peek ahead to determine the format of the number
  std::streampos pos = stream.tellg();
  std::string token;
  stream >> token;

  int ndp = 0;
  if (size_t decimal_pos = token.find('.'); decimal_pos != std::string::npos) {
    ndp = static_cast<int>(token.size() - decimal_pos - 1);
  }
#ifdef DEBUG3
  std::cout << "DEBUG3: " << token << " has ";
  std::cout << ndp << " decimal places" << std::endl;
#endif
  // Reset stream to before reading token, so the original extraction
  // works
  stream.clear();
  stream.seekg(pos);

  return ndp;
};

// readComplex() returns the real and imagaginary parts of the complex
// number as separate values
// Note: This assumes the complex number is in the format (real, imag)
// where real and imag are floating point numbers, a comma is used as the
// separator, and the complex number is enclosed in parentheses.
// The function reads the complex number from the stream and returns a
// pair containing the real and imaginary parts as doubles. It assumes the
// leading '(' has already been read from the stream.)
// e.g. (1.0, 2.0)
// or (1.0, 2.0) with spaces around the comma
// or (1.0, 2.0) without spaces
// or (1.0,2.0)  with no spaces at all

auto readComplex =
    [](std::istringstream& stream) -> std::tuple<double, double, int, int> {
  // values
  double real;
  double imag;
  // dummy characters to read the complex number format
  char comma;
  char closeParen;
  stream >> real >> comma >> imag >> closeParen;
  if (comma == ',' && closeParen == ')') {
    // Save the current position (just after '(')
    std::streampos start_pos = stream.tellg();

    // Read the real part as a string to count decimal places
    stream.seekg(start_pos);
    std::string real_token;
    stream >> real_token;
    std::istringstream real_stream(real_token);
    int real_dp = stream_countDecimalPlaces(real_stream);

    // Move to the comma after reading real part
    stream.seekg(start_pos);
    double dummy_real;
    stream >> dummy_real >> comma;
    std::streampos imag_pos = stream.tellg();

    // Read the imag part as a string to count decimal places
    stream.seekg(imag_pos);
    std::string imag_token;
    stream >> imag_token;
    std::istringstream imag_stream(imag_token);
    int imag_dp = stream_countDecimalPlaces(imag_stream);

    // Optionally, you can use real_dp and imag_dp as needed here

    return {real, imag, real_dp, imag_dp};
  } else {
    std::cerr << "Error reading complex number";
    std::exit(EXIT_FAILURE);
  }
};

auto round_to_decimals = [](double value, int precision) {
  double scale = std::pow(10.0, precision);
  return std::round(value * scale) / scale;
};

bool compareFiles(const std::string& file1, const std::string& file2,
                  double threshold, double hard_threshold) {
  // assign files to input file streams
  std::ifstream infile1(file1);
  std::ifstream infile2(file2);

  // check if the files are open
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

  // individual line conents
  std::string line1;
  std::string line2;
  // number of lines read
  int lineNumber = 0;
  // number of elements checked
  int elemNumber = 0;

  // number of differences found (where the difference is greater than
  // threshold, which is the minimum between the function argument
  // threshold and the minimum difference between the two files, based on format
  // precision)
  int count = 0;
  // track the maximum difference found
  double max_diff = 0;
  double max_diff_rounded = 0;

  // track if the file format has changed
  long unsigned int prev_n_col = 0;
  // bool new_fmt = false;

  // track if the files are the same
  bool is_same = false;

  // define the maximum valid value for TL (Transmission Loss)
  const double max_TL = -20 * log10(pow(2, -23));
  std::cout << "Max TL: " << max_TL << std::endl;

  // read the files line by line
  while (std::getline(infile1, line1) && std::getline(infile2, line2)) {
    // increment the line number
    lineNumber++;

    // create a string stream for each line
    std::istringstream stream1(line1);
    std::istringstream stream2(line2);

    // variables to store the values while reading from the stream
    double value;
    char ch;

    // create vectors to store the numerical values in each line
    std::vector<double> values1;
    std::vector<double> values2;

    // vector to store the number of decimal places
    std::vector<int> dp;
    std::vector<int> dp_per_col;

    // read in values from file1
    while (stream1 >> ch) {
      // check if the numbers are complex and read them accordingly
      // check if string starts with '('
      if (ch == '(') {
        // read the complex number
        auto [real, imag, dp_real, dp_imag] = readComplex(stream1);
        values1.push_back(real);
        values1.push_back(imag);
        dp.push_back(dp_real);
        dp.push_back(dp_imag);

#ifdef DEBUG3
        std::cout << "DEBUG3: Found complex number in file1 at line "
                  << lineNumber << std::endl;
        std::cout << "DEBUG3: Real part: " << real
                  << ", Imaginary part: " << imag << std::endl;
        std::cout << "DEBUG3: Decimal places - Real: " << dp_real
                  << ", Imaginary: " << dp_imag << std::endl;
#endif

      } else {
        // if the character is not '(', it is a number
        // put the character back to the stream
        stream1.putback(ch);

        // count the number of decimal places
        int dp1 = stream_countDecimalPlaces(stream1);

        // store the number of decimal places
        dp.push_back(dp1);
#ifdef DEBUG3
        std::cout << "DEBUG3: dp1 = " << dp1 << std::endl;
        std::cout << "DEBUG3: dp vector: ";
        for (const auto& d : dp) {
          std::cout << d << " ";
        }
        std::cout << std::endl;
#endif

        // read the value
        stream1 >> value;
        // store the value in the vector
        values1.push_back(value);
      }  // end check complex 1
    }  // end read in file 1

    // read in values from file2
    while (stream2 >> ch) {
      // check if string starts with '('
      if (ch == '(') {
        auto [real, imag, dp_real, dp_imag] = readComplex(stream2);
        values2.push_back(real);
        values2.push_back(imag);
        dp.push_back(dp_real);
        dp.push_back(dp_imag);

// DEBUG output
#ifdef DEBUG3
        std::cout << "DEBUG3: Found complex number in file2 at line "
                  << lineNumber << std::endl;
        std::cout << "DEBUG3: Real part: " << real
                  << ", Imaginary part: " << imag << std::endl;
        std::cout << "DEBUG3: Decimal places - Real: " << dp_real
                  << ", Imaginary: " << dp_imag << std::endl;
#endif

      } else {
        // put the character back to the stream
        stream2.putback(ch);

        // count the number of decimal places
        int dp2 = stream_countDecimalPlaces(stream2);

        // store the number of decimal places
        dp.push_back(dp2);

#ifdef DEBUG3
        std::cout << "DEBUG3: dp2 = " << dp2 << std::endl;
        std::cout << "DEBUG3: dp vector: ";
        for (const auto& d : dp) {
          std::cout << d << " ";
        }
        std::cout << std::endl;
#endif

        // read the value
        stream2 >> value;
        // store the value in the vector
        values2.push_back(value);
      }
    }  // end check complex 2

    // get the numbers of columns in each file
    long unsigned int n_col1 = values1.size();
    long unsigned int n_col2 = values2.size();

    // print parsed file contents
#ifdef DEBUG2
    std::cout << "DEBUG2: Line " << lineNumber;
    std::cout << " file1: ";
    for (size_t i = 0; i < n_col1; ++i) {
      std::cout << values1[i] << "(" << dp[i] << ") ";
    }

    std::cout << ", file2: ";
    for (size_t i = 0; i < n_col2; ++i) {
      std::cout << values2[i] << "(" << dp[n_col1 + i] << ") ";
    }
    std::cout << std::endl;
#endif

    // check if both lines have the same number of columns
    if (n_col1 != n_col2) {
      std::cerr << "Line " << lineNumber << " has different number of columns!"
                << std::endl;
      return false;
    } else {
      // check if the number of columns has changed
      // if it is not the first line, compare with the previous number of
      // columns and print a warning if it has changed
      if (lineNumber == 1) {
        prev_n_col = n_col1;  // initialize prev_n_col on first line
#ifdef DEBUG2
        std::cout << "DEBUG2: Line " << lineNumber << " has " << n_col1
                  << " columns in both files." << std::endl;
#endif
      }
      if (prev_n_col > 0 && n_col1 != prev_n_col) {
        std::cerr << "\033[1;31mNote: Number of columns changed at line "
                  << lineNumber << " (previous: " << prev_n_col
                  << ", current: " << n_col1 << ")\033[0m" << std::endl;
        //  dp_per_col.clear();
        //  new_fmt = true;  // set new_fmt to true if the number of columns
        // has changed
      }
      prev_n_col = n_col1;
    }  // end check number of columns

    // loop over columns
    for (size_t i = 0; i < n_col1; ++i) {
      // compare values (without rounding)
      double diff = std::abs(values1[i] - values2[i]);
      if (diff > max_diff) {
        max_diff = diff;
      }

      // get the number of decimal places for each column
      int dp1 = dp[i];
      int dp2 = dp[n_col1 + i];
      if (dp1 < 0 || dp2 < 0) {
        std::cerr << "Line " << lineNumber
                  << " has negative number of decimal places!" << std::endl;
        isERROR = true;
        return false;
      }

      // Calculate minimum difference
      int min_dp = std::min(dp1, dp2);

      //   if (lineNumber == 1) {
      //     // initialize the dp_per_col vector with the minimum decimal places
      //     dp_per_col.push_back(min_dp);
      //   }

      //   else if (dp_per_col[i] != min_dp) {
      //     // if the number of decimal places has changed, update the vector
      //     dp_per_col[i] = min_dp;
      //     new_fmt = true;  // set new_fmt to true if the number of decimal
      //     places
      //                      // has changed
      //   }

      // check if the number of decimal places is the same
      if (dp1 != dp2) {
#ifdef DEBUG
        std::cout << "DEBUG : Line " << lineNumber << ", Column " << i + 1;
        std::cout << " number of decimal places dp1: " << dp1
                  << "  dp2: " << dp2 << std::endl;
#endif
      }

      double rounded1 = round_to_decimals(values1[i], min_dp);
      double rounded2 = round_to_decimals(values2[i], min_dp);

      double diff_rounded = std::abs(rounded1 - rounded2);
      if (diff_rounded > max_diff_rounded) {
        max_diff_rounded = diff_rounded;
      }

      // determine the comparison threshold
      double dp_threshold = std::pow(10, -min_dp);

      //   bool new_dp = false;

      //   if (dp_per_col[i] == min_dp) {
      //   } else {
      //     std::cout << "Line " << lineNumber << ", Column " << i + 1
      //               << " has a minimum difference of 10^(" << -min_dp
      //               << ") = " << dp_threshold << std::endl;

      //     dp_per_col[i] = min_dp;
      //     new_dp = true;
      //   }

      double ithreshold;

      if (threshold < dp_threshold) {
        // If the threshold is less than the minimum difference, use the
        // minimum difference as the threshold
        ithreshold = dp_threshold;
        // if (new_dp) {
        //   std::cout << "\033[1;33mLine " << lineNumber << ", Column " << i +
        //   1
        //             << " has a minimum difference of " << dp_threshold
        //             << " which is greater than the specified threshold: "
        //             << threshold << "\033[0m" << std::endl;
        // }
      } else {
        // If the threshold is greater than the minimum difference, use the
        // threshold as the threshold
        ithreshold = threshold;
      }

      if ((diff_rounded > hard_threshold) &&
          ((values1[i] <= max_TL) || (values2[i] <= max_TL))) {
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
        std::cout << count << " with differences between " << threshold
                  << " and " << hard_threshold << std::endl;

        std::cout << "   File1: " << std::setw(7) << rounded1 << std::endl;
        std::cout << "   File2: " << std::setw(7) << rounded2 << std::endl;
        std::cout << "    diff: \033[1;31m" << std::setw(7) << diff_rounded
                  << "\033[0m" << std::endl;
        return false;
      }

      if (diff_rounded > ithreshold) {
        std::cout << "Line " << lineNumber << ", Column " << i + 1
                  << " exceeds threshold: " << ithreshold << std::endl;

        if (count == 0) {
          is_same = false;
          // print table header on first difference
          std::cout << " line col    range   tl1      tl2    |    diff"
                    << std::endl;
        }
        count++;
        if (count < 10) {
          std::cout << std::fixed << std::setprecision(3) << std::setw(5)
                    << lineNumber << "  " << std::setw(2) << i + 1 << "  "
                    << std::setw(7) << values1[0] << "  " << std::setw(7)
                    << rounded1 << "  " << std::setw(7) << rounded2 << " | "
                    << std::setw(7) << diff_rounded << " (" << min_dp << ")"
                    << std::endl;
        }
      } else {
        elemNumber++;
        std::cout << "   Values at line " << lineNumber << ", column " << i + 1
                  << " are equal: " << rounded1 << std::endl;
      }

    }  // end check values in line
  }  // end read in file

  // check if both files have the same number of lines
  int linesFile1 = 0;
  int linesFile2 = 0;
  std::ifstream countFile1(file1);
  std::ifstream countFile2(file2);
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

#ifdef DEBUG2
  std::cout << "Debug mode 2 is ON" << std::endl;
#endif

#ifdef DEBUG
  std::cout << "Debug mode is ON" << std::endl;
  std::cout << "argc: " << argc << std::endl;
  for (int i = 0; i < argc; i++) {
    std::cout << "argv[" << i << "]: " << argv[i] << std::endl;

    /* code */
  }
#endif

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
    count_level = static_cast<float>(0.05);
    std::cout << "Using default threshold: " << count_level << std::endl;
  }

  if (argc == 5) {
    hard_level = std::stof(argv[4]);
  } else {
    hard_level = 1;
    std::cout << "Using default high threshold: " << hard_level << std::endl;
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
      return 1;
    }
  }

  return 0;
}

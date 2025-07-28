#include "file_reader.h"
#include <iostream>

// ========================================================================
// File Operations
// ========================================================================

bool FileReader::open_files(const std::string& file1, const std::string& file2,
                            std::ifstream& infile1, std::ifstream& infile2) const {
    infile1.open(file1);
    infile2.open(file2);

    if (!infile1.is_open()) {
        std::cerr << "\033[1;31mError opening file: " << file1 << "\033[0m" << std::endl;
        error_found = true;
        return false;
    }
    if (!infile2.is_open()) {
        std::cerr << "\033[1;31mError opening file: " << file2 << "\033[0m" << std::endl;
        error_found = true;
        return false;
    }
    return true;
}

size_t FileReader::get_file_length(const std::string& file) const {
    std::ifstream infile(file);
    if (!infile.is_open()) {
        std::cerr << "\033[1;31mError opening file: " << file << "\033[0m" << std::endl;
        error_found = true;
        return 0;
    }
    size_t length = 0;
    std::string line;
    while (std::getline(infile, line)) {
        ++length;
    }
    return length;
}

bool FileReader::compare_file_lengths(const std::string& file1,
                                      const std::string& file2) const {
    size_t length1 = get_file_length(file1);

    if (size_t length2 = get_file_length(file2); length1 != length2) {
        std::cerr << "\033[1;31mFiles have different lengths: " << file1 << " ("
                  << length1 << " lines) and " << file2 << " (" << length2
                  << " lines)\033[0m" << std::endl;
        std::cerr << "\033[1;31mFiles have different number of lines!\033[0m" << std::endl;
        std::cerr << "   File1 has " << length1 << " lines " << std::endl;
        std::cerr << "   File2 has " << length2 << " lines " << std::endl;
        return false;
    }
    return true;
}

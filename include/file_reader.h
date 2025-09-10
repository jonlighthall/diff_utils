/** 
 * @author J. Lighthall
 * @date July 2025
 * Refactored from include/uband_diff.h
 */ 
 
#ifndef FILE_READER_H
#define FILE_READER_H

#include <fstream>
#include <string>

/**
 * @brief Handles file I/O operations for the file comparison utility
 *
 * This class is responsible for:
 * - Opening and validating files
 * - Reading file contents line by line
 * - Determining file lengths
 * - Managing file streams
 */
class FileReader {
public:
    FileReader() = default;
    ~FileReader() = default;
  // ========================================================================
  // File Operations
  // ========================================================================
    bool open_files(const std::string& file1, const std::string& file2,
                    std::ifstream& infile1, std::ifstream& infile2) const;

    size_t get_file_length(const std::string& file) const;

    bool compare_file_lengths(const std::string& file1,
                              const std::string& file2) const;

private:
    mutable bool error_found = false;
};

#endif // FILE_READER_H

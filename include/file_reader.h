/**
 * @author J. Lighthall
 * @date July 2025
 * Refactored from include/uband_diff.h
 */

#ifndef FILE_READER_H
#define FILE_READER_H

#include <fstream>
#include <string>
#include <vector>

/**
 * @brief Structure to hold column format information for a file
 */
struct ColumnGroup {
  size_t start_line;  // First line where this format appears
  size_t
      end_line;  // Last line where this format appears (0 = continues to end)
  size_t column_count;  // Number of columns in this group
  bool is_header;       // True if this appears to be a header section
};

/**
 * @brief Structure to hold complete column structure analysis
 */
struct ColumnStructure {
  std::vector<ColumnGroup> groups;  // Different column format groups
  size_t total_lines;               // Total number of lines analyzed
  size_t data_start_line;           // Line where consistent data format begins
  bool has_headers;                 // True if header lines detected
  bool is_monotonic_first_column;   // True if first column increases
                                    // monotonically
  std::string structure_summary;    // Human-readable summary
};

/**
 * @brief Handles file I/O operations for the file comparison utility
 *
 * This class is responsible for:
 * - Opening and validating files
 * - Reading file contents line by line
 * - Determining file lengths
 * - Managing file streams
 * - Analyzing column structure and format
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

  // ========================================================================
  // Column Structure Analysis
  // ========================================================================
  ColumnStructure analyze_column_structure(const std::string& filename) const;

  bool compare_column_structures(const std::string& file1,
                                 const std::string& file2) const;

  void print_column_structure(const ColumnStructure& structure,
                              const std::string& filename) const;

 private:
  mutable bool error_found = false;

  // Helper methods for column structure analysis
  size_t count_columns_in_line(const std::string& line) const;
  bool is_first_column_monotonic(const std::string& filename) const;
  std::string generate_structure_summary(
      const ColumnStructure& structure) const;
};

#endif  // FILE_READER_H

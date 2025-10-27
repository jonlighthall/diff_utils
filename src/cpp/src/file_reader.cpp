/**
 * @author J. Lighthall
 * @date July 2025
 * Refactored from src/uband_diff.cpp
 */

#include "file_reader.h"

#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>

#include "uband_diff.h"  // for PrintLevel definition

// ========================================================================
// File Operations
// ========================================================================

bool FileReader::open_files(const std::string& file1, const std::string& file2,
                            std::ifstream& infile1,
                            std::ifstream& infile2) const {
  infile1.open(file1);
  infile2.open(file2);

  if (!infile1.is_open()) {
    std::cerr << "\033[1;31mError opening file: " << file1 << "\033[0m"
              << std::endl;
    error_found = true;
    return false;
  }
  if (!infile2.is_open()) {
    std::cerr << "\033[1;31mError opening file: " << file2 << "\033[0m"
              << std::endl;
    error_found = true;
    return false;
  }
  return true;
}

size_t FileReader::get_file_length(const std::string& file) const {
  std::ifstream infile(file);
  if (!infile.is_open()) {
    std::cerr << "\033[1;31mError opening file: " << file << "\033[0m"
              << std::endl;
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
    std::cerr << "\033[1;31mFiles have different number of lines!\033[0m"
              << std::endl;
    std::cerr << "   File1 has " << length1 << " lines " << std::endl;
    std::cerr << "   File2 has " << length2 << " lines " << std::endl;
    return false;
  }
  return true;
}

// ========================================================================
// Column Structure Analysis
// ========================================================================

size_t FileReader::count_columns_in_line(const std::string& line) const {
  std::istringstream stream(line);
  std::string token;
  size_t count = 0;

  while (stream >> token) {
    count++;
  }

  return count;
}

bool FileReader::is_first_column_monotonic(const std::string& filename) const {
  std::ifstream infile(filename);
  if (!infile.is_open()) {
    return false;
  }

  std::string line;
  double prev_value = std::numeric_limits<double>::lowest();
  bool first_value = true;

  while (std::getline(infile, line)) {
    if (line.empty()) continue;

    std::istringstream stream(line);
    double current_value;

    if (stream >> current_value) {
      if (!first_value && current_value < prev_value) {
        return false;
      }
      prev_value = current_value;
      first_value = false;
    }
  }

  return true;
}

bool FileReader::is_first_column_fixed_delta(
    const std::string& filename) const {
  std::ifstream infile(filename);
  if (!infile.is_open()) {
    return false;
  }

  std::string line;
  double prev_value = 0.0;
  double first_value_seen = 0.0;
  double expected_delta = 0.0;
  bool first_value = true;
  bool second_value = true;
  const double TOLERANCE = 0.01;  // 1% tolerance for delta consistency
  const double MAX_STARTING_VALUE = 100.0;  // Range data typically starts small
  const double MIN_DELTA = 1e-10;           // Delta must be non-zero

  while (std::getline(infile, line)) {
    if (line.empty()) continue;

    std::istringstream stream(line);
    double current_value;

    if (stream >> current_value) {
      if (first_value) {
        first_value_seen = current_value;
        // If first value is too large, unlikely to be range data
        if (std::abs(first_value_seen) > MAX_STARTING_VALUE) {
          return false;
        }
        first_value = false;
      } else if (second_value) {
        // Calculate the expected delta from first two values
        expected_delta = current_value - prev_value;
        // Delta must be non-zero for range data
        if (std::abs(expected_delta) < MIN_DELTA) {
          return false;
        }
        second_value = false;
      } else {
        // Check if delta is consistent
        double actual_delta = current_value - prev_value;
        double delta_diff = std::abs(actual_delta - expected_delta);
        double relative_error = (std::abs(expected_delta) > 1e-10)
                                    ? delta_diff / std::abs(expected_delta)
                                    : delta_diff;

        if (relative_error > TOLERANCE) {
          return false;
        }
      }
      prev_value = current_value;
    }
  }

  return !second_value;  // Return true only if we had at least 3 values
}

std::string FileReader::generate_structure_summary(
    const ColumnStructure& structure) const {
  std::ostringstream summary;

  if (structure.has_headers) {
    summary << "File has " << structure.groups.size()
            << " column format groups:\n";
    for (size_t i = 0; i < structure.groups.size(); ++i) {
      const auto& group = structure.groups[i];
      if (group.is_header) {
        summary << "  Header (lines " << group.start_line << "-"
                << group.end_line << "): " << group.column_count
                << " columns\n";
      } else {
        summary << "  Data (lines " << group.start_line
                << "+): " << group.column_count << " columns\n";
      }
    }
  } else {
    summary << "File has consistent " << structure.groups[0].column_count
            << " columns throughout\n";
  }

  if (structure.is_monotonic_first_column) {
    summary << "First column is monotonically increasing\n";
  } else {
    summary << "First column is NOT monotonically increasing\n";
  }

  return summary.str();
}

ColumnStructure FileReader::analyze_column_structure(
    const std::string& filename) const {
  ColumnStructure structure;
  structure.total_lines = 0;
  structure.data_start_line = 0;
  structure.has_headers = false;
  structure.is_monotonic_first_column = false;
  structure.is_first_column_fixed_delta = false;
  structure.is_first_column_range_data = false;

  std::ifstream infile(filename);
  if (!infile.is_open()) {
    std::cerr << "\033[1;31mError opening file for structure analysis: "
              << filename << "\033[0m" << std::endl;
    error_found = true;
    return structure;
  }

  std::string line;
  size_t line_number = 1;
  size_t current_column_count = 0;
  std::vector<size_t> column_counts;

  // First pass: collect column counts for each line
  while (std::getline(infile, line)) {
    if (!line.empty()) {
      size_t columns = count_columns_in_line(line);
      column_counts.push_back(columns);
    }
    structure.total_lines++;
    line_number++;
  }

  if (column_counts.empty()) {
    return structure;
  }

  // Analyze column count patterns
  size_t most_common_count =
      column_counts.back();  // Assume data format is at end
  size_t most_common_frequency = 0;

  // Find the most frequent column count (likely the main data format)
  for (size_t target_count : column_counts) {
    size_t frequency = 0;
    for (size_t count : column_counts) {
      if (count == target_count) frequency++;
    }
    if (frequency > most_common_frequency) {
      most_common_frequency = frequency;
      most_common_count = target_count;
    }
  }

  // Build column groups
  current_column_count = 0;
  line_number = 1;

  for (size_t i = 0; i < column_counts.size(); ++i) {
    size_t columns = column_counts[i];

    if (columns != current_column_count) {
      // End previous group if it exists
      if (current_column_count > 0 && !structure.groups.empty()) {
        structure.groups.back().end_line = line_number - 1;
      }

      // Start new group
      ColumnGroup group;
      group.start_line = line_number;
      group.end_line = 0;  // Will be set later or remain 0 for final group
      group.column_count = columns;
      group.is_header = (columns != most_common_count);

      if (group.is_header) {
        structure.has_headers = true;
      } else if (structure.data_start_line == 0) {
        structure.data_start_line = line_number;
      }

      structure.groups.push_back(group);
      current_column_count = columns;
    }

    line_number++;
  }

  // Check if first column is monotonic
  structure.is_monotonic_first_column = is_first_column_monotonic(filename);

  // Check if first column has fixed delta
  structure.is_first_column_fixed_delta = is_first_column_fixed_delta(filename);

  // Determine if first column is likely range data (both monotonic and fixed
  // delta)
  structure.is_first_column_range_data = structure.is_monotonic_first_column &&
                                         structure.is_first_column_fixed_delta;

  // Generate summary
  structure.structure_summary = generate_structure_summary(structure);

  return structure;
}

bool FileReader::compare_column_structures(
    const std::string& file1, const std::string& file2,
    const PrintLevel& print_level) const {
  ColumnStructure struct1 = analyze_column_structure(file1);
  ColumnStructure struct2 = analyze_column_structure(file2);
  if (print_level.debug2) {
    std::cout << "\n\033[1;36m=== Column Structure Analysis ===\033[0m"
              << std::endl;

    std::cout << "\n\033[1;36m=== Column Structure Comparison ===\033[0m\n"
              << std::endl;

    std::cout << "\033[1;34mFile 1 (" << file1 << "):\033[0m\n";
    print_column_structure(struct1, file1);

    std::cout << "\n\033[1;34mFile 2 (" << file2 << "):\033[0m\n";
    print_column_structure(struct2, file2);
  }

  // Compare structures
  bool structures_match = true;
  if (print_level.debug2) {
    std::cout << "\n\033[1;33mStructure Comparison:\033[0m\n";
  }

  if (struct1.groups.size() != struct2.groups.size()) {
    if (print_level.debug2) {
      std::cout << "\033[1;31m❌ Different number of column groups: "
                << struct1.groups.size() << " vs " << struct2.groups.size()
                << "\033[0m\n";
    }
    structures_match = false;
  } else {
    if (print_level.debug2) {
      std::cout << "\033[1;32m✓ Same number of column groups: "
                << struct1.groups.size() << "\033[0m\n";
    }

    // Compare each group
    for (size_t i = 0; i < struct1.groups.size(); ++i) {
      const auto& g1 = struct1.groups[i];
      const auto& g2 = struct2.groups[i];

      if (g1.column_count != g2.column_count) {
        if (print_level.debug2) {
          std::cout << "\033[1;31m❌ Group " << (i + 1)
                    << " column count differs: " << g1.column_count << " vs "
                    << g2.column_count << "\033[0m\n";
        }
        structures_match = false;
      } else {
        if (print_level.debug2) {
          std::cout << "\033[1;32m✓ Group " << (i + 1) << " has matching "
                    << g1.column_count << " columns\033[0m\n";
        }
      }

      if (g1.is_header != g2.is_header) {
        if (print_level.debug2) {
          std::cout << "\033[1;31m❌ Group " << (i + 1)
                    << " header status differs\033[0m\n";
        }
        structures_match = false;
      }
    }
  }

  if (struct1.is_monotonic_first_column != struct2.is_monotonic_first_column) {
    if (print_level.debug2) {
      std::cout << "\033[1;31m❌ First column monotonicity differs\033[0m\n";
    }
    structures_match = false;
  } else {
    if (print_level.debug2) {
      std::cout << "\033[1;32m✓ First column monotonicity matches\033[0m\n";
    }
  }

  if (print_level.debug2) {
    if (structures_match) {
      std::cout << "\n\033[1;32m🎉 Column structures are compatible!\033[0m\n";
    } else {
      std::cout
          << "\n\033[1;31m⚠️  Column structures are NOT compatible!\033[0m\n";
    }
  }

  return structures_match;
}

// Overload that defaults to silent comparison (no debug output)
bool FileReader::compare_column_structures(const std::string& file1,
                                           const std::string& file2) const {
  PrintLevel silent{0, false, false, false, false};
  return compare_column_structures(file1, file2, silent);
}

void FileReader::print_column_structure(const ColumnStructure& structure,
                                        const std::string& filename) const {
  std::cout << "Total lines: " << structure.total_lines << std::endl;

  if (structure.groups.empty()) {
    std::cout << "\033[1;31mNo column structure detected\033[0m" << std::endl;
    return;
  }

  std::cout << "Column groups detected: " << structure.groups.size()
            << std::endl;

  for (size_t i = 0; i < structure.groups.size(); ++i) {
    const auto& group = structure.groups[i];
    std::cout << "  Group " << (i + 1) << ": ";

    if (group.is_header) {
      std::cout << "\033[1;35m[HEADER]\033[0m ";
    } else {
      std::cout << "\033[1;36m[DATA]\033[0m ";
    }

    std::cout << "Lines " << group.start_line;
    if (group.end_line > 0) {
      std::cout << "-" << group.end_line;
    } else {
      std::cout << "+";
    }
    std::cout << " → " << group.column_count << " columns" << std::endl;
  }

  if (structure.data_start_line > 0) {
    std::cout << "Main data starts at line: " << structure.data_start_line
              << std::endl;
  }

  if (structure.is_monotonic_first_column) {
    std::cout << "\033[1;32m✓ First column is monotonically increasing\033[0m"
              << std::endl;
  } else {
    std::cout
        << "\033[1;33m⚠ First column is NOT monotonically increasing\033[0m"
        << std::endl;
  }
}

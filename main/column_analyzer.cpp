/**
 * @file column_analyzer.cpp
 * @brief Standalone utility for analyzing column structure of numerical files
 *
 * This utility can analyze the column structure of one or two files and:
 * - Detect the number of column groups
 * - Identify header lines vs data lines
 * - Check if the first column is monotonically increasing
 * - Compare column structures between two files
 */

#include "file_reader.h"
#include <iostream>
#include <string>

void print_usage(const char* program_name) {
    std::cout << "Usage:\n";
    std::cout << "  " << program_name << " <file>                 # Analyze single file\n";
    std::cout << "  " << program_name << " <file1> <file2>       # Compare two files\n";
    std::cout << "\nThis tool analyzes the column structure of numerical data files.\n";
    std::cout << "It can detect:\n";
    std::cout << "  - Number of columns per line\n";
    std::cout << "  - Header sections with different column counts\n";
    std::cout << "  - Whether the first column increases monotonically\n";
    std::cout << "  - Compatibility between file structures\n";
}

int main(int argc, char* argv[]) {
    if (argc < 2 || argc > 3) {
        print_usage(argv[0]);
        return 1;
    }

    FileReader reader;

    if (argc == 2) {
        // Single file analysis
        std::string filename = argv[1];
        std::cout << "\033[1;36m=== Column Structure Analysis ===\033[0m\n" << std::endl;
        std::cout << "\033[1;34mAnalyzing: " << filename << "\033[0m\n" << std::endl;

        ColumnStructure structure = reader.analyze_column_structure(filename);
        reader.print_column_structure(structure, filename);

        std::cout << "\n\033[1;33mStructure Summary:\033[0m\n";
        std::cout << structure.structure_summary << std::endl;

    } else if (argc == 3) {
        // Two file comparison
        std::string file1 = argv[1];
        std::string file2 = argv[2];

        bool compatible = reader.compare_column_structures(file1, file2);

        return compatible ? 0 : 1;
    }

    return 0;
}

#include <gtest/gtest.h>
#include "uband_diff.h"
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstdio>

class FileComparatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Set up test with default thresholds
        comparator = std::make_unique<FileComparator>(0.05, 10.0, 1.0);

        // Clean up any existing test files
        cleanup_test_files();
    }

    void TearDown() override {
        // Clean up test files after each test
        cleanup_test_files();
    }

    std::unique_ptr<FileComparator> comparator;

    // Helper function to create test files
    void createTestFile(const std::string& filename, const std::vector<std::string>& lines) {
        std::ofstream file(filename);
        for (const auto& line : lines) {
            file << line << std::endl;
        }
        file.close();
    }

    // Helper function to clean up test files
    void cleanup_test_files() {
        std::vector<std::string> test_files = {
            "test_file1.txt", "test_file2.txt",
            "test_identical1.txt", "test_identical2.txt",
            "test_different1.txt", "test_different2.txt",
            "test_complex1.txt", "test_complex2.txt"
        };

        for (const auto& file : test_files) {
            std::remove(file.c_str());
        }
    }
};

// Test parse_line function with simple numbers
TEST_F(FileComparatorTest, ParseLineSimpleNumbers) {
    std::string line = "1.23 4.56";
    LineData result = comparator->parse_line(line);

    ASSERT_EQ(result.values.size(), 2);
    EXPECT_DOUBLE_EQ(result.values[0], 1.23);
    EXPECT_DOUBLE_EQ(result.values[1], 4.56);
    EXPECT_EQ(result.decimal_places[0], 2);
    EXPECT_EQ(result.decimal_places[1], 2);
}

// Test parse_line with different decimal places
TEST_F(FileComparatorTest, ParseLineDifferentPrecision) {
    std::string line = "1.2 3.4567";
    LineData result = comparator->parse_line(line);

    ASSERT_EQ(result.values.size(), 2);
    EXPECT_DOUBLE_EQ(result.values[0], 1.2);
    EXPECT_DOUBLE_EQ(result.values[1], 3.4567);
    EXPECT_EQ(result.decimal_places[0], 1);
    EXPECT_EQ(result.decimal_places[1], 4);
}

// Test parse_line with integers (no decimal point)
TEST_F(FileComparatorTest, ParseLineIntegers) {
    std::string line = "123 456";
    LineData result = comparator->parse_line(line);

    ASSERT_EQ(result.values.size(), 2);
    EXPECT_DOUBLE_EQ(result.values[0], 123.0);
    EXPECT_DOUBLE_EQ(result.values[1], 456.0);
    EXPECT_EQ(result.decimal_places[0], 0);
    EXPECT_EQ(result.decimal_places[1], 0);
}

// Test parse_line with complex numbers
TEST_F(FileComparatorTest, ParseLineComplexNumbers) {
    std::string line = "(1.0, 2.0) (3.5, 4.25)";
    LineData result = comparator->parse_line(line);

    ASSERT_EQ(result.values.size(), 4);
    EXPECT_DOUBLE_EQ(result.values[0], 1.0);  // real part of first complex
    EXPECT_DOUBLE_EQ(result.values[1], 2.0);  // imag part of first complex
    EXPECT_DOUBLE_EQ(result.values[2], 3.5);  // real part of second complex
    EXPECT_DOUBLE_EQ(result.values[3], 4.25); // imag part of second complex

    EXPECT_EQ(result.decimal_places[0], 1);   // real part precision
    EXPECT_EQ(result.decimal_places[1], 1);   // imag part precision
    EXPECT_EQ(result.decimal_places[2], 1);   // real part precision
    EXPECT_EQ(result.decimal_places[3], 2);   // imag part precision
}

// Test comparing identical files
TEST_F(FileComparatorTest, CompareIdenticalFiles) {
    createTestFile("test_identical1.txt", {"1.0 2.0", "3.0 4.0"});
    createTestFile("test_identical2.txt", {"1.0 2.0", "3.0 4.0"});

    bool result = comparator->compare_files("test_identical1.txt", "test_identical2.txt");
    EXPECT_TRUE(result);
    EXPECT_TRUE(comparator->getFlag().files_are_same);
    EXPECT_FALSE(comparator->getFlag().error_found);
}

// Test comparing different files within tolerance
TEST_F(FileComparatorTest, CompareDifferentFilesWithinTolerance) {
    createTestFile("test_different1.txt", {"1.000 2.000"});
    createTestFile("test_different2.txt", {"1.001 2.001"}); // Small differences

    bool result = comparator->compare_files("test_different1.txt", "test_different2.txt");
    EXPECT_TRUE(result);  // Should pass within tolerance
    EXPECT_TRUE(comparator->getFlag().has_non_zero_diff);
    EXPECT_FALSE(comparator->getFlag().has_significant_diff);
}

// Test comparing files with significant differences
TEST_F(FileComparatorTest, CompareDifferentFilesSignificant) {
    createTestFile("test_different1.txt", {"1.0 2.0"});
    createTestFile("test_different2.txt", {"1.1 2.2"}); // Large differences

    bool result = comparator->compare_files("test_different1.txt", "test_different2.txt");
    EXPECT_FALSE(result);
    EXPECT_TRUE(comparator->getFlag().has_significant_diff);
    EXPECT_FALSE(comparator->getFlag().files_are_close_enough);
}

// Test handling non-existent files
TEST_F(FileComparatorTest, HandleNonExistentFile) {
    bool result = comparator->compare_files("nonexistent1.txt", "nonexistent2.txt");
    EXPECT_FALSE(result);
    EXPECT_TRUE(comparator->getFlag().error_found);
}

// Test files with different lengths
TEST_F(FileComparatorTest, HandleDifferentFileLengths) {
    createTestFile("test_different1.txt", {"1.0 2.0", "3.0 4.0"});
    createTestFile("test_different2.txt", {"1.0 2.0"}); // One line shorter

    bool result = comparator->compare_files("test_different1.txt", "test_different2.txt");
    EXPECT_FALSE(result);
}

// Test files with complex numbers
TEST_F(FileComparatorTest, CompareComplexNumbers) {
    createTestFile("test_complex1.txt", {"(1.0, 2.0) (3.0, 4.0)"});
    createTestFile("test_complex2.txt", {"(1.0, 2.0) (3.0, 4.0)"});

    bool result = comparator->compare_files("test_complex1.txt", "test_complex2.txt");
    EXPECT_TRUE(result);
    EXPECT_TRUE(comparator->getFlag().files_are_same);
}

// Test extract_column_values function
TEST_F(FileComparatorTest, ExtractColumnValues) {
    LineData data1, data2;
    data1.values = {1.23, 4.56, 7.89};
    data1.decimal_places = {2, 2, 2};
    data2.values = {1.24, 4.57, 7.90};
    data2.decimal_places = {2, 2, 2};

    ColumnValues result = comparator->extract_column_values(data1, data2, 1);

    EXPECT_DOUBLE_EQ(result.value1, 4.56);
    EXPECT_DOUBLE_EQ(result.value2, 4.57);
    EXPECT_DOUBLE_EQ(result.range, 1.23); // First value in line
    EXPECT_EQ(result.dp1, 2);
    EXPECT_EQ(result.dp2, 2);
    EXPECT_EQ(result.min_dp, 2);
}

// Test threshold behavior
TEST_F(FileComparatorTest, StrictThresholdBehavior) {
    // Create comparator with very strict threshold
    FileComparator strict_comparator(0.0001, 10.0, 1.0);

    createTestFile("test_different1.txt", {"1.000"});
    createTestFile("test_different2.txt", {"1.001"}); // Small difference

    bool result = strict_comparator.compare_files("test_different1.txt", "test_different2.txt");

    EXPECT_FALSE(result);
    EXPECT_TRUE(strict_comparator.getFlag().has_significant_diff);
}

// Test with empty lines (should handle gracefully)
TEST_F(FileComparatorTest, HandleEmptyLines) {
    // Note: This test may need adjustment based on how your program handles empty lines
    createTestFile("test_different1.txt", {"1.0 2.0", "", "3.0 4.0"});
    createTestFile("test_different2.txt", {"1.0 2.0", "", "3.0 4.0"});

    // The behavior here depends on how your parse_line handles empty strings
    // You may need to adjust expectations based on actual behavior
    bool result = comparator->compare_files("test_different1.txt", "test_different2.txt");

    // This test will help you discover how your program handles edge cases
    std::cout << "Empty line test result: " << result << std::endl;
    std::cout << "Error found: " << comparator->getFlag().error_found << std::endl;
}

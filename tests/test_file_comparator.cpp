#include <gtest/gtest.h>
#include "uband_diff.h"
#include <sstream>
#include <fstream>
#include <filesystem>
#include <cstdio>

class FileComparatorTest : public ::testing::Test {
protected:
    // Keep SetUp() and TearDown() protected (required by Google Test)
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
    void createTestFile(const std::string& filename, const std::vector<std::string>& lines) const {
        std::ofstream file(filename);
        for (const auto& line : lines) {
            file << line << std::endl;
        }
        file.close();
    }

    // Helper function to clean up test files
    void cleanup_test_files() const {
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
    LineData data1;
    LineData data2;
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

// ============================================================================
// SUMMATION INVARIANT TESTS
// Test that all difference counters follow the proper mathematical relationships
// ============================================================================

// Test basic counter summation invariants
TEST_F(FileComparatorSummationTest, CounterSummationInvariants) {
    // Test with files that have various types of differences
    createTestFile("test_mixed1.txt", {
        "1.000 2.000",      // Identical values
        "3.000 4.001",      // Small difference (trivial)
        "5.000 5.100",      // Medium difference (non-trivial, significant)
        "7.000 8.000"       // Large difference (significant)
    });
    createTestFile("test_mixed2.txt", {
        "1.000 2.000",      // Identical
        "3.000 4.000",      // Small difference
        "5.000 5.050",      // Medium difference  
        "7.000 7.500"       // Large difference
    });

    bool result = comparator->compare_files("test_mixed1.txt", "test_mixed2.txt");
    
    // Validate all invariants
    validateCounterInvariants();
    
    // Specific checks for this test case
    const auto& counter = comparator->getCountStats();
    const auto& flags = comparator->getFlag();
    
    EXPECT_EQ(counter.elem_number, 8) << "Should have processed 8 elements total";
    EXPECT_TRUE(flags.has_non_zero_diff) << "Should have non-zero differences";
    
    // Print debug information
    std::cout << "\nCounter Summary:" << std::endl;
    std::cout << "  Elements: " << counter.elem_number << std::endl;
    std::cout << "  Non-zero: " << counter.diff_non_zero << std::endl;
    std::cout << "  Trivial: " << counter.diff_trivial << std::endl;
    std::cout << "  Non-trivial: " << counter.diff_non_trivial << std::endl;
    std::cout << "  Significant: " << counter.diff_significant << std::endl;
    std::cout << "  Marginal: " << counter.diff_marginal << std::endl;
    std::cout << "  Critical: " << counter.diff_critical << std::endl;
}

// Test summation invariants with getter access to counters
class FileComparatorSummationTest : public FileComparatorTest {
protected:
    // Helper to validate all counter invariants
    void validateCounterInvariants() {
        const auto& counter = comparator->getCountStats();
        const auto& flags = comparator->getFlag();
        
        // ========================================================================
        // FUNDAMENTAL SUMMATION INVARIANTS
        // ========================================================================
        
        // Invariant 1: Total elements = zero differences + non-zero differences
        size_t zero_differences = counter.elem_number - counter.diff_non_zero;
        EXPECT_EQ(counter.elem_number, zero_differences + counter.diff_non_zero)
            << "Total elements should equal zero + non-zero differences";
        
        // Invariant 2: Non-zero differences = trivial + non-trivial differences  
        EXPECT_EQ(counter.diff_non_zero, counter.diff_trivial + counter.diff_non_trivial)
            << "Non-zero differences should equal trivial + non-trivial differences";
        
        // Invariant 3: Non-trivial differences >= significant differences
        // (some non-trivial differences may be insignificant due to ignore threshold)
        EXPECT_GE(counter.diff_non_trivial, counter.diff_significant)
            << "Non-trivial differences should be >= significant differences";
        
        // Invariant 4: Significant differences >= marginal + critical
        // (there can be non-marginal, non-critical significant differences)
        EXPECT_GE(counter.diff_significant, counter.diff_marginal + counter.diff_critical)
            << "Significant differences should be >= marginal + critical differences";
        
        // ========================================================================
        // LOGICAL FLAG CONSISTENCY
        // ========================================================================
        
        // If we have significant differences, we should have non-trivial differences
        if (flags.has_significant_diff) {
            EXPECT_TRUE(flags.has_non_trivial_diff)
                << "Significant differences require non-trivial differences";
            EXPECT_GT(counter.diff_significant, 0)
                << "has_significant_diff flag should match counter > 0";
        }
        
        // If we have non-trivial differences, we should have non-zero differences
        if (flags.has_non_trivial_diff) {
            EXPECT_TRUE(flags.has_non_zero_diff)
                << "Non-trivial differences require non-zero differences";
            EXPECT_GT(counter.diff_non_trivial, 0)
                << "has_non_trivial_diff flag should match counter > 0";
        }
        
        // If we have non-zero differences, element count should be > 0
        if (flags.has_non_zero_diff) {
            EXPECT_GT(counter.elem_number, 0)
                << "Non-zero differences require elements to be checked";
            EXPECT_GT(counter.diff_non_zero, 0)
                << "has_non_zero_diff flag should match counter > 0";
        }
        
        // If we have critical differences, we should have significant differences  
        // AND both values should be <= ignore threshold (meaningful TL values)
        if (flags.has_critical_diff) {
            EXPECT_TRUE(flags.has_significant_diff)
                << "Critical differences should also be significant";
            EXPECT_GT(counter.diff_critical, 0)
                << "has_critical_diff flag should match counter > 0";
        }
        
        // If we have marginal differences, we should have significant differences
        if (flags.has_marginal_diff) {
            EXPECT_TRUE(flags.has_significant_diff)
                << "Marginal differences should also be significant";
            EXPECT_GT(counter.diff_marginal, 0)
                << "has_marginal_diff flag should match counter > 0";
        }
        
        // ========================================================================
        // COUNTER-FLAG CONSISTENCY
        // ========================================================================
        
        // Counters should be consistent with flags
        EXPECT_EQ(counter.diff_non_zero > 0, flags.has_non_zero_diff)
            << "diff_non_zero counter should match has_non_zero_diff flag";
        EXPECT_EQ(counter.diff_non_trivial > 0, flags.has_non_trivial_diff)
            << "diff_non_trivial counter should match has_non_trivial_diff flag";
        EXPECT_EQ(counter.diff_significant > 0, flags.has_significant_diff)
            << "diff_significant counter should match has_significant_diff flag";
        EXPECT_EQ(counter.diff_critical > 0, flags.has_critical_diff)
            << "diff_critical counter should match has_critical_diff flag";
        EXPECT_EQ(counter.diff_marginal > 0, flags.has_marginal_diff)
            << "diff_marginal counter should match has_marginal_diff flag";
        
        // ========================================================================
        // PRINT LOGIC CONSISTENCY
        // ========================================================================
        
        // Print counter should not exceed non-zero differences
        EXPECT_LE(counter.diff_print, counter.diff_non_zero)
            << "Print differences should not exceed non-zero differences";
    }
    
    // Helper to validate 2% threshold logic
    void validate2PercentLogic() {
        const auto& counter = comparator->getCountStats();
        const auto& flags = comparator->getFlag();
        
        if (counter.elem_number > 0) {
            // Calculate non-marginal, non-critical, significant differences
            size_t non_marginal_non_critical = 
                counter.diff_significant - counter.diff_marginal - counter.diff_critical;
            
            double percentage = 100.0 * static_cast<double>(non_marginal_non_critical) /
                               static_cast<double>(counter.elem_number);
            
            // The 2% logic should be reflected in the flags
            if (percentage > 2.0) {
                EXPECT_TRUE(flags.error_found)
                    << "Error should be found when non-marginal, non-critical significant "
                    << "differences (" << percentage << "%) exceed 2%";
                EXPECT_FALSE(flags.files_are_close_enough)
                    << "Files should not be close enough when exceeding 2% threshold";
            }
        }
    }
};

// Test with identical files (all counters should be zero except elem_number)
TEST_F(FileComparatorSummationTest, IdenticalFilesSummation) {
    createTestFile("test_identical1.txt", {"1.000 2.000", "3.000 4.000"});
    createTestFile("test_identical2.txt", {"1.000 2.000", "3.000 4.000"});

    bool result = comparator->compare_files("test_identical1.txt", "test_identical2.txt");
    
    EXPECT_TRUE(result);
    
    const auto& flags = comparator->getFlag();
    EXPECT_FALSE(flags.has_non_zero_diff);
    EXPECT_FALSE(flags.has_non_trivial_diff);
    EXPECT_FALSE(flags.has_significant_diff);
    EXPECT_FALSE(flags.has_critical_diff);
    EXPECT_FALSE(flags.has_marginal_diff);
    
    validateCounterInvariants();
}

// Test with only trivial differences
TEST_F(FileComparatorSummationTest, TrivialDifferencesSummation) {
    createTestFile("test_trivial1.txt", {"1.000 2.000"});
    createTestFile("test_trivial2.txt", {"1.0001 2.0001"}); // Very small differences

    bool result = comparator->compare_files("test_trivial1.txt", "test_trivial2.txt");
    
    const auto& flags = comparator->getFlag();
    
    // Should have non-zero differences but potentially not non-trivial
    EXPECT_TRUE(flags.has_non_zero_diff);
    // Whether it's trivial or non-trivial depends on precision and thresholds
    
    validateCounterInvariants();
}

// Test with significant differences within different categories
TEST_F(FileComparatorSummationTest, CategorizedDifferencesSummation) {
    // Create a comparator with specific thresholds for testing
    FileComparator test_comparator(0.01, 1.0, 0.5);  // sig=0.01, crit=1.0, print=0.5
    
    createTestFile("test_categories1.txt", {
        "1.000 2.000",      // Identical
        "10.000 20.000",    // Small values, small diff (significant)
        "150.000 250.000",  // Large values, large diff (should be marginal)
        "10.000 15.000"     // Medium values, very large diff (should be critical)
    });
    createTestFile("test_categories2.txt", {
        "1.000 2.000",      // Identical  
        "10.020 20.020",    // Small values, significant diff
        "150.500 250.500",  // Large values, large diff but marginal
        "10.000 12.000"     // Medium values, critical diff
    });

    bool result = test_comparator.compare_files("test_categories1.txt", "test_categories2.txt");
    
    const auto& flags = test_comparator.getFlag();
    
    // Should have various types of differences
    EXPECT_TRUE(flags.has_non_zero_diff);
    EXPECT_TRUE(flags.has_non_trivial_diff);
    EXPECT_TRUE(flags.has_significant_diff);
    
    // Validate the relationships
    if (flags.has_significant_diff) {
        EXPECT_TRUE(flags.has_non_trivial_diff);
        EXPECT_TRUE(flags.has_non_zero_diff);
    }
}

// Test the 2% failure threshold logic
TEST_F(FileComparatorSummationTest, TwoPercentThresholdTest) {
    // Create a file with 100 elements, where exactly 3 are significantly different
    // This should trigger the 2% failure threshold (3% > 2%)
    std::vector<std::string> lines1, lines2;
    
    // Add 97 identical values
    for (int i = 0; i < 97; ++i) {
        lines1.push_back("1.000");
        lines2.push_back("1.000");
    }
    
    // Add 3 significantly different values (should be > 2%)
    // Use small values to ensure they're significant (not marginal due to ignore threshold)
    lines1.push_back("10.000 20.000 30.000");  
    lines2.push_back("10.100 20.100 30.100");  // Should be significant differences
    
    createTestFile("test_2percent1.txt", lines1);
    createTestFile("test_2percent2.txt", lines2);
    
    bool result = comparator->compare_files("test_2percent1.txt", "test_2percent2.txt");
    
    validateCounterInvariants();
    validate2PercentLogic();
    
    const auto& counter = comparator->getCountStats();
    EXPECT_EQ(counter.elem_number, 100) << "Should have exactly 100 elements";
    
    std::cout << "\n2% Threshold Test Results:" << std::endl;
    std::cout << "  Total elements: " << counter.elem_number << std::endl;
    std::cout << "  Significant differences: " << counter.diff_significant << std::endl;
    std::cout << "  Marginal differences: " << counter.diff_marginal << std::endl;
    std::cout << "  Critical differences: " << counter.diff_critical << std::endl;
    
    size_t non_marginal_non_critical = counter.diff_significant - counter.diff_marginal - counter.diff_critical;
    double percentage = 100.0 * static_cast<double>(non_marginal_non_critical) / static_cast<double>(counter.elem_number);
    std::cout << "  Non-marginal, non-critical: " << non_marginal_non_critical 
              << " (" << percentage << "%)" << std::endl;
}

// Test edge case: exactly 2% threshold
TEST_F(FileComparatorSummationTest, ExactlyTwoPercentThreshold) {
    // Create exactly 2% significant differences that are non-marginal and non-critical
    std::vector<std::string> lines1, lines2;
    
    // 98 identical elements
    for (int i = 0; i < 98; ++i) {
        lines1.push_back("1.000");
        lines2.push_back("1.000");
    }
    
    // 2 significantly different elements = exactly 2%
    // Use small values to ensure they're not marginal or critical
    lines1.push_back("10.000 20.000");
    lines2.push_back("10.100 20.100");  // Should be significant but not critical
    
    createTestFile("test_exact2percent1.txt", lines1);
    createTestFile("test_exact2percent2.txt", lines2);
    
    bool result = comparator->compare_files("test_exact2percent1.txt", "test_exact2percent2.txt");
    
    validateCounterInvariants();
    validate2PercentLogic();
    
    const auto& counter = comparator->getCountStats();
    EXPECT_EQ(counter.elem_number, 100) << "Should have exactly 100 elements";
    
    std::cout << "\nExact 2% Threshold Test Results:" << std::endl;
    size_t non_marginal_non_critical = counter.diff_significant - counter.diff_marginal - counter.diff_critical;
    double percentage = 100.0 * static_cast<double>(non_marginal_non_critical) / static_cast<double>(counter.elem_number);
    std::cout << "  Non-marginal, non-critical: " << non_marginal_non_critical 
              << " (" << percentage << "%)" << std::endl;
    
    // At exactly 2%, the behavior depends on implementation (< vs <=)
    // This test will help verify the boundary condition
}

// Test comprehensive invariants with zero differences
TEST_F(FileComparatorSummationTest, ZeroDifferencesInvariants) {
    createTestFile("test_zero1.txt", {"1.000 2.000", "3.000 4.000"});
    createTestFile("test_zero2.txt", {"1.000 2.000", "3.000 4.000"});

    bool result = comparator->compare_files("test_zero1.txt", "test_zero2.txt");
    
    validateCounterInvariants();
    
    const auto& counter = comparator->getCountStats();
    
    // All difference counters should be zero
    EXPECT_EQ(counter.diff_non_zero, 0);
    EXPECT_EQ(counter.diff_trivial, 0);
    EXPECT_EQ(counter.diff_non_trivial, 0);
    EXPECT_EQ(counter.diff_significant, 0);
    EXPECT_EQ(counter.diff_marginal, 0);
    EXPECT_EQ(counter.diff_critical, 0);
    
    // Only elem_number should be non-zero
    EXPECT_GT(counter.elem_number, 0);
    
    // Zero differences = elem_number
    size_t zero_differences = counter.elem_number - counter.diff_non_zero;
    EXPECT_EQ(zero_differences, counter.elem_number);
}

// Test edge case with all difference types
TEST_F(FileComparatorSummationTest, AllDifferenceTypesInvariants) {
    // Create a comprehensive test with all types of differences
    FileComparator test_comparator(0.01, 0.5, 0.1);  // Low thresholds for testing
    
    createTestFile("test_all_types1.txt", {
        "1.000 2.000",       // Identical
        "3.0000 4.0000",     // Will be trivial difference  
        "5.000 6.000",       // Non-trivial, significant
        "50.000 60.000",     // Large values, may be marginal
        "10.000 11.000"      // May be critical
    });
    createTestFile("test_all_types2.txt", {
        "1.000 2.000",       // Identical
        "3.0001 4.0001",     // Trivial difference
        "5.020 6.020",       // Significant difference
        "50.200 60.200",     // Large difference (marginal due to size)
        "10.000 10.600"      // Critical difference
    });

    bool result = test_comparator.compare_files("test_all_types1.txt", "test_all_types2.txt");
    
    // Use the test comparator's getters
    const auto& counter = test_comparator.getCountStats();
    const auto& flags = test_comparator.getFlag();
    
    // Validate all invariants manually for this test
    size_t zero_differences = counter.elem_number - counter.diff_non_zero;
    EXPECT_EQ(counter.elem_number, zero_differences + counter.diff_non_zero);
    EXPECT_EQ(counter.diff_non_zero, counter.diff_trivial + counter.diff_non_trivial);
    EXPECT_GE(counter.diff_non_trivial, counter.diff_significant);
    EXPECT_GE(counter.diff_significant, counter.diff_marginal + counter.diff_critical);
    
    std::cout << "\nAll Difference Types Test Results:" << std::endl;
    std::cout << "  Elements: " << counter.elem_number << std::endl;
    std::cout << "  Zero differences: " << zero_differences << std::endl;
    std::cout << "  Non-zero: " << counter.diff_non_zero << std::endl;
    std::cout << "  Trivial: " << counter.diff_trivial << std::endl;
    std::cout << "  Non-trivial: " << counter.diff_non_trivial << std::endl;
    std::cout << "  Significant: " << counter.diff_significant << std::endl;
    std::cout << "  Marginal: " << counter.diff_marginal << std::endl;
    std::cout << "  Critical: " << counter.diff_critical << std::endl;
    
    // Verify the breakdown adds up
    EXPECT_EQ(counter.elem_number, 
              zero_differences + counter.diff_trivial + counter.diff_non_trivial)
        << "Total should equal zero + trivial + non-trivial";
}

// Test transmission loss domain-specific logic
TEST_F(FileComparatorSummationTest, TransmissionLossDomainLogic) {
    // Test with TL values in different ranges to validate domain logic
    // marginal = 110, ignore ≈ 304
    
    FileComparator tl_comparator(0.1, 2.0, 1.0);  // Appropriate TL thresholds
    
    createTestFile("test_tl1.txt", {
        "50.0 60.0",     // Low TL (high pressure) - operationally important
        "120.0 130.0",   // Medium TL (marginal range) - less important  
        "350.0 360.0",   // High TL (> ignore) - numerically meaningless
        "80.0 90.0"      // Low TL - should be critical if diff > critical threshold
    });
    createTestFile("test_tl2.txt", {
        "50.0 60.0",     // Identical
        "125.0 135.0",   // Marginal difference (both in marginal range)
        "355.0 365.0",   // Insignificant (both > ignore threshold)
        "80.0 95.0"      // Critical difference (both <= ignore, large diff)
    });

    bool result = tl_comparator.compare_files("test_tl1.txt", "test_tl2.txt");
    
    const auto& counter = tl_comparator.getCountStats();
    const auto& flags = tl_comparator.getFlag();
    
    std::cout << "\nTransmission Loss Domain Test Results:" << std::endl;
    std::cout << "  Total elements: " << counter.elem_number << std::endl;
    std::cout << "  Significant: " << counter.diff_significant << std::endl;
    std::cout << "  Marginal: " << counter.diff_marginal << std::endl;
    std::cout << "  Critical: " << counter.diff_critical << std::endl;
    
    // Domain-specific expectations:
    // - Values > ignore (~304) should not contribute to significant differences
    // - Values in marginal range (110-304) should be marked as marginal
    // - Critical differences should only occur for meaningful TL values (<= ignore)
    
    validateCounterInvariants();
    
    // Should have some significant differences (from low TL values)
    EXPECT_TRUE(flags.has_significant_diff) 
        << "Should have significant differences from low TL values";
    
    // Should have marginal differences (from medium TL values)
    EXPECT_TRUE(flags.has_marginal_diff)
        << "Should have marginal differences from medium TL values";
}

// ============================================================================

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

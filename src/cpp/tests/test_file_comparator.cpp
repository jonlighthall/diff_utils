#include <gtest/gtest.h>

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "uband_diff.h"

class FileComparatorTest : public ::testing::Test {
 protected:
  // Directory for temporary test files
  static constexpr const char* TEST_DIR = "../../build/test/";

  // Keep SetUp() and TearDown() protected (required by Google Test)
  void SetUp() override {
    // Create test directory if it doesn't exist
    std::filesystem::create_directories(TEST_DIR);

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

  // Helper function to create test files in the test directory
  void createTestFile(const std::string& filename,
                      const std::vector<std::string>& lines) const {
    std::string filepath = std::string(TEST_DIR) + filename;
    std::ofstream file(filepath);
    for (const auto& line : lines) {
      file << line << std::endl;
    }
    file.close();
  }

  // Helper function to get full path for test file
  std::string getTestFilePath(const std::string& filename) const {
    return std::string(TEST_DIR) + filename;
  }

  // Helper function to clean up test files
  void cleanup_test_files() const {
    std::vector<std::string> test_files = {
        "test_file1.txt",      "test_file2.txt",      "test_identical1.txt",
        "test_identical2.txt", "test_different1.txt", "test_different2.txt",
        "test_complex1.txt",   "test_complex2.txt",   "test_6level1.txt",
        "test_6level2.txt"};

    for (const auto& file : test_files) {
      std::string filepath = std::string(TEST_DIR) + file;
      std::remove(filepath.c_str());
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
  EXPECT_DOUBLE_EQ(result.values[0], 1.0);   // real part of first complex
  EXPECT_DOUBLE_EQ(result.values[1], 2.0);   // imag part of first complex
  EXPECT_DOUBLE_EQ(result.values[2], 3.5);   // real part of second complex
  EXPECT_DOUBLE_EQ(result.values[3], 4.25);  // imag part of second complex

  EXPECT_EQ(result.decimal_places[0], 1);  // real part precision
  EXPECT_EQ(result.decimal_places[1], 1);  // imag part precision
  EXPECT_EQ(result.decimal_places[2], 1);  // real part precision
  EXPECT_EQ(result.decimal_places[3], 2);  // imag part precision
}

// Test comparing identical files
TEST_F(FileComparatorTest, CompareIdenticalFiles) {
  createTestFile("test_identical1.txt", {"1.0 2.0", "3.0 4.0"});
  createTestFile("test_identical2.txt", {"1.0 2.0", "3.0 4.0"});

  bool result =
      comparator->compare_files(getTestFilePath("test_identical1.txt"),
                                getTestFilePath("test_identical2.txt"));
  EXPECT_TRUE(result);
  EXPECT_TRUE(comparator->getFlag().files_are_same);
  EXPECT_FALSE(comparator->getFlag().error_found);
}

// Test comparing different files within tolerance
TEST_F(FileComparatorTest, CompareDifferentFilesWithinTolerance) {
  createTestFile("test_different1.txt", {"1.000 2.000"});
  createTestFile("test_different2.txt", {"1.001 2.001"});  // Small differences

  bool result =
      comparator->compare_files(getTestFilePath("test_different1.txt"),
                                getTestFilePath("test_different2.txt"));
  EXPECT_TRUE(result);  // Should pass within tolerance
  EXPECT_TRUE(comparator->getFlag().has_non_zero_diff);
  EXPECT_FALSE(comparator->getFlag().has_significant_diff);
}

// Test comparing files with significant differences
TEST_F(FileComparatorTest, CompareDifferentFilesSignificant) {
  createTestFile("test_different1.txt", {"1.0 2.0"});
  createTestFile("test_different2.txt",
                 {"1.5 2.8"});  // Clearly significant differences (0.5 and 0.8)

  bool result =
      comparator->compare_files(getTestFilePath("test_different1.txt"),
                                getTestFilePath("test_different2.txt"));
  EXPECT_FALSE(result);
  EXPECT_TRUE(comparator->getFlag().has_significant_diff);
  EXPECT_FALSE(comparator->getFlag().files_are_close_enough);
}

// Test handling non-existent files
TEST_F(FileComparatorTest, HandleNonExistentFile) {
  bool result =
      comparator->compare_files("nonexistent1.txt", "nonexistent2.txt");
  EXPECT_FALSE(result);
  EXPECT_TRUE(comparator->getFlag().error_found);
}

// Test files with different lengths
TEST_F(FileComparatorTest, HandleDifferentFileLengths) {
  createTestFile("test_different1.txt", {"1.0 2.0", "3.0 4.0"});
  createTestFile("test_different2.txt", {"1.0 2.0"});  // One line shorter

  bool result =
      comparator->compare_files(getTestFilePath("test_different1.txt"),
                                getTestFilePath("test_different2.txt"));
  EXPECT_FALSE(result);
}

// Test files with complex numbers
TEST_F(FileComparatorTest, CompareComplexNumbers) {
  createTestFile("test_complex1.txt", {"(1.0, 2.0) (3.0, 4.0)"});
  createTestFile("test_complex2.txt", {"(1.0, 2.0) (3.0, 4.0)"});

  bool result = comparator->compare_files(getTestFilePath("test_complex1.txt"),
                                          getTestFilePath("test_complex2.txt"));
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
  EXPECT_DOUBLE_EQ(result.range, 1.23);  // First value in line
  EXPECT_EQ(result.dp1, 2);
  EXPECT_EQ(result.dp2, 2);
  EXPECT_EQ(result.min_dp, 2);
}

// Test threshold behavior
TEST_F(FileComparatorTest, StrictThresholdBehavior) {
  // Create comparator with very strict threshold
  FileComparator strict_comparator(0.0001, 10.0, 1.0);

  createTestFile("test_different1.txt", {"1.000"});
  createTestFile("test_different2.txt",
                 {"1.005"});  // Difference of 0.005 > 0.001 threshold

  bool result =
      strict_comparator.compare_files(getTestFilePath("test_different1.txt"),
                                      getTestFilePath("test_different2.txt"));

  EXPECT_FALSE(result);
  EXPECT_TRUE(strict_comparator.getFlag().has_significant_diff);
}

// ============================================================================
// SUMMATION INVARIANT TESTS
// Test that all difference counters follow the proper mathematical
// relationships
// ============================================================================

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
    EXPECT_EQ(counter.diff_non_zero,
              counter.diff_trivial + counter.diff_non_trivial)
        << "Non-zero differences should equal trivial + non-trivial "
           "differences";

    // Invariant 3: Non-trivial differences >= significant differences
    // (some non-trivial differences may be insignificant due to ignore
    // threshold)
    EXPECT_GE(counter.diff_non_trivial, counter.diff_significant)
        << "Non-trivial differences should be >= significant differences";

    // Invariant 4: Significant differences >= marginal + critical
    // (there can be non-marginal, non-critical significant differences)
    EXPECT_GE(counter.diff_significant,
              counter.diff_marginal + counter.diff_critical)
        << "Significant differences should be >= marginal + critical "
           "differences";

    // ========================================================================
    // LOGICAL FLAG CONSISTENCY
    // ========================================================================

    // If we have significant differences, we should have non-trivial
    // differences
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

    // LEVEL 5 counter-flag consistency
    EXPECT_EQ(counter.diff_error > 0, flags.has_error_diff)
        << "diff_error counter should match has_error_diff flag";
    EXPECT_EQ(counter.diff_non_error > 0, flags.has_non_error_diff)
        << "diff_non_error counter should match has_non_error_diff flag";

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
      size_t non_marginal_non_critical = counter.diff_significant -
                                         counter.diff_marginal -
                                         counter.diff_critical;

      double percentage = 100.0 *
                          static_cast<double>(non_marginal_non_critical) /
                          static_cast<double>(counter.elem_number);

      // The 2% logic should be reflected in the flags
      if (percentage > 2.0) {
        EXPECT_TRUE(flags.error_found)
            << "Error should be found when non-marginal, non-critical "
               "significant "
            << "differences (" << percentage << "%) exceed 2%";
        EXPECT_FALSE(flags.files_are_close_enough)
            << "Files should not be close enough when exceeding 2% threshold";
      }
    }
  }
};

// Test basic counter summation invariants
TEST_F(FileComparatorSummationTest, CounterSummationInvariants) {
  // Test with files that have various types of differences
  createTestFile(
      "test_mixed1.txt",
      {
          "1.000 2.000",  // Identical values
          "3.000 4.001",  // Small difference (trivial)
          "5.000 5.100",  // Medium difference (non-trivial, significant)
          "7.000 8.000"   // Large difference (significant)
      });
  createTestFile("test_mixed2.txt", {
                                        "1.000 2.000",  // Identical
                                        "3.000 4.000",  // Small difference
                                        "5.000 5.050",  // Medium difference
                                        "7.000 7.500"   // Large difference
                                    });

  bool result = comparator->compare_files(getTestFilePath("test_mixed1.txt"),
                                          getTestFilePath("test_mixed2.txt"));

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

// ============================================================================
// TRANSMISSION LOSS DOMAIN-SPECIFIC TESTS
// Test behavior with different TL value ranges and thresholds
// ============================================================================

// Test values that should be ignored (TL > ignore threshold ~138.47)
TEST_F(FileComparatorSummationTest, IgnoreHighTLValues) {
  // Test with TL values above ignore threshold - differences should be ignored
  createTestFile(
      "test_high_tl1.txt",
      {"150.0 200.0 300.0",  // All values > ignore threshold (~138.47)
       "160.0 250.0 350.0"});
  createTestFile(
      "test_high_tl2.txt",
      {"155.0 220.0 330.0",  // Large differences but should be ignored
       "175.0 280.0 380.0"});

  bool result = comparator->compare_files(getTestFilePath("test_high_tl1.txt"),
                                          getTestFilePath("test_high_tl2.txt"));

  validateCounterInvariants();

  const auto& counter = comparator->getCountStats();
  const auto& flags = comparator->getFlag();

  EXPECT_EQ(counter.elem_number, 6) << "Should have processed 6 elements";
  EXPECT_TRUE(flags.has_non_zero_diff) << "Should have non-zero differences";
  EXPECT_TRUE(flags.has_non_trivial_diff)
      << "Should have non-trivial differences";
  EXPECT_FALSE(flags.has_significant_diff)
      << "Should NOT have significant differences (all values > ignore)";
  EXPECT_FALSE(flags.has_marginal_diff)
      << "Should NOT have marginal differences";
  EXPECT_FALSE(flags.has_critical_diff)
      << "Should NOT have critical differences";

  std::cout << "\nHigh TL Values Test (values > ignore ~138.47):" << std::endl;
  std::cout << "  Significant: " << counter.diff_significant << " (expected: 0)"
            << std::endl;
  std::cout << "  All differences should be ignored due to high TL values"
            << std::endl;
}

// Test marginal differences (110 < TL < ignore ~138.47)
TEST_F(FileComparatorSummationTest, MarginalTLDifferences) {
  // Test with TL values in marginal range
  createTestFile("test_marginal1.txt",
                 {"115.0 120.0 130.0"});  // Values in marginal range
  createTestFile("test_marginal2.txt",
                 {"117.0 125.0 135.0"});  // Differences in marginal range

  bool result =
      comparator->compare_files(getTestFilePath("test_marginal1.txt"),
                                getTestFilePath("test_marginal2.txt"));

  validateCounterInvariants();

  const auto& counter = comparator->getCountStats();
  const auto& flags = comparator->getFlag();

  EXPECT_EQ(counter.elem_number, 3) << "Should have processed 3 elements";
  EXPECT_TRUE(flags.has_significant_diff)
      << "Should have significant differences";
  EXPECT_TRUE(flags.has_marginal_diff) << "Should have marginal differences";
  EXPECT_FALSE(flags.has_critical_diff)
      << "Should NOT have critical differences (values in marginal range)";

  std::cout << "\nMarginal TL Values Test (110 < TL < 138.47):" << std::endl;
  std::cout << "  Significant: " << counter.diff_significant << std::endl;
  std::cout << "  Marginal: " << counter.diff_marginal << std::endl;
  std::cout << "  Expected: marginal differences for values in operational "
               "warning range"
            << std::endl;
}

// Test critical differences (both TL values ≤ ignore threshold)
TEST_F(FileComparatorSummationTest, CriticalTLDifferences) {
  // Create comparator with lower critical threshold for testing
  FileComparator critical_comparator(0.05, 2.0, 1.0);  // critical=2.0

  createTestFile("test_critical1.txt",
                 {"50.0"});  // Single low TL value (operationally important)
  createTestFile("test_critical2.txt",
                 {"53.0"});  // Difference > critical threshold (3.0 > 2.0)

  bool result =
      critical_comparator.compare_files(getTestFilePath("test_critical1.txt"),
                                        getTestFilePath("test_critical2.txt"));

  const auto& counter = critical_comparator.getCountStats();
  const auto& flags = critical_comparator.getFlag();

  // Critical differences should cause early exit, so only 1 element processed
  EXPECT_EQ(counter.elem_number, 1)
      << "Should have processed 1 element before critical exit";
  EXPECT_TRUE(flags.has_significant_diff)
      << "Should have significant differences";
  EXPECT_TRUE(flags.has_critical_diff) << "Should have critical differences";
  EXPECT_FALSE(flags.has_marginal_diff)
      << "Should NOT have marginal differences (values < marginal threshold)";
  EXPECT_FALSE(result) << "Should return false due to critical difference";

  std::cout
      << "\nCritical TL Values Test (low TL values with large differences):"
      << std::endl;
  std::cout << "  Significant: " << counter.diff_significant << std::endl;
  std::cout << "  Critical: " << counter.diff_critical << std::endl;
  std::cout << "  Expected: critical differences for operationally important "
               "low TL values"
            << std::endl;
  std::cout << "  Note: Program exits early when critical difference found"
            << std::endl;
}

// Test mixed TL ranges in same comparison
TEST_F(FileComparatorSummationTest, MixedTLRanges) {
  // Mix of low TL (critical range), medium TL (marginal range), and high TL
  // (ignore range)
  createTestFile("test_mixed_tl1.txt",
                 {"50.0",     // Low TL - critical range
                  "120.0",    // Medium TL - marginal range
                  "200.0"});  // High TL - ignore range
  createTestFile("test_mixed_tl2.txt",
                 {"51.0",     // Small difference in critical range
                  "125.0",    // Small difference in marginal range
                  "250.0"});  // Large difference in ignore range

  bool result =
      comparator->compare_files(getTestFilePath("test_mixed_tl1.txt"),
                                getTestFilePath("test_mixed_tl2.txt"));

  validateCounterInvariants();

  const auto& counter = comparator->getCountStats();
  const auto& flags = comparator->getFlag();

  EXPECT_EQ(counter.elem_number, 3) << "Should have processed 3 elements";
  EXPECT_TRUE(flags.has_significant_diff)
      << "Should have significant differences";

  // The high TL difference should be ignored, but low and medium TL should be
  // significant
  std::cout << "\nMixed TL Ranges Test:" << std::endl;
  std::cout << "  Elements: " << counter.elem_number << std::endl;
  std::cout << "  Significant: " << counter.diff_significant
            << " (expected: 2, ignoring high TL)" << std::endl;
  std::cout << "  Marginal: " << counter.diff_marginal << std::endl;
  std::cout << "  Critical: " << counter.diff_critical << std::endl;
}

// Test edge cases around thresholds
TEST_F(FileComparatorSummationTest, ThresholdEdgeCases) {
  // Test values right at threshold boundaries
  double ignore_threshold = 138.47379800543135;
  double marginal_threshold = 110.0;

  createTestFile("test_edge1.txt",
                 {"109.9",    // Just below marginal
                  "110.1",    // Just above marginal
                  "138.0",    // Just below ignore
                  "139.0"});  // Just above ignore
  createTestFile("test_edge2.txt",
                 {"110.1",    // Cross marginal boundary
                  "110.3",    // Within marginal range
                  "138.2",    // Within marginal range
                  "139.2"});  // Both above ignore

  bool result = comparator->compare_files(getTestFilePath("test_edge1.txt"),
                                          getTestFilePath("test_edge2.txt"));

  validateCounterInvariants();

  const auto& counter = comparator->getCountStats();

  std::cout << "\nThreshold Edge Cases Test:" << std::endl;
  std::cout << "  Marginal threshold: " << marginal_threshold << std::endl;
  std::cout << "  Ignore threshold: " << ignore_threshold << std::endl;
  std::cout << "  Elements: " << counter.elem_number << std::endl;
  std::cout << "  Significant: " << counter.diff_significant << std::endl;
  std::cout << "  Marginal: " << counter.diff_marginal << std::endl;
  std::cout << "  Expected behavior:" << std::endl;
  std::cout << "    - Element 1: significant (crosses into marginal range)"
            << std::endl;
  std::cout << "    - Element 2: marginal (both in marginal range)"
            << std::endl;
  std::cout << "    - Element 3: marginal (both in marginal range)"
            << std::endl;
  std::cout << "    - Element 4: ignored (both above ignore threshold)"
            << std::endl;
}

// Test 2% failure threshold with different TL ranges
TEST_F(FileComparatorSummationTest, TwoPercentThresholdWithTLRanges) {
  std::vector<std::string> lines1, lines2;

  // Create 100 elements where most are identical but some have differences in
  // different TL ranges
  for (int i = 0; i < 95; ++i) {
    lines1.push_back("50.0");  // Low TL value - operationally important
    lines2.push_back("50.0");  // Identical
  }

  // Add 3 differences in low TL range (should be significant and count toward
  // 2% threshold)
  lines1.push_back("60.0 70.0 80.0");
  lines2.push_back("60.2 70.2 80.2");  // Small but significant differences

  // Add 2 differences in high TL range (should be ignored and NOT count toward
  // 2% threshold)
  lines1.push_back("200.0 250.0");
  lines2.push_back("220.0 280.0");  // Large differences but in ignore range

  createTestFile("test_2percent_tl1.txt", lines1);
  createTestFile("test_2percent_tl2.txt", lines2);

  bool result =
      comparator->compare_files(getTestFilePath("test_2percent_tl1.txt"),
                                getTestFilePath("test_2percent_tl2.txt"));

  validateCounterInvariants();
  validate2PercentLogic();

  const auto& counter = comparator->getCountStats();
  const auto& flags = comparator->getFlag();

  EXPECT_EQ(counter.elem_number, 100) << "Should have exactly 100 elements";

  // Calculate non-marginal, non-critical significant differences
  size_t non_marginal_non_critical =
      counter.diff_significant - counter.diff_marginal - counter.diff_critical;
  double percentage = 100.0 * static_cast<double>(non_marginal_non_critical) /
                      static_cast<double>(counter.elem_number);

  std::cout << "\n2% Threshold with TL Ranges Test:" << std::endl;
  std::cout << "  Total elements: " << counter.elem_number << std::endl;
  std::cout << "  Total significant: " << counter.diff_significant << std::endl;
  std::cout << "  Non-marginal, non-critical: " << non_marginal_non_critical
            << " (" << percentage << "%)" << std::endl;
  std::cout << "  Expected: Only low TL differences should count (3%), high TL "
               "ignored"
            << std::endl;

  // Should have exactly 3% significant differences (3 out of 100), exceeding 2%
  // threshold
  if (percentage > 2.0) {
    EXPECT_TRUE(flags.error_found) << "Should trigger 2% failure threshold";
  }
}

// ============================================================================
// 6-LEVEL HIERARCHY VALIDATION TESTS
// Test all mathematical relationships in the 6-level counting hierarchy
// ============================================================================

TEST_F(FileComparatorSummationTest, SixLevelHierarchyValidation) {
  // Create test data that exercises all 6 levels of the hierarchy
  std::vector<std::string> lines1 = {
      "   50.0  51.0  52.0  53.0", "  115.0 116.0 117.0 118.0",
      "  140.0 141.0 142.0 143.0", "   60.0  61.0  62.0  63.0"};

  std::vector<std::string> lines2 = {
      "   50.5  51.3  52.8  54.2", "  115.2 116.1 117.5 118.3",
      "  140.0 141.0 142.0 143.0", "   60.0  65.0  62.0  63.0"};

  // Use thresholds that will exercise all levels:
  // user_thresh=0.2, critical_thresh=2.0, print_thresh=0.1
  auto test_comparator = std::make_unique<FileComparator>(0.2, 2.0, 0.1, 0);

  createTestFile("test_6level1.txt", lines1);
  createTestFile("test_6level2.txt", lines2);
  bool result = test_comparator->compare_files(
      getTestFilePath("test_6level1.txt"), getTestFilePath("test_6level2.txt"));

  const auto& counter = test_comparator->getCountStats();

  std::cout << "\n6-Level Hierarchy Validation:" << std::endl;
  std::cout << "  Total elements: " << counter.elem_number << std::endl;
  std::cout << "  Zero differences: "
            << (counter.elem_number - counter.diff_non_zero) << std::endl;
  std::cout << "  Non-zero differences: " << counter.diff_non_zero << std::endl;
  std::cout << "  Trivial differences: " << counter.diff_trivial << std::endl;
  std::cout << "  Non-trivial differences: " << counter.diff_non_trivial
            << std::endl;
  std::cout << "  Insignificant differences: " << counter.diff_insignificant
            << std::endl;
  std::cout << "  Significant differences: " << counter.diff_significant
            << std::endl;
  std::cout << "  Marginal differences: " << counter.diff_marginal << std::endl;
  std::cout << "  Critical differences: " << counter.diff_critical << std::endl;
  std::cout << "  Error differences: " << counter.diff_error << std::endl;
  std::cout << "  Non-error differences: " << counter.diff_non_error
            << std::endl;

  // Calculate derived values
  size_t zero_differences = counter.elem_number - counter.diff_non_zero;
  size_t non_marginal = counter.diff_significant - counter.diff_marginal;
  size_t non_critical =
      non_marginal - counter.diff_critical;  // Only non-marginal non-critical

  // ========================================================================
  // LEVEL 1: total = zero + non_zero (based on raw difference [calculated])
  // ========================================================================
  EXPECT_EQ(counter.elem_number, zero_differences + counter.diff_non_zero)
      << "LEVEL 1: total should equal zero + non_zero differences";

  // ========================================================================
  // LEVEL 2: non_zero = trivial + non_trivial (based on printed precision)
  // ========================================================================
  EXPECT_EQ(counter.diff_non_zero,
            counter.diff_trivial + counter.diff_non_trivial)
      << "LEVEL 2: non_zero should equal trivial + non_trivial differences";

  // Alternative Level 2 validation: total = zero + trivial + non_trivial
  EXPECT_EQ(counter.elem_number,
            zero_differences + counter.diff_trivial + counter.diff_non_trivial)
      << "LEVEL 2: total should equal zero + trivial + non_trivial";

  // ========================================================================
  // LEVEL 3: non_trivial = insignificant + significant (based on ignore
  // threshold ~138)
  // ========================================================================
  EXPECT_EQ(counter.diff_non_trivial,
            counter.diff_insignificant + counter.diff_significant)
      << "LEVEL 3: non_trivial should equal insignificant + significant "
         "differences";

  // Alternative Level 3 validation: total = zero + trivial + insignificant +
  // significant
  EXPECT_EQ(counter.elem_number, zero_differences + counter.diff_trivial +
                                     counter.diff_insignificant +
                                     counter.diff_significant)
      << "LEVEL 3: total should equal zero + trivial + insignificant + "
         "significant";

  // ========================================================================
  // LEVEL 4: significant = marginal + non_marginal (based on marginal threshold
  // 110)
  // ========================================================================
  EXPECT_EQ(counter.diff_significant, counter.diff_marginal + non_marginal)
      << "LEVEL 4: significant should equal marginal + non_marginal "
         "differences";

  // Alternative Level 4 validation: total = zero + trivial + insignificant +
  // marginal + non_marginal
  EXPECT_EQ(counter.elem_number, zero_differences + counter.diff_trivial +
                                     counter.diff_insignificant +
                                     counter.diff_marginal + non_marginal)
      << "LEVEL 4: total should equal zero + trivial + insignificant + "
         "marginal + non_marginal";

  // ========================================================================
  // LEVEL 5: non_marginal = critical + non_critical (based on critical
  // threshold)
  // ========================================================================
  EXPECT_EQ(non_marginal, counter.diff_critical + non_critical)
      << "LEVEL 5: non_marginal should equal critical + non_critical "
         "differences";

  // Alternative Level 5 validation: total = zero + trivial + insignificant +
  // marginal + critical + non_critical
  EXPECT_EQ(counter.elem_number, zero_differences + counter.diff_trivial +
                                     counter.diff_insignificant +
                                     counter.diff_marginal +
                                     counter.diff_critical + non_critical)
      << "LEVEL 5: total should equal zero + trivial + insignificant + "
         "marginal + critical + non_critical";

  // ========================================================================
  // LEVEL 6: non_critical = error + non_error (based on user threshold)
  // ========================================================================
  EXPECT_EQ(non_critical, counter.diff_error + counter.diff_non_error)
      << "LEVEL 6: non_critical should equal error + non_error differences";

  // Alternative Level 6 validation: total = zero + trivial + insignificant +
  // marginal + critical + error + non_error
  EXPECT_EQ(counter.elem_number,
            zero_differences + counter.diff_trivial +
                counter.diff_insignificant + counter.diff_marginal +
                counter.diff_critical + counter.diff_error +
                counter.diff_non_error)
      << "LEVEL 6: total should equal zero + trivial + insignificant + "
         "marginal + critical + error + non_error";

  std::cout << "\nHierarchy Breakdown:" << std::endl;
  std::cout << "  Non-marginal: " << non_marginal << " (significant - marginal)"
            << std::endl;
  std::cout << "  Non-critical: " << non_critical
            << " (non_marginal - critical)" << std::endl;
  std::cout << "  Mathematical check: " << counter.elem_number << " = "
            << zero_differences << " + " << counter.diff_trivial << " + "
            << counter.diff_insignificant << " + " << counter.diff_marginal
            << " + " << counter.diff_critical << " + " << counter.diff_error
            << " + " << counter.diff_non_error << std::endl;

  // Clean up test files
  std::remove("test_6level1.txt");
  std::remove("test_6level2.txt");
}

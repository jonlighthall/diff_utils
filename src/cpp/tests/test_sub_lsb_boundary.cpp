/**
 * @file test_sub_lsb_boundary.cpp
 * @brief Unit tests for sub-LSB (Least Significant Bit) boundary detection
 *
 * This test suite validates the critical edge case where the raw difference
 * between values is less than the minimum representable difference (LSB) at
 * the coarser precision level.
 *
 * EDGE CASE: 30.8 (1dp) vs 30.85 (2dp)
 * - Raw difference: 0.05
 * - LSB at 1dp: 0.1
 * - Half-LSB: 0.05
 * - Rounded difference: 0.1 (30.8 vs 30.9)
 *
 * CRITICAL QUESTION:
 * When user sets threshold=0 (maximum sensitivity), should 0.05 be considered
 * equivalent because it's smaller than the minimum representable difference?
 *
 * ANSWER: YES - for true precision-aware, cross-platform robust comparison.
 *
 * RATIONALE:
 * 1. Information Theory: File1's "30.8" implicitly represents [30.75, 30.85),
 *    which CONTAINS 30.85
 * 2. Cross-Platform: Different systems could produce this exact difference
 *    from identical calculations with different output formatting
 * 3. Scientific Validity: Sub-LSB differences are indistinguishable at the
 *    coarser precision
 *
 * @author J.C. Lighthall
 * @date October 2025
 */

#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <string>

#include "uband_diff.h"

/**
 * @brief Helper to create temporary test files
 * @param name Filename for the temporary file
 * @param contents File contents as a string
 * @return Absolute path to the created file
 */
static std::string write_temp(const std::string& name,
                              const std::string& contents) {
  std::string path = "/tmp/" + name;
  std::ofstream f(path);
  if (!f) {
    throw std::runtime_error("Failed to open temp file: " + path);
  }
  f << contents;
  f.close();
  return path;
}

/**
 * @brief Test the exact edge case: 30.8 vs 30.85 with threshold=0
 *
 * This is the canonical example of a sub-LSB difference that should be
 * considered trivial/equivalent for precision-aware comparison.
 */
TEST(SubLSBBoundaryTest, ExactHalfLSBDifferenceAtZeroThreshold) {
  // File 1: 1 decimal place precision
  std::string ref = "30.8\n";

  // File 2: 2 decimal places precision, value exactly 0.05 higher
  std::string test = "30.85\n";

  auto ref_path = write_temp("test_sublsb_ref.txt", ref);
  auto test_path = write_temp("test_sublsb_test.txt", test);

  // Create comparator with threshold=0 (maximum sensitivity)
  FileComparator cmp(/*user_thresh*/ 0.0, /*hard*/ 9999.0, /*print*/ 0.0,
                     /*debug*/ 0);

  bool result = cmp.compare_files(ref_path, test_path);

  const auto& stats = cmp.getCountStats();
  const auto& flags = cmp.getFlag();

  // CRITICAL ASSERTIONS
  // ===================

  // The raw difference exists and is non-zero
  EXPECT_EQ(stats.elem_number, 1) << "Should have checked 1 element";
  EXPECT_EQ(stats.diff_non_zero, 1) << "Raw difference (0.05) is non-zero";

  // The key assertion: this should be classified as TRIVIAL
  // because raw_diff (0.05) <= half-LSB (0.05)
  EXPECT_EQ(stats.diff_trivial, 1)
      << "Sub-LSB difference should be classified as TRIVIAL";
  EXPECT_EQ(stats.diff_non_trivial, 0)
      << "No non-trivial differences should exist";

  // Therefore, no significant differences
  EXPECT_EQ(stats.diff_significant, 0)
      << "Sub-LSB differences are never significant";

  // Files should be considered equivalent
  EXPECT_TRUE(flags.files_are_close_enough)
      << "Files with only sub-LSB differences should be 'close enough'";
  EXPECT_TRUE(result)
      << "Comparison should succeed (files are equivalent within precision)";

  // Clean up
  std::remove(ref_path.c_str());
  std::remove(test_path.c_str());
}

/**
 * @brief Test sub-LSB differences remain trivial at various precision levels
 */
TEST(SubLSBBoundaryTest, SubLSBAtMultiplePrecisionLevels) {
  struct TestCase {
    std::string value1;
    std::string value2;
    int precision1;
    int precision2;
    double raw_diff;
    std::string description;
  };

  std::vector<TestCase> cases = {
      {"10.5", "10.54", 1, 2, 0.04, "1dp: 0.04 < 0.05 (half-LSB)"},
      {"100.0", "100.04", 1, 2, 0.04, "Integer vs 2dp: 0.04 < 0.05"},
      {"3.140", "3.1404", 3, 4, 0.0004,
       "3dp vs 4dp: 0.0004 < 0.0005 (half-LSB)"},
      {"0.1", "0.105", 1, 3, 0.005, "1dp: 0.005 < 0.05 (half-LSB)"},
  };

  for (const auto& tc : cases) {
    auto ref_path = write_temp("test_sublsb_multi_ref.txt", tc.value1 + "\n");
    auto test_path = write_temp("test_sublsb_multi_test.txt", tc.value2 + "\n");

    FileComparator cmp(0.0, 9999.0, 0.0, 0);
    cmp.compare_files(ref_path, test_path);

    const auto& stats = cmp.getCountStats();

    EXPECT_EQ(stats.diff_trivial, 1) << "Failed for case: " << tc.description;
    EXPECT_EQ(stats.diff_non_trivial, 0)
        << "Failed for case: " << tc.description;

    std::remove(ref_path.c_str());
    std::remove(test_path.c_str());
  }
}

/**
 * @brief Test that supra-LSB differences (>= LSB) are still non-trivial
 */
TEST(SubLSBBoundaryTest, SupraLSBDifferencesAreNonTrivial) {
  // Difference of 0.1 at 1dp precision should be non-trivial
  std::string ref = "30.8\n";
  std::string test = "30.9\n";  // Exactly 1 LSB different

  auto ref_path = write_temp("test_supra_ref.txt", ref);
  auto test_path = write_temp("test_supra_test.txt", test);

  FileComparator cmp(0.0, 9999.0, 0.0, 0);
  cmp.compare_files(ref_path, test_path);

  const auto& stats = cmp.getCountStats();

  EXPECT_EQ(stats.diff_trivial, 0)
      << "Difference of 1 LSB should NOT be trivial";
  EXPECT_EQ(stats.diff_non_trivial, 1)
      << "Difference of 1 LSB should be non-trivial";
  EXPECT_EQ(stats.diff_significant, 1)
      << "With threshold=0, all non-trivial diffs are significant";

  std::remove(ref_path.c_str());
  std::remove(test_path.c_str());
}

/**
 * @brief Test multiple values with mixed sub-LSB and supra-LSB differences
 */
TEST(SubLSBBoundaryTest, MixedSubLSBAndSupraLSBDifferences) {
  std::string ref =
      "10.5\n"   // Line 1: sub-LSB (0.04 < 0.1)
      "20.8\n"   // Line 2: exactly half-LSB (0.05 == 0.05)
      "30.7\n";  // Line 3: supra-LSB (0.2 >= 0.1)

  std::string test =
      "10.54\n"
      "20.85\n"
      "30.9\n";

  auto ref_path = write_temp("test_mixed_ref.txt", ref);
  auto test_path = write_temp("test_mixed_test.txt", test);

  FileComparator cmp(0.0, 9999.0, 0.0, 0);
  cmp.compare_files(ref_path, test_path);

  const auto& stats = cmp.getCountStats();

  EXPECT_EQ(stats.elem_number, 3);
  EXPECT_EQ(stats.diff_non_zero, 3) << "All have non-zero raw differences";
  EXPECT_EQ(stats.diff_trivial, 2)
      << "Lines 1 and 2 should be trivial (sub-LSB)";
  EXPECT_EQ(stats.diff_non_trivial, 1)
      << "Line 3 should be non-trivial (supra-LSB)";
  EXPECT_EQ(stats.diff_significant, 1)
      << "Line 3 is significant at threshold=0";

  std::remove(ref_path.c_str());
  std::remove(test_path.c_str());
}

/**
 * @brief Test cross-platform scenario: same value, different formatting
 */
TEST(SubLSBBoundaryTest, CrossPlatformFormattingEquivalence) {
  // Simulate the scenario where two platforms calculate 30.849999...
  // but print it with different precision and rounding

  std::string platform1 = "30.8\n";   // Rounded to 1dp
  std::string platform2 = "30.85\n";  // Rounded to 2dp

  auto path1 = write_temp("test_platform1.txt", platform1);
  auto path2 = write_temp("test_platform2.txt", platform2);

  FileComparator cmp(0.0, 9999.0, 0.0, 0);
  bool result = cmp.compare_files(path1, path2);

  const auto& flags = cmp.getFlag();

  EXPECT_TRUE(result)
      << "Cross-platform formatting differences should not fail comparison";
  EXPECT_TRUE(flags.files_are_close_enough)
      << "Files should be considered equivalent despite formatting differences";

  std::remove(path1.c_str());
  std::remove(path2.c_str());
}

/**
 * @brief Test that the fix works correctly with non-zero user thresholds
 */
TEST(SubLSBBoundaryTest, SubLSBWithNonZeroThreshold) {
  std::string ref = "30.8\n";
  std::string test = "30.85\n";

  auto ref_path = write_temp("test_nonzero_ref.txt", ref);
  auto test_path = write_temp("test_nonzero_test.txt", test);

  // With a threshold of 0.05, the sub-LSB logic should still apply
  FileComparator cmp(/*user_thresh*/ 0.05, /*hard*/ 9999.0, /*print*/ 0.0,
                     /*debug*/ 0);
  cmp.compare_files(ref_path, test_path);

  const auto& stats = cmp.getCountStats();

  EXPECT_EQ(stats.diff_trivial, 1)
      << "Sub-LSB classification should be independent of user threshold";
  EXPECT_EQ(stats.diff_significant, 0)
      << "Trivial differences are never significant";

  std::remove(ref_path.c_str());
  std::remove(test_path.c_str());
}

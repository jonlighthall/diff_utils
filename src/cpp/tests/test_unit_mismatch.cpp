/**
 * @file test_unit_mismatch.cpp
 * @brief Unit tests for detecting unit mismatch (meters vs nautical miles) in
 * column 1
 *
 * @author J. Lighthall
 * @date October 2025
 */

#include <gtest/gtest.h>

#include <cmath>
#include <fstream>
#include <string>

#include "uband_diff.h"

/**
 * @brief Test detection of unit mismatch when column 1 is scaled by ~1852
 * (meters vs nautical miles)
 *
 * This test creates two identical files except that column 1 in file1 is in
 * meters while column 1 in file2 is in nautical miles (scaled by 1852).
 */
TEST(UnitMismatchTest, DetectMetersVsNauticalMiles) {
  // Create two temporary test files in the build/test directory
  std::string file1 = "../../build/test/test_unit_mismatch_meters.txt";
  std::string file2 = "../../build/test/test_unit_mismatch_nmi.txt";

  // Create file with meters in column 1
  std::ofstream out1(file1);
  ASSERT_TRUE(out1.is_open());
  out1 << "  1852.0  10.5  20.3  30.1\n";
  out1 << "  3704.0  11.2  21.5  31.8\n";
  out1 << "  5556.0  12.3  22.1  32.5\n";
  out1.close();

  // Create file with nautical miles in column 1 (1852 meters = 1 nmi)
  // All other columns identical
  std::ofstream out2(file2);
  ASSERT_TRUE(out2.is_open());
  out2 << "     1.0  10.5  20.3  30.1\n";
  out2 << "     2.0  11.2  21.5  31.8\n";
  out2 << "     3.0  12.3  22.1  32.5\n";
  out2.close();

  // Create comparator with moderate thresholds to avoid critical errors
  // We use a large user threshold and critical threshold to avoid triggering
  // errors due to the large column 1 differences
  FileComparator comparator(
      /*user_thresh*/ 10000.0,  // Large threshold to not fail on column 1
      /*critical*/ 100000.0,    // Very large critical threshold
      /*print*/ 0.0,            // Print all differences
      /*debug*/ 0);             // No debug output for cleaner test

  // Run the comparison
  comparator.compare_files(file1, file2);

  // Check that unit mismatch was detected
  const Flags& flags = comparator.getFlag();
  EXPECT_TRUE(flags.unit_mismatch)
      << "Unit mismatch should be detected for ~1852x scale factor";
  EXPECT_EQ(flags.unit_mismatch_line, 1)
      << "Unit mismatch should be detected on first line";

  // Check that the ratio is close to 1852
  EXPECT_NEAR(flags.unit_mismatch_ratio, 1852.0, 1852.0 * 0.01)
      << "Ratio should be approximately 1852 (within 1%)";

  // The other columns should match
  const CountStats& counter = comparator.getCountStats();
  // We have 3 lines × 4 columns = 12 elements total
  EXPECT_EQ(counter.elem_number, 12) << "Should have checked 12 elements";
  // Only column 1 differs (3 differences)
  EXPECT_EQ(counter.diff_non_zero, 3)
      << "Only column 1 (3 values) should differ";
}

/**
 * @brief Test that no unit mismatch is detected for identical files
 */
TEST(UnitMismatchTest, NoMismatchForIdenticalFiles) {
  std::string file1 = "../../build/test/test_unit_no_mismatch1.txt";
  std::string file2 = "../../build/test/test_unit_no_mismatch2.txt";

  // Create two identical files
  std::ofstream out1(file1);
  ASSERT_TRUE(out1.is_open());
  out1 << "  100.0  10.5  20.3  30.1\n";
  out1 << "  200.0  11.2  21.5  31.8\n";
  out1.close();

  std::ofstream out2(file2);
  ASSERT_TRUE(out2.is_open());
  out2 << "  100.0  10.5  20.3  30.1\n";
  out2 << "  200.0  11.2  21.5  31.8\n";
  out2.close();

  FileComparator comparator(0.05, 10.0, 1.0, 0);
  comparator.compare_files(file1, file2);

  const Flags& flags = comparator.getFlag();
  EXPECT_FALSE(flags.unit_mismatch)
      << "No unit mismatch should be detected for identical files";
}

/**
 * @brief Test that no unit mismatch is detected for different scale factors
 */
TEST(UnitMismatchTest, NoMismatchForOtherScaleFactors) {
  std::string file1 = "../../build/test/test_unit_other_scale1.txt";
  std::string file2 = "../../build/test/test_unit_other_scale2.txt";

  // Create files with a 2x scale factor (not 1852)
  std::ofstream out1(file1);
  ASSERT_TRUE(out1.is_open());
  out1 << "  100.0  10.5  20.3  30.1\n";
  out1 << "  200.0  11.2  21.5  31.8\n";
  out1.close();

  std::ofstream out2(file2);
  ASSERT_TRUE(out2.is_open());
  out2 << "   50.0  10.5  20.3  30.1\n";  // 2x scale, not 1852x
  out2 << "  100.0  11.2  21.5  31.8\n";
  out2.close();

  FileComparator comparator(10.0, 100.0, 0.0, 0);
  comparator.compare_files(file1, file2);

  const Flags& flags = comparator.getFlag();
  EXPECT_FALSE(flags.unit_mismatch)
      << "No unit mismatch should be detected for scale factor of 2";
}

/**
 * @brief Test detection when file2 has meters and file1 has nautical miles
 * (inverse ratio)
 */
TEST(UnitMismatchTest, DetectInverseRatio) {
  std::string file1 = "../../build/test/test_unit_inverse1.txt";
  std::string file2 = "../../build/test/test_unit_inverse2.txt";

  // Create file with nautical miles in column 1
  std::ofstream out1(file1);
  ASSERT_TRUE(out1.is_open());
  out1 << "     1.0  10.5  20.3  30.1\n";
  out1 << "     2.0  11.2  21.5  31.8\n";
  out1.close();

  // Create file with meters in column 1
  std::ofstream out2(file2);
  ASSERT_TRUE(out2.is_open());
  out2 << "  1852.0  10.5  20.3  30.1\n";
  out2 << "  3704.0  11.2  21.5  31.8\n";
  out2.close();

  FileComparator comparator(10000.0, 100000.0, 0.0, 0);
  comparator.compare_files(file1, file2);

  const Flags& flags = comparator.getFlag();
  EXPECT_TRUE(flags.unit_mismatch)
      << "Unit mismatch should be detected for inverse ratio";
  EXPECT_EQ(flags.unit_mismatch_line, 1);

  // The inverse ratio (v1/v2) will be ~1/1852, so we check v2/v1 instead
  double inverse = 1.0 / flags.unit_mismatch_ratio;
  EXPECT_NEAR(inverse, 1852.0, 1852.0 * 0.01)
      << "Inverse ratio should be approximately 1852";
}

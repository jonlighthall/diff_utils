#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "uband_diff.h"

// Simple tests to validate negative/percent 'significant' threshold behavior.
// These use the same test directory as other tests.

static constexpr const char* TEST_DIR = "../../build/test/";

// Helper to create a test file
static void write_file(const std::string& path, const std::string& line) {
  std::filesystem::create_directories(TEST_DIR);
  std::ofstream ofs(path);
  ofs << line << std::endl;
}

TEST(PercentThresholdTest, FractionalDifferenceAbovePercentIsSignificant) {
  // File2 is treated as reference. Use 1% (0.01) percent threshold.
  FileComparator comp(/*user_thresh*/ 0.0, /*critical*/ 10.0, /*print*/ 1.0, 0,
                      /*significant_is_percent*/ true,
                      /*significant_percent*/ 0.01);

  std::string f1 = std::string(TEST_DIR) + "percent_a1.txt";
  std::string f2 = std::string(TEST_DIR) + "percent_a2.txt";

  // Reference value = 100.0; test value differs by 1.5 -> 1.5% > 1% ->
  // significant
  write_file(f1, "101.5");
  write_file(f2, "100.0");

  bool ok = comp.compare_files(f1, f2);
  EXPECT_FALSE(ok);
  EXPECT_TRUE(comp.getFlag().has_significant_diff);
  EXPECT_FALSE(comp.getFlag().files_are_close_enough);

  std::remove(f1.c_str());
  std::remove(f2.c_str());
}

TEST(PercentThresholdTest, FractionalDifferenceBelowPercentIsNotSignificant) {
  FileComparator comp(/*user_thresh*/ 0.0, /*critical*/ 10.0, /*print*/ 1.0, 0,
                      /*significant_is_percent*/ true,
                      /*significant_percent*/ 0.01);

  std::string f1 = std::string(TEST_DIR) + "percent_b1.txt";
  std::string f2 = std::string(TEST_DIR) + "percent_b2.txt";

  // Reference = 100.0; diff = 0.5 -> 0.5% <= 1% -> not significant
  write_file(f1, "100.5");
  write_file(f2, "100.0");

  bool ok = comp.compare_files(f1, f2);
  EXPECT_TRUE(ok);
  EXPECT_FALSE(comp.getFlag().has_significant_diff);
  EXPECT_TRUE(comp.getFlag().files_are_close_enough);

  std::remove(f1.c_str());
  std::remove(f2.c_str());
}

TEST(PercentThresholdTest, NearZeroReferenceTreatsNonTrivialAsSignificant) {
  // When reference value is near-zero (<= thresh.zero), any non-trivial
  // difference should be treated as exceeding percent threshold.
  FileComparator comp(/*user_thresh*/ 0.0, /*critical*/ 10.0, /*print*/ 1.0, 0,
                      /*significant_is_percent*/ true,
                      /*significant_percent*/ 0.01);

  std::string f1 = std::string(TEST_DIR) + "percent_c1.txt";
  std::string f2 = std::string(TEST_DIR) + "percent_c2.txt";

  // Reference value = 0.0 (<= zero). Test value 0.5 should count as significant
  write_file(f1, "0.5");
  write_file(f2, "0.0");

  bool ok = comp.compare_files(f1, f2);
  EXPECT_FALSE(ok);
  EXPECT_TRUE(comp.getFlag().has_significant_diff);

  std::remove(f1.c_str());
  std::remove(f2.c_str());
}

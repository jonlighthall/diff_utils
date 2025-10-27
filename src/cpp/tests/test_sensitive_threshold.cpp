#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "uband_diff.h"

// This test validates the special behavior when the user-specified
// significant threshold is zero. At zero user threshold the implementation
// is "sensitive": any non-trivial difference where both values are not
// simultaneously above the ignore threshold should be classified as
// significant. In other words, if at least one of the two values is within
// the operational range of interest (<= ignore), then a non-trivial
// difference is significant when user threshold == 0.
//
// For mathematical rigor we craft a small, deterministic dataset that
// exercises the relevant classification branches and makes expectations
// explicit. This keeps the test self-contained and avoids relying on
// external canonical files.

static constexpr const char* TEST_DIR = "/tmp/diff_utils_test_sensitive/";

static void write_lines(const std::string& path,
                        const std::vector<std::string>& lines) {
  std::filesystem::create_directories(TEST_DIR);
  std::ofstream ofs(path);
  for (const auto& l : lines) ofs << l << '\n';
}

TEST(SensitiveThresholdTest,
     CanonicalZeroThresholdClassification_SelfContained) {
  // Build two tiny files with values chosen to exercise classification.
  // Lines (index -> ref, test):
  // 1: 0.0     , 0.0     -> zero (no diff)
  // 2: 50.0    , 52.0    -> non-trivial, operational (<= ignore) -> significant
  // 3: 115.0   , 117.0   -> non-trivial, marginal (<= marginal) -> marginal
  // 4: 150.0   , 160.0   -> non-trivial, both > ignore -> insignificant
  // 5: 0.0     , 0.5     -> near-zero reference -> treated as significant
  // 6: 200.0   , 250.0   -> both > ignore -> insignificant

  const std::string f_ref = std::string(TEST_DIR) + "sensitive_ref.txt";
  const std::string f_test = std::string(TEST_DIR) + "sensitive_test.txt";

  // Each line: <range> <value>. Keep range identical between files so only the
  // second column contributes meaningful differences. This also mirrors the
  // multi-column format used elsewhere in the test-suite.
  write_lines(f_ref, {"100.0 0.0", "101.0 50.0", "102.0 115.0", "103.0 150.0",
                      "104.0 0.0", "105.0 200.0"});
  write_lines(f_test, {"100.0 0.0", "101.0 52.0", "102.0 117.0", "103.0 160.0",
                       "104.0 0.5", "105.0 250.0"});

  // Use user significant threshold = 0.0 (sensitive mode), choose a high
  // critical threshold so no differences are classified as critical in this
  // small dataset. print_thresh=0.0, debug=0.
  FileComparator comparator(/*user_thresh*/ 0.0, /*hard_thresh*/ 1000.0,
                            /*print_thresh*/ 0.0, /*debug_level*/ 0);

  // Sanity checks: files must exist and contain expected number of lines.
  EXPECT_TRUE(std::filesystem::exists(f_ref))
      << "Ref file not created: " << f_ref;
  EXPECT_TRUE(std::filesystem::exists(f_test))
      << "Test file not created: " << f_test;
  {
    std::ifstream ifs(f_ref);
    std::string l;
    int cnt = 0;
    while (std::getline(ifs, l)) ++cnt;
    EXPECT_EQ(cnt, 6) << "Ref file should have 6 lines";
  }
  {
    std::ifstream ifs(f_test);
    std::string l;
    int cnt = 0;
    while (std::getline(ifs, l)) ++cnt;
    EXPECT_EQ(cnt, 6) << "Test file should have 6 lines";
  }

  bool result = comparator.compare_files(f_ref, f_test);

  const auto& counters = comparator.getCountStats();
  const auto& flags = comparator.getFlag();

  // Expectations based on the crafted dataset above

  // We have two columns per line. The first column (range) is identical and
  // therefore contributes zero differences. The second column contains the
  // test differences described above.
  // Therefore: elem_number = 6 lines * 2 columns = 12 elements
  EXPECT_EQ(counters.elem_number, 12u) << "Should have processed 12 elements";
  EXPECT_EQ(counters.diff_non_trivial, 5u)
      << "Five non-zero differences expected (measurement column only)";

  // Significant differences: measurement columns on lines 2,3,5 -> 3
  EXPECT_EQ(counters.diff_significant, 3u)
      << "Three significant differences expected";

  // Insignificant: measurement columns on lines 4 and 6 -> 2
  EXPECT_EQ(counters.diff_insignificant, 2u)
      << "Two insignificant differences expected";

  // Marginal: line 3 only
  EXPECT_EQ(counters.diff_marginal, 1u) << "One marginal difference expected";

  // No critical differences in this dataset
  EXPECT_EQ(counters.diff_critical, 0u) << "No critical differences expected";

  // Non-marginal, non-critical significant = significant - marginal - critical
  size_t non_marginal_non_critical = counters.diff_significant -
                                     counters.diff_marginal -
                                     counters.diff_critical;
  EXPECT_EQ(non_marginal_non_critical, 2u)
      << "Two non-marginal non-critical significant expected";

  EXPECT_TRUE(flags.has_significant_diff);
  EXPECT_TRUE(flags.has_non_trivial_diff);
  EXPECT_FALSE(flags.has_critical_diff);

  // Cleanup
  std::remove(f_ref.c_str());
  std::remove(f_test.c_str());
}

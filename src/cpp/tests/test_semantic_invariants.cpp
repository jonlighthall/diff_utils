#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "uband_diff.h"

// Directory for temporary test files
static constexpr const char* TEST_DIR = "../../build/test/";

// Helper to create a temporary test file with provided lines.
static void writeFile(const std::string& filename,
                      const std::vector<std::string>& lines) {
  std::filesystem::create_directories(TEST_DIR);
  std::string path = std::string(TEST_DIR) + filename;
  std::ofstream f(path);
  for (const auto& l : lines) {
    f << l << "\n";
  }
}

// Helper to get full path for test file
static std::string getTestFilePath(const std::string& filename) {
  return std::string(TEST_DIR) + filename;
}

// 1. Zero-threshold semantic invariant:
//    When user significant threshold == 0, all non-trivial differences whose
//    values are not BOTH above ignore must be significant.
//    Invariant: diff_significant + diff_high_ignore == diff_non_trivial.
TEST(SemanticInvariants, ZeroThresholdInvariant) {
  FileComparator cmp(0.0, /*critical*/ 1.0, /*print*/ 0.0, /*debug*/ 0);
  bool ok = cmp.compare_files("../../data/pe.std1.pe01.ref.txt",
                              "../../data/pe.std1.pe01.test.txt");
  (void)ok;  // outcome not central to invariant
  const auto& c = cmp.getCountStats();
  EXPECT_EQ(c.diff_significant + c.diff_high_ignore, c.diff_non_trivial)
      << "Zero-threshold semantic invariant failed: significant + high_ignore "
         "!= non_trivial";
}

// 2. High ignore band isolation:
//    All values > ignore threshold; expect non-trivial but zero significant.
TEST(SemanticInvariants, HighIgnoreIsolation) {
  // Build two single-line files with values all > ~138.47
  writeFile("test_ignore1.txt", {"150.0 160.0 170.0"});
  writeFile("test_ignore2.txt", {"151.0 161.5 175.0"});
  FileComparator cmp(0.05, 2.0, 0.0, 0);
  cmp.compare_files(getTestFilePath("test_ignore1.txt"),
                    getTestFilePath("test_ignore2.txt"));
  const auto& c = cmp.getCountStats();
  EXPECT_GT(c.diff_non_trivial, 0u);
  EXPECT_EQ(c.diff_significant, 0u);
  EXPECT_EQ(c.diff_high_ignore, c.diff_non_trivial)
      << "All non-trivial diffs should be counted as high-ignore insignificant";
}

// 3. Critical suppression behavior:
//    First difference non-critical & printable, second difference critical;
//    table should stop after first line's row.
TEST(SemanticInvariants, CriticalSuppressionStopsPrinting) {
  writeFile("test_crit_sup1.txt", {"0.0", "0.0"});
  writeFile("test_crit_sup2.txt",
            {"0.5", "2.0"});  // second line is critical (>1.0)
  FileComparator cmp(0.1, 1.0, 0.0, 0);
  cmp.compare_files(getTestFilePath("test_crit_sup1.txt"),
                    getTestFilePath("test_crit_sup2.txt"));
  const auto& c = cmp.getCountStats();
  const auto& f = cmp.getFlag();
  EXPECT_TRUE(f.has_critical_diff);
  // Two elements compared, both non-zero & non-trivial => two significant diffs
  // expected.
  EXPECT_EQ(c.diff_significant, 2u);
  // Only first (non-critical) printed; second suppressed after critical flag
  // set.
  EXPECT_EQ(c.diff_print, 1u)
      << "Printing should stop after first critical encountered";
}

// 4. Print threshold decoupling:
//    Differences significant but below print threshold should not increment
//    diff_print.
TEST(SemanticInvariants, PrintThresholdDecouplesCounting) {
  writeFile("test_print_decouple1.txt", {"0.000 0.000 0.000"});
  writeFile("test_print_decouple2.txt",
            {"0.200 0.150 0.120"});  // all diffs > user(0.05)
  FileComparator cmp(0.05, 5.0, /*print threshold*/ 1.0,
                     0);  // print threshold higher than diffs
  cmp.compare_files(getTestFilePath("test_print_decouple1.txt"),
                    getTestFilePath("test_print_decouple2.txt"));
  const auto& c = cmp.getCountStats();
  EXPECT_GT(c.diff_significant, 0u);
  EXPECT_EQ(c.diff_print, 0u)
      << "No rows should print when below print threshold";
}

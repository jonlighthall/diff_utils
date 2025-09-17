#include <gtest/gtest.h>
// Public interface header for the comparison engine
#include "uband_diff.h"

// Utility: create two temporary in-memory files via actual disk writes for current comparator flow
static std::string write_temp(const std::string &name, const std::string &contents) {
    std::string path = name;
    FILE *f = fopen(path.c_str(), "w");
    if(!f) throw std::runtime_error("Failed to open temp file: " + path);
    fwrite(contents.data(), 1, contents.size(), f);
    fclose(f);
    return path;
}

// We craft lines where raw differences exist but rounding to the shared precision zeroes them out.
// Example: values differ in the 3rd decimal place but both have only 2 decimals -> rounded diff == 0 -> trivial.
// We then run with user_significant=0.0 (max sensitivity) and with a small positive threshold to verify
// trivial counts are identical and never migrate into significant/insignificant splits.
TEST(TrivialExclusionTest, TrivialDifferencesRemainExcludedAtZeroThreshold) {
    // Each line: col1 values differ in 3rd decimal place (0.001), printed precision intentionally limited.
    // Format ensures both sides present 2 decimal places so rounding eliminates the difference.
    std::string ref =
        "10.12 20.45 30.78\n"
        "11.34 21.56 31.67\n"
        "12.90 22.10 32.55\n"; // 12.90 vs 12.901 would have been non-trivial if precision higher
    std::string test =
        "10.121 20.451 30.781\n"  // all +0.001
        "11.341 21.561 31.671\n"
        "12.901 22.101 32.551\n";

    auto ref_path = write_temp("temp_trivial_ref.txt", ref);
    auto test_path = write_temp("temp_trivial_test.txt", test);

    // Thresholds: significant=0.0 triggers sensitive semantics; critical large; print small to attempt emission.
    // Construct comparators directly using public ctor signature used elsewhere in tests
    FileComparator cmp_sensitive(/*user_thresh*/0.0, /*hard*/9999.0, /*print*/0.0, /*debug*/0);
    FileComparator cmp_normal(/*user_thresh*/0.05, /*hard*/9999.0, /*print*/0.0, /*debug*/0);

    (void)cmp_sensitive.compare_files(ref_path, test_path);
    (void)cmp_normal.compare_files(ref_path, test_path);

    const auto& stats_sensitive = cmp_sensitive.getCountStats();
    const auto& stats_normal = cmp_normal.getCountStats();

    // Basic sanity: all raw differences are *non-zero* but should collapse to trivial after rounding.
    // Therefore diff_trivial should equal number of elements and diff_non_trivial = 0 in both modes.
    int total_elements = stats_sensitive.elem_number; // assume same in both
    EXPECT_EQ(stats_sensitive.elem_number, stats_normal.elem_number);
    EXPECT_GT(total_elements, 0);

    EXPECT_EQ(stats_sensitive.diff_non_trivial, 0) << "Sensitive mode incorrectly treated formatting noise as non-trivial";
    EXPECT_EQ(stats_normal.diff_non_trivial, 0) << "Normal mode incorrectly treated formatting noise as non-trivial";

    EXPECT_EQ(stats_sensitive.diff_trivial, (size_t)total_elements);
    EXPECT_EQ(stats_normal.diff_trivial, (size_t)total_elements);

    // No significant or insignificant counts should appear (they partition only non_trivial subset)
    EXPECT_EQ(stats_sensitive.diff_significant, 0);
    EXPECT_EQ(stats_sensitive.diff_insignificant, 0);
    EXPECT_EQ(stats_normal.diff_significant, 0);
    EXPECT_EQ(stats_normal.diff_insignificant, 0);

    // Switching to zero threshold must not change trivial classification outcome.
    EXPECT_EQ(stats_sensitive.diff_trivial, stats_normal.diff_trivial);
}

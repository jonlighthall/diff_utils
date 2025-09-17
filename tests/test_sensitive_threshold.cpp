#include <gtest/gtest.h>

#include <string>

#include "uband_diff.h"

// This test validates the special behavior when the user-specified
// significant threshold is zero: every non-trivial difference whose values
// are not BOTH above the ignore threshold should be classified as significant.
// Canonical dataset: example_data/pe.std1.pe01.ref.txt vs test.txt
// Expected (empirically validated):
//   diff_non_trivial = 39
//   diff_significant = 27
//   diff_insignificant = 12
//   diff_marginal = 24
//   non-marginal, non-critical significant = 3
//   diff_critical = 0
// NOTE: We are asserting the public counters reflect this distribution.

TEST(SensitiveThresholdTest, CanonicalZeroThresholdClassification) {
  const std::string ref_file = "example_data/pe.std1.pe01.ref.txt";
  const std::string test_file = "example_data/pe.std1.pe01.test.txt";

  // significant=0.0, critical=1.0, print=0.0, debug level 0
  FileComparator comparator(/*user_thresh*/ 0.0, /*hard_thresh*/ 1.0,
                            /*print_thresh*/ 0.0, /*debug_level*/ 0);
  bool result = comparator.compare_files(ref_file, test_file);

  const auto& counters = comparator.getCountStats();
  const auto& flags = comparator.getFlag();

  // Core expectations
  EXPECT_EQ(counters.diff_non_trivial, 39u) << "Unexpected non-trivial count";
  EXPECT_EQ(counters.diff_significant, 27u)
      << "Significant count mismatch (should treat all meaningful non-trivial "
         "diffs as significant)";
  EXPECT_EQ(counters.diff_insignificant, 12u) << "Insignificant count mismatch";
  EXPECT_EQ(counters.diff_marginal, 24u) << "Marginal count mismatch";
  // Non-marginal significant = significant - marginal - critical
  size_t non_marginal_non_critical = counters.diff_significant -
                                     counters.diff_marginal -
                                     counters.diff_critical;
  EXPECT_EQ(non_marginal_non_critical, 3u)
      << "Non-marginal non-critical significant mismatch";
  EXPECT_EQ(counters.diff_critical, 0u)
      << "Critical count should be zero for this dataset";

  // Flag sanity
  EXPECT_TRUE(flags.has_significant_diff);
  EXPECT_TRUE(flags.has_non_trivial_diff);
  EXPECT_FALSE(flags.has_critical_diff);
}
